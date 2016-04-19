#ifndef DSB_FMI_IMPORTER_HPP
#define DSB_FMI_IMPORTER_HPP

#include <map>
#include <memory>
#include <string>

#include "boost/filesystem/path.hpp"
#include "boost/optional.hpp"

#include "dsb/config.h"
#include "dsb/util.hpp"


// Forward declarations to avoid external dependency on FMI Library.
struct fmi_xml_context_t;
typedef fmi_xml_context_t fmi_import_context_t;
struct jm_callbacks;


namespace dsb
{
/// Classes and functions related to the Functional Mock-up Interface (FMI).
namespace fmi
{

class Logger;
class FMU;


/**
\brief  Imports and caches FMUs.

The main purpose of this class is to read %FMU files and create dsb::fmi::FMU
objects to represent them.  This is done with the Import() function.

An Importer object uses an on-disk cache that holds the unpacked contents
of previously imported FMUs, so that they don't need to be unpacked anew every
time they are imported.  This is a huge time-saver when large and/or many FMUs
are loaded.  The path to this cache may be supplied by the user, in which case
it is not automatically emptied on destruction.  Thus, if the same path is
supplied each time, the cache becomes persistent between program runs.
It may be cleared manually by calling CleanCache().

\warning
    Currently there are no synchronisation mechanisms to protect the cache from
    concurrent use, so accessing the same cache from multiple
    instances/processes will likely cause problems.
*/
class Importer : public std::enable_shared_from_this<Importer>
{
public:
    /**
    \brief  Creates a new %FMU importer that uses a specific cache directory.

    The given directory will not be removed nor emptied on destruction.

    \param [in] cachePath
        The path to the directory which will hold the %FMU cache.  If it does
        not exist already, it will be created.
    \param [in] logger
        The logger which should be used.  If this is null, a standard logger is
        acquired using dsb::fmi::StdStreamLogger().
    */
    static std::shared_ptr<Importer> Create(
        const boost::filesystem::path& cachePath,
        std::shared_ptr<Logger> logger = nullptr);

    /**
    \brief  Creates a new %FMU importer that uses a temporary cache directory.

    A new cache directory will be created in a location suitable for temporary
    files under the conventions of the operating system.  It will be completely
    removed again on destruction.

    \param [in] logger
        The logger which should be used.  If this is null, a standard logger is
        acquired using dsb::fmi::StdStreamLogger().
    */
    static std::shared_ptr<Importer> Create(
        std::shared_ptr<Logger> logger = nullptr);

private:
    // Private constructors, to force use of factory functions.
    Importer(
        const boost::filesystem::path& cachePath,
        std::shared_ptr<Logger> logger);
    Importer(
        dsb::util::TempDir tempDir,
        std::shared_ptr<Logger> logger);

public:
    /**
    \brief  Imports and loads an %FMU.

    Loaded FMUs are managed using reference counting.  If an %FMU is loaded,
    and then the same %FMU is loaded again before the first one has been
    destroyed, the second call will return a reference to the first one.
    (Two FMUs are deemed to be the same if they have the same path *or* the
    same GUID.)

    \param [in] fmuPath
        The path to the %FMU file.
    \returns
        An object which represents the imported %FMU.
    */
    std::shared_ptr<FMU> Import(const boost::filesystem::path& fmuPath);

    /**
    \brief  Removes unused files and directories from the %FMU cache.

    This will remove all %FMU contents from the cache, except the ones for
    which there currently exist FMU objects.
    */
    void CleanCache();

    /// Returns the last FMI Library error message.
    std::string LastErrorMessage();

    /// Returns a pointer to the underlying FMI Library import context.
    fmi_import_context_t* FmilibHandle() const;

private:
    void PrunePtrCaches();

    // Note: The order of these declarations is important!
    boost::optional<dsb::util::TempDir> m_tempCacheDir; // Only used when no cache dir is given
    std::shared_ptr<Logger> m_logger;
    std::unique_ptr<jm_callbacks> m_callbacks;
    std::unique_ptr<fmi_import_context_t, void (*)(fmi_import_context_t*)> m_handle;

    boost::filesystem::path m_fmuDir;
    boost::filesystem::path m_workDir;

    std::map<boost::filesystem::path, std::weak_ptr<FMU>> m_pathCache;
    std::map<std::string, std::weak_ptr<FMU>> m_guidCache;
};


}} // namespace
#endif // header guard
