#include "dsb/fmi/importer.hpp"

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <new>
#include <sstream>

#include "boost/algorithm/string/trim.hpp"
#include "boost/filesystem.hpp"
#include "boost/property_tree/ptree.hpp"
#include "boost/property_tree/xml_parser.hpp"

#include "fmilib.h"

#include "dsb/error.hpp"
#include "dsb/fmi/fmu1.hpp"
#include "dsb/log.hpp"
#include "dsb/util.hpp"
#include "dsb/util/zip.hpp"


namespace dsb
{
namespace fmi
{


std::shared_ptr<Importer> Importer::Create(
    const boost::filesystem::path& cachePath)
{
    return std::shared_ptr<Importer>(new Importer(cachePath));
}


std::shared_ptr<Importer> Importer::Create()
{
    return std::shared_ptr<Importer>(new Importer(dsb::util::TempDir()));
}


namespace
{
    dsb::log::Level ConvertLogLevel(jm_log_level_enu_t jmLogLevel)
    {
        switch (jmLogLevel)
        {
        case jm_log_level_fatal:
        case jm_log_level_error:
            return dsb::log::error;
        case jm_log_level_warning:
            return dsb::log::warning;
        case jm_log_level_info:
            return dsb::log::info;
        case jm_log_level_verbose:
        case jm_log_level_debug:
        case jm_log_level_nothing:
        case jm_log_level_all:
        default:
            // The last two cases + default should never match, and if
            // they do, we at least make sure a message is printed in
            // debug mode.
            return dsb::log::debug;
        }
    }

    void LoggerCallback(
        jm_callbacks* callbacks,
        jm_string module,
        jm_log_level_enu_t logLevel,
        jm_string message)
    {
        const auto myLevel = ConvertLogLevel(logLevel);
        // Errors are dealt with with exceptions
        if (myLevel < dsb::log::error) {
            dsb::log::Log(
                myLevel,
                boost::format("[FMI Library: %s] %s") % module % message);
        }
    }

    std::unique_ptr<jm_callbacks> MakeCallbacks()
    {
        auto c = std::make_unique<jm_callbacks>();
        std::memset(c.get(), 0, sizeof(jm_callbacks));
        c->malloc = std::malloc;
        c->calloc = std::calloc;
        c->realloc = std::realloc;
        c->free = std::free;
        c->logger = &LoggerCallback;
        c->log_level = jm_log_level_all;
        c->context = nullptr;
        std::memset(c->errMessageBuffer, 0, JM_MAX_ERROR_MESSAGE_SIZE);
        return c;
    }
}


Importer::Importer(const boost::filesystem::path& cachePath)
    : m_callbacks{MakeCallbacks()}
    , m_handle{fmi_import_allocate_context(m_callbacks.get()), &fmi_import_free_context}
    , m_fmuDir{cachePath / "fmu"}
    , m_workDir{cachePath / "tmp"}
{
    if (m_handle == nullptr) throw std::bad_alloc();
}


Importer::Importer(dsb::util::TempDir tempDir)
    : Importer{tempDir.Path()}
{
    m_tempCacheDir = std::make_unique<dsb::util::TempDir>(std::move(tempDir));
}


namespace
{
    struct MinimalModelDescription
    {
        FMIVersion fmiVersion;
        std::string guid;
    };

    // Reads the 'fmiVersion' and 'guid' attributes from the XML file.
    MinimalModelDescription PeekModelDescription(
        const boost::filesystem::path& fmuUnpackDir)
    {
        const auto xmlFile = fmuUnpackDir / "modelDescription.xml";
        boost::property_tree::ptree xml;
        boost::property_tree::read_xml(xmlFile.string(), xml);

        MinimalModelDescription md;

        auto fmiVersion = xml.get(
            "fmiModelDescription.<xmlattr>.fmiVersion",
            std::string());
        boost::trim(fmiVersion);
        if (fmiVersion.empty()) {
            throw std::runtime_error(
                "Invalid modelDescription.xml; fmiVersion attribute missing or empty");
        }
        if (fmiVersion.size() >= 3 && fmiVersion.substr(0, 3) == "1.0") {
            md.fmiVersion = FMIVersion::v1_0;
        } else if (fmiVersion.size() >= 3 && fmiVersion.substr(0, 3) == "2.0") {
            md.fmiVersion = FMIVersion::v2_0;
        } else {
            md.fmiVersion = FMIVersion::unknown;
        }

        if (md.fmiVersion != FMIVersion::unknown) {
            md.guid = xml.get(
                "fmiModelDescription.<xmlattr>.guid",
                std::string());
            boost::trim(md.guid);
            if (md.guid.empty()) {
                throw std::runtime_error(
                    "Invalid modelDescription.xml; guid attribute missing or empty");
            }
        }
        return md;
    }

