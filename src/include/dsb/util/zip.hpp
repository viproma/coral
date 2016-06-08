/**
\file
\brief  Utilities for dealing with ZIP archives.
*/
#ifndef DSB_UTIL_ZIP_HPP
#define DSB_UTIL_ZIP_HPP

#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>

#include "boost/filesystem/path.hpp"
#include "boost/optional.hpp"

#include "dsb/config.h"


// Forward declarations to avoid dependency on zip.h
struct zip;
struct zip_file;


namespace dsb
{
namespace util
{

/**
\brief  A type for numeric zip entry indices.
\see ZipArchive
\see INVALID_ZIP_ENTRY_INDEX
*/
typedef std::uint64_t ZipEntryIndex;


/**
\brief  An index value that represents an invalid/unknown zip entry.
\see ZipArchive
*/
const ZipEntryIndex INVALID_ZIP_ENTRY_INDEX = 0xFFFFFFFFFFFFFFFFull;


/**
\brief  A class for reading ZIP archives.

Currently, only a limited set of reading operations are supported, and no
writing/modification operations.

A ZIP archive is organised as a number of *entries*, where each entry is a
file or a directory.  Each entry has a unique integer index, and the indices
run consecutively from 0 through `EntryCount()-1`.  For example, a file with
2 file entries and 1 directory entry, i.e. `EntryCount() == 3`, could look
like this:

    Index  Name
    -----  ----------------
        0  readme.txt
        1  images/
        2  images/photo.jpg

*/
class ZipArchive
{
public:
    /// Default constructor; does not associate the object with an archive file.
    ZipArchive() DSB_NOEXCEPT;

    /**
    \brief  Constructor which opens a ZIP archive.

    This is equivalent to default construction followed by a call to Open().

    \param [in] path
        The path to a ZIP archive file.

    \throws ZipException
        If there was an error opening the archive.
    */
    ZipArchive(const boost::filesystem::path& path);

    // Disable copying.
    ZipArchive(const ZipArchive&) = delete;
    ZipArchive& operator=(const ZipArchive&) = delete;

    /// Move constructor.
    ZipArchive(ZipArchive&&) DSB_NOEXCEPT;
    /// Move assignment operator.
    ZipArchive& operator=(ZipArchive&&) DSB_NOEXCEPT;

    /// Destructor; calls Discard().
    ~ZipArchive() DSB_NOEXCEPT;

    /**
    \brief  Opens a ZIP archive.

    \param [in] path
        The path to a ZIP archive file.
    \throws ZipException
        If there was an error opening the archive.
    \pre
        `IsOpen() == false`
    */
    void Open(const boost::filesystem::path& path);

    /**
    \brief  Closes the archive.

    If no archive is open, this function has no effect.
    */
    void Discard() DSB_NOEXCEPT;

    /// Returns whether this object refers to an open ZIP archive.
    bool IsOpen() const DSB_NOEXCEPT;

    /**
    \brief  Returns the number of entries in the archive.

    This includes both files and directories.

    \pre `IsOpen() == true`
    */
    std::uint64_t EntryCount() const;

    /**
    \brief  Finds an entry by name.

    \param [in] name
        The full name of a file or directory in the archive.  The search is
        case sensitive, and directory names must end with a forward slash (/).
    \returns
        The index of the entry with the given name, or INVALID_ZIP_ENTRY_INDEX
        if no such entry was found.
    \throws ZipException
        If there was an error accessing the archive.
    \pre
        `IsOpen() == true`
    */
    ZipEntryIndex FindEntry(const std::string& name) const;

    /**
    \brief  Returns the name of an archive entry.

    \param [in] index
        An archive entry index in the range `[0,EntryCount())`.
    \returns
        The full name of the entry with the given index.
    \throws ZipException
        If there was an error accessing the archive.
    \pre
        `IsOpen() == true`
    */
    std::string EntryName(ZipEntryIndex index) const;

    /**
    \brief  Returns whether an archive entry is a directory.

    This returns `true` if and only if the entry has zero size, has a CRC of
    zero, and a name which ends with a forward slash (/).

    \param [in] index
        An archive entry index in the range `[0,EntryCount())`.
    \returns
        Whether the entry with the given index is a directory.
    \throws ZipException
        If there was an error accessing the archive.
    \pre
        `IsOpen() == true`
    */
    bool IsDirEntry(ZipEntryIndex index) const;

    /**
    \brief  Extracts the entire contents of the archive.

    This will extract all entries in the archive to the given target directory,
    recreating the subdirectory structure in the archive.

    \param [in] targetDir
        The directory to which the files should be extracted.
    \throws ZipException
        If there was an error accessing the archive.
    \throws std::ios_base::failure
        On I/O error.
    \pre
        `IsOpen() == true`
    */
    void ExtractAll(
        const boost::filesystem::path& targetDir) const;

    /**
    \brief  Extracts a single file from the archive, placing it in a specific
            target directory.

    This ignores the directory structure *inside* the archive, i.e. the file
    will always be created directly under the given target directory.

    \param [in] index
        An archive entry index in the range `[0,EntryCount())`.
    \param [in] targetDir
        The directory to which the file should be extracted.

    \returns
        The full path to the extracted file, i.e. the base name of the archive
        entry appended to the target directory.
    \throws ZipException
        If there was an error accessing the archive.
    \throws std::ios_base::failure
        On I/O error.
    \pre
        `IsOpen() == true`
    */
    boost::filesystem::path ExtractFileTo(
        ZipEntryIndex index,
        const boost::filesystem::path& targetDir) const;

private:
    zip* m_archive;
};


/// Exception class for errors that occur while dealing with ZIP files.
class ZipException : public std::runtime_error
{
public:
    // Creates an exception with the given message
    ZipException(const std::string& msg) DSB_NOEXCEPT;

    // Creates an exception for the last error for the given archive
    ZipException(zip* archive) DSB_NOEXCEPT;

    // Creates an exception for the last error for the given file
    ZipException(zip_file* file) DSB_NOEXCEPT;
};


}} // namespace
#endif // header guard
