#include "dsb/fmilib/importcontext.hpp"

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <new>

#include "dsb/fmilib/fmu.hpp"
#include "dsb/fmilib/logger.hpp"
#include "dsb/fmilib/streamlogger.hpp"
#include "dsb/fmilib/fmu1.hpp"


std::shared_ptr<dsb::fmilib::ImportContext> dsb::fmilib::MakeImportContext(
    std::shared_ptr<dsb::fmilib::ILogger> logger,
    jm_log_level_enu_t logLevel)
{
    return std::shared_ptr<ImportContext>(new ImportContext(logger, logLevel));
}


namespace
{
    void LoggerCallback(jm_callbacks* callbacks,
                        jm_string module,
                        jm_log_level_enu_t logLevel,
                        jm_string message)
    {
        reinterpret_cast<dsb::fmilib::ILogger*>(callbacks->context)
            ->Log(module, logLevel, message);
    }
}


dsb::fmilib::ImportContext::ImportContext(std::shared_ptr<dsb::fmilib::ILogger> logger,
                                     jm_log_level_enu_t logLevel)
    : m_handle(nullptr),
      m_logger(!logger ? StdStreamLogger() : logger)
{
    m_callbacks.malloc = std::malloc;
    m_callbacks.calloc = std::calloc;
    m_callbacks.realloc = std::realloc;
    m_callbacks.free = std::free;
    m_callbacks.logger = LoggerCallback;
    m_callbacks.log_level = logLevel;
    m_callbacks.context = m_logger.get();
    std::memset(m_callbacks.errMessageBuffer, 0, JM_MAX_ERROR_MESSAGE_SIZE);

    m_handle = fmi_import_allocate_context(&m_callbacks);
    if (m_handle == nullptr) throw std::bad_alloc();
}


dsb::fmilib::ImportContext::~ImportContext()
{
    assert(m_handle != nullptr);
    fmi_import_free_context(m_handle);
}


std::shared_ptr<dsb::fmilib::Fmu> dsb::fmilib::ImportContext::Import(
    const boost::filesystem::path& fmuPath,
    const boost::filesystem::path& unzipDir)
{
    switch (fmi_import_get_fmi_version(
        m_handle,
        fmuPath.string().c_str(),
        unzipDir.string().c_str()))
    {
        case fmi_version_1_enu:
            return std::make_shared<dsb::fmilib::Fmu1>(shared_from_this(), unzipDir);
        case fmi_version_2_0_enu:
        case fmi_version_unsupported_enu:
            throw std::runtime_error("Unsupported FMI version for FMU '" + fmuPath.string() + "'");
        case fmi_version_unknown_enu:
            throw std::runtime_error("Failed to import FMU '" + fmuPath.string() + "'");
    }
    // Never get here
    throw std::logic_error("Internal error");
}


jm_log_level_enu_t dsb::fmilib::ImportContext::LogLevel() const
{
    return m_callbacks.log_level;
}


void dsb::fmilib::ImportContext::SetLogLevel(jm_log_level_enu_t value)
{
    m_callbacks.log_level = value;
}


std::string dsb::fmilib::ImportContext::LastErrorMessage()
{
    return std::string(jm_get_last_error(&m_callbacks));
}


fmi_import_context_t* dsb::fmilib::ImportContext::Handle() const
{
    return m_handle;
}
