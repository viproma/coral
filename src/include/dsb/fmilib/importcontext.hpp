#ifndef DSB_FMILIB_IMPORTCONTEXT_HPP
#define DSB_FMILIB_IMPORTCONTEXT_HPP

#include <memory>
#include <string>

#include "boost/filesystem/path.hpp"
#include "boost/noncopyable.hpp"
#include "fmilib.h"

#include "dsb/fmilib/logger.hpp"


namespace dsb
{
namespace fmilib
{

class Fmu;
class ImportContext;


/**
\brief  Allocates a new library context.

\param [in] logger      The logger which should be used.  If this is
                        null, a standard logger is acquired using
                        dsb::fmilib::StdStreamLogger().
\param [in] logLevel    The FMI Library log level that should be used.
                        This may be changed later via SetLogLevel().
*/
std::shared_ptr<ImportContext> MakeImportContext(
    std::shared_ptr<dsb::fmilib::ILogger> logger = nullptr,
    jm_log_level_enu_t logLevel = jm_log_level_info);


/**
\brief  Library context (required for importing FMUs).

This class wraps an `fmi_import_context_t`.  It cannot be instantiated directly;
use dsb::fmilib::MakeImportContext() for this purpose.
~~~{.cpp}
auto context = dsb::fmilib::MakeImportContext();
auto fmu = context->Import("path/to/my.fmu", "some/directory");
~~~
*/
class ImportContext : public std::enable_shared_from_this<ImportContext>,
                      boost::noncopyable
{
public:
    ~ImportContext();

    /**
    \brief  Imports an FMU.

    \param [in] fmuPath     The path to the FMU file.
    \param [in] unzipDir    The directory into which the contents of the
                            FMU will be unzipped.
    */
    std::shared_ptr<Fmu> Import(
        const boost::filesystem::path& fmuPath,
        const boost::filesystem::path& unzipDir);

    /**
    \brief  The current log level used by FMI Library.
    \see SetLogLevel()
    */
    jm_log_level_enu_t LogLevel() const;

    /**
    \brief  Sets the log level used by FMI Library.
    \see LogLevel()
    */
    void SetLogLevel(jm_log_level_enu_t value);

    /// Returns the last FMI Library error message.
    std::string LastErrorMessage();

    /// Returns a pointer to the underlying FMI Library import context.
    fmi_import_context_t* Handle() const;

private:
    // MakeImportContext() must have access to the constructor.
    friend std::shared_ptr<ImportContext> dsb::fmilib::MakeImportContext(
        std::shared_ptr<dsb::fmilib::ILogger>,
        jm_log_level_enu_t);

    // Private constructor, to force construction with MakeImportContext().
    ImportContext(std::shared_ptr<dsb::fmilib::ILogger> logger = nullptr,
                  jm_log_level_enu_t logLevel = jm_log_level_info);

    fmi_import_context_t* m_handle;
    jm_callbacks m_callbacks;
    std::shared_ptr<dsb::fmilib::ILogger> m_logger;
};


}} // namespace
#endif // header guard
