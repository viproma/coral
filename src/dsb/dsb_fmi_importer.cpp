#include "dsb/fmi/importer.hpp"

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <new>

#include "boost/filesystem.hpp"
#include "boost/property_tree/ptree.hpp"
#include "boost/property_tree/xml_parser.hpp"

#include "fmilib.h"

#include "dsb/fmi/fmu1.hpp"
#include "dsb/fmi/logger.hpp"
#include "dsb/fmi/streamlogger.hpp"
#include "dsb/util.hpp"
#include "dsb/util/zip.hpp"


namespace dsb
{
namespace fmi
{


std::shared_ptr<Importer> Importer::Create(
    const boost::filesystem::path& cachePath,
    std::shared_ptr<Logger> logger)
{
    return std::shared_ptr<Importer>(new Importer(cachePath, logger));
}


std::shared_ptr<Importer> Importer::Create(std::shared_ptr<Logger> logger)
{
    return std::shared_ptr<Importer>(new Importer(
        dsb::util::TempDir(),
        logger));
}


namespace
{
    void LoggerCallback(
        jm_callbacks* callbacks,
        jm_string module,
        jm_log_level_enu_t logLevel,
        jm_string message)
    {
        reinterpret_cast<Logger*>(callbacks->context)
            ->Log(module, logLevel, message);
    }

    std::unique_ptr<jm_callbacks> MakeCallbacks(Logger* logger)
    {
        auto c = std::make_unique<jm_callbacks>();
        std::memset(c.get(), 0, sizeof(jm_callbacks));
        c->malloc = std::malloc;
        c->calloc = std::calloc;
        c->realloc = std::realloc;
        c->free = std::free;
        c->logger = &LoggerCallback;
        c->log_level = jm_log_level_warning;
        c->context = logger;
        std::memset(c->errMessageBuffer, 0, JM_MAX_ERROR_MESSAGE_SIZE);
        return c;
    }
}


Importer::Importer(
    const boost::filesystem::path& cachePath,
    std::shared_ptr<Logger> logger)
    : m_logger{!logger ? StdStreamLogger() : logger}
    , m_callbacks{MakeCallbacks(m_logger.get())}
    , m_handle{fmi_import_allocate_context(m_callbacks.get()), &fmi_import_free_context}
    , m_fmuDir{cachePath / "fmu"}
    , m_workDir{cachePath / "tmp"}
{
    if (m_handle == nullptr) throw std::bad_alloc();
}


Importer::Importer(dsb::util::TempDir tempDir, std::shared_ptr<Logger> logger)
    : Importer{tempDir.Path(), logger}
{
    m_tempCacheDir = std::move(tempDir);
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

        const auto fmiVersion = xml.get(
            "fmiModelDescription.<xmlattr>.fmiVersion",
            std::string());
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
            if (md.guid.empty()) {
                throw std::runtime_error(
                    "Invalid modelDescription.xml; guid attribute missing or empty");
            }
        }
        return md;
    }
}


std::shared_ptr<FMU> Importer::Import(const boost::filesystem::path& fmuPath)
{
    PrunePtrCaches();
    auto pit = m_pathCache.find(fmuPath);
    if (pit != end(m_pathCache)) return pit->second.lock();

    const auto zip = dsb::util::ZipArchive(fmuPath);
    const auto tempMdDir = m_workDir / dsb::util::RandomUUID();
    boost::filesystem::create_directories(tempMdDir);
    const auto removeTempMdDir = dsb::util::OnScopeExit([&](){
        boost::system::error_code ignored;
        boost::filesystem::remove_all(tempMdDir, ignored);
    });

    const auto modelDescriptionIndex = zip.FindEntry("modelDescription.xml");
    if (modelDescriptionIndex == dsb::util::INVALID_ZIP_ENTRY_INDEX) {
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

    const auto fmuUnpackDir = m_fmuDir / minModelDesc.guid;
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