    // Replaces all characters which are not printable ASCII characters or
    // not valid for use in a path with their percent-encoded equivalents.
    // References:
    //     https://en.wikipedia.org/wiki/Percent-encoding
    //     https://msdn.microsoft.com/en-us/library/aa365247.aspx
    std::string SanitisePath(const std::string& str)
    {
        DSB_INPUT_CHECK(!str.empty());
        std::ostringstream sanitised;
        sanitised.fill('0');
        sanitised << std::hex;
        for (const char c : str) {
            if (c < 0x20 || c > 0x7E
#ifdef _WIN32
                || c == '<'  || c == '>' || c == ':' || c == '"' || c == '/'
                || c == '\\' || c == '|' || c == '?' || c == '*'
#endif
            ) {
                sanitised
                    << '%'
                    << std::setw(2)
                    << static_cast<int>(static_cast<unsigned char>(c));
            } else {
                sanitised << c;
            }
        }
        return sanitised.str();
    }
}


std::shared_ptr<FMU> Importer::Import(const boost::filesystem::path& fmuPath)
{
    PrunePtrCaches();
    auto pit = m_pathCache.find(fmuPath);
    if (pit != end(m_pathCache)) return pit->second.lock();

    const auto zip = dsb::util::zip::Archive(fmuPath);
    const auto tempMdDir = m_workDir / dsb::util::RandomUUID();
    boost::filesystem::create_directories(tempMdDir);
    const auto removeTempMdDir = dsb::util::OnScopeExit([&](){
        boost::system::error_code ignored;
        boost::filesystem::remove_all(tempMdDir, ignored);
    });

    const auto modelDescriptionIndex = zip.FindEntry("modelDescription.xml");
    if (modelDescriptionIndex == dsb::util::zip::INVALID_ENTRY_INDEX) {
        throw std::runtime_error(
            fmuPath.string() + " does not contain modelDescription.xml");
    }
    zip.ExtractFileTo(modelDescriptionIndex, tempMdDir);

    const auto minModelDesc = PeekModelDescription(tempMdDir);
    if (minModelDesc.fmiVersion != FMIVersion::v1_0) {
        throw std::runtime_error(
            "Unsupported FMI version for FMU '" + fmuPath.string() + "'");
    }
    auto git = m_guidCache.find(minModelDesc.guid);
    if (git != end(m_guidCache)) return git->second.lock();

    const auto fmuUnpackDir = m_fmuDir / SanitisePath(minModelDesc.guid);
    if (!boost::filesystem::exists(fmuUnpackDir) ||
        !boost::filesystem::exists(fmuUnpackDir / "modelDescription.xml") ||
        boost::filesystem::last_write_time(fmuPath) > boost::filesystem::last_write_time(fmuUnpackDir / "modelDescription.xml"))
    {
        boost::filesystem::create_directories(fmuUnpackDir);
        try {
            zip.ExtractAll(fmuUnpackDir);
        } catch (...) {
            boost::system::error_code ignoreErrors;
            boost::filesystem::remove_all(fmuUnpackDir, ignoreErrors);
            throw;
        }
    }

    auto fmu = std::shared_ptr<FMU>(new FMU1(shared_from_this(), fmuUnpackDir));
    m_pathCache[fmuPath] = fmu;
    m_guidCache[minModelDesc.guid] = fmu;
    return fmu;
}


void Importer::CleanCache()
{
    // Remove unused FMUs
    if (boost::filesystem::exists(m_fmuDir)) {
        boost::system::error_code ignoredError;
        for (auto it = boost::filesystem::directory_iterator(m_fmuDir);
             it != boost::filesystem::directory_iterator();
             ++it)
        {
            if (m_guidCache.count(it->path().filename().string()) == 0) {
                boost::filesystem::remove_all(*it, ignoredError);
            }
        }
        if (boost::filesystem::is_empty(m_fmuDir)) {
            boost::filesystem::remove(m_fmuDir, ignoredError);
        }
    }

    // Delete the temp-files directory
    boost::system::error_code ec;
    boost::filesystem::remove_all(m_workDir, ec);
}


std::string Importer::LastErrorMessage()
{
    return std::string(jm_get_last_error(m_callbacks.get()));
}


fmi_import_context_t* Importer::FmilibHandle() const
{
    return m_handle.get();
}


void Importer::PrunePtrCaches()
{
    for (auto it = begin(m_pathCache); it != end(m_pathCache);) {
        if (it->second.expired()) m_pathCache.erase(it++);
        else ++it;
    }
    for (auto it = begin(m_guidCache); it != end(m_guidCache);) {
        if (it->second.expired()) m_guidCache.erase(it++);
        else ++it;
    }
}


}} // namespace
