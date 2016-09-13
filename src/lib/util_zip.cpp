#include "coral/util/zip.hpp"

#include <cassert>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <system_error>
#include <vector>

#include "zip.h"

#include "boost/filesystem.hpp"
#include "coral/error.hpp"


namespace coral
{
namespace util
{
namespace zip
{


namespace
{
    // A simple RAII class that manages a zip_file*.
    class ZipFile
    {
    public:
        ZipFile(::zip* archive, zip_uint64_t index, zip_flags_t flags = 0)
            : m_file{zip_fopen_index(archive, index, flags)}
        {
            if (m_file == nullptr) {
                throw Exception(archive);
            }
        }

        // Disabled because we don't need them (for now):
        ZipFile(const ZipFile&) = delete;
        ZipFile& operator=(const ZipFile&) = delete;
        ZipFile(ZipFile&&) = delete;
        ZipFile& operator=(ZipFile&&) = delete;

        ~ZipFile() CORAL_NOEXCEPT
        {
            if (m_file) zip_fclose(m_file);
        }

        std::size_t Read(void* buffer, std::size_t maxBytes)
        {
            assert(m_file != nullptr);
            assert(buffer != nullptr);
            assert(maxBytes > 0);
            const auto bytesRead = zip_fread(m_file, buffer, maxBytes);
            if (bytesRead < 0) {
                throw Exception(m_file);
            }
            return static_cast<std::size_t>(bytesRead);
        }

    private:
        zip_file* m_file;
    };
}


Archive::Archive() CORAL_NOEXCEPT
    : m_archive{nullptr}
{
}


Archive::Archive(const boost::filesystem::path& path)
    : m_archive{nullptr}
{
    Open(path);
}


Archive::Archive(Archive&& other) CORAL_NOEXCEPT
    : m_archive{other.m_archive}
{
    other.m_archive = nullptr;
}


Archive& Archive::operator=(Archive&& other) CORAL_NOEXCEPT
{
    Discard();
    m_archive = other.m_archive;
    other.m_archive = nullptr;
    return *this;
}


Archive::~Archive() CORAL_NOEXCEPT
{
    Discard();
}


void Archive::Open(const boost::filesystem::path& path)
{
    CORAL_PRECONDITION_CHECK(!IsOpen());
    int errorCode;
    auto archive = zip_open(path.string().c_str(), 0, &errorCode);
    if (!archive) {
        const auto errnoVal = errorCode == ZIP_ER_READ ? errno : 0;
        auto msgBuf = std::vector<char>(
            zip_error_to_str(nullptr, 0, errorCode, errnoVal) + 1);
        zip_error_to_str(msgBuf.data(), msgBuf.size(), errorCode, errno);
        throw Exception(msgBuf.data());
    }
    m_archive = archive;
}


/*
The reason this function is not called "Close" is that libzip has a separate
zip_close() function which saves changes to the archive, whereas zip_discard()
does not save changes.  Since this module currently only supports non-modifying
operations, there would be no practical difference, but this way we keep the
door open for adding this functionality in the future.
*/
void Archive::Discard() CORAL_NOEXCEPT
{
    if (m_archive) {
        zip_discard(m_archive);
        m_archive = nullptr;
    }
}


bool Archive::IsOpen() const CORAL_NOEXCEPT
{
    return m_archive != nullptr;
}


std::uint64_t Archive::EntryCount() const
{
    CORAL_PRECONDITION_CHECK(IsOpen());
    return zip_get_num_entries(m_archive, 0);
}


EntryIndex Archive::FindEntry(const std::string& name) const
{
    CORAL_PRECONDITION_CHECK(IsOpen());
    const auto n = zip_name_locate(m_archive, name.c_str(), ZIP_FL_ENC_GUESS);
    if (n < 0) {
        int code = 0;
        zip_error_get(m_archive, &code, nullptr);
        if (code == ZIP_ER_NOENT) return INVALID_ENTRY_INDEX;
        else throw Exception(m_archive);
    }
    return static_cast<EntryIndex>(n);
}


std::string Archive::EntryName(EntryIndex index) const
{
    CORAL_PRECONDITION_CHECK(IsOpen());
    const auto name = zip_get_name(m_archive, index, ZIP_FL_ENC_GUESS);
    if (name == nullptr) {
        throw Exception(m_archive);
    }
    return std::string(name);
}


bool Archive::IsDirEntry(EntryIndex index) const
{
    CORAL_PRECONDITION_CHECK(IsOpen());
    struct zip_stat zs;
    if (zip_stat_index(m_archive, index, 0, &zs)) {
        throw Exception(m_archive);
    }
    if ((zs.valid & ZIP_STAT_NAME) && (zs.valid & ZIP_STAT_SIZE) && (zs.valid & ZIP_STAT_CRC)) {
        const auto nameLen = std::strlen(zs.name);
        return zs.name[nameLen-1] == '/' && zs.size == 0 && zs.crc == 0;
    } else {
        throw Exception("Cannot determine entry type");
    }
}


namespace
{
    void Copy(
        ZipFile& source,
        std::ostream& target,
        std::vector<char>& buffer)
    {
        assert(target.good());
        assert(!buffer.empty());
        for (;;) {
            const auto n = source.Read(buffer.data(), buffer.size());
            if (n == 0) break;
            target.write(buffer.data(), n);
        }
    }

    void ExtractFileAs(
        ::zip* archive,
        EntryIndex index,
        const boost::filesystem::path& targetPath,
        std::vector<char>& buffer)
    {
        assert(archive != nullptr);
        assert(index >= 0);
        assert(!targetPath.empty());
        assert(!buffer.empty());

        ZipFile srcFile(archive, index, 0);
        std::ofstream tgtFile(
            targetPath.string(),
            std::ios_base::binary | std::ios_base::trunc);
        if (!tgtFile.is_open()) {
            const int e = errno;
            throw std::runtime_error(coral::error::ErrnoMessage(
                "Error opening file \"" + targetPath.string() + "\" for writing",
                e));
        }
        Copy(srcFile, tgtFile, buffer);
        if (tgtFile.fail()) {
            throw std::runtime_error(
                "An I/O error occurred during extraction of \""
                + targetPath.string() + '"');
        }
    }
}


void Archive::ExtractAll(
    const boost::filesystem::path& targetDir) const
{
    CORAL_PRECONDITION_CHECK(IsOpen());
    if (!boost::filesystem::exists(targetDir) ||
        !boost::filesystem::is_directory(targetDir))
    {
        throw std::ios_base::failure("Not a directory: " + targetDir.string());
    }

    auto buffer = std::vector<char>(4096*16);
    const auto entryCount = EntryCount();
    for (EntryIndex index = 0; index < entryCount; ++index) {
        const auto entryName = EntryName(index);
        if (!entryName.empty() && entryName.back() != '/') {
            const auto entryPath = boost::filesystem::path(entryName);
            if (entryPath.has_root_path()) {
                throw Exception(
                    "Archive contains an entry with an absolute path: "
                    + entryName);
            }
            const auto targetPath = targetDir / entryPath;
            boost::filesystem::create_directories(targetPath.parent_path());
            ExtractFileAs(m_archive, index, targetPath, buffer);
        }
    }
}


boost::filesystem::path Archive::ExtractFileTo(
    EntryIndex index,
    const boost::filesystem::path& targetDir) const
{
    CORAL_PRECONDITION_CHECK(IsOpen());
    const auto entryPath = boost::filesystem::path(EntryName(index));
    const auto targetPath = targetDir / entryPath.filename();
    auto buffer = std::vector<char>(4096*16);
    ExtractFileAs(m_archive, index, targetPath, buffer);
    return targetPath;
}


// =============================================================================
// Exception
// =============================================================================

Exception::Exception(const std::string& msg) CORAL_NOEXCEPT
    : std::runtime_error{msg}
{
}

Exception::Exception(::zip* archive) CORAL_NOEXCEPT
    : std::runtime_error{zip_strerror(archive)}
{
}

Exception::Exception(zip_file* file) CORAL_NOEXCEPT
    : std::runtime_error{zip_file_strerror(file)}
{
}


}}} // namespace
