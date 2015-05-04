#ifndef DSB_FMILIB_STREAMLOGGER_HPP
#define DSB_FMILIB_STREAMLOGGER_HPP

#include <memory>
#include <ostream>
#include <string>

#include "boost/format.hpp"

#include "dsb/config.h"
#include "dsb/fmilib/logger.hpp"


namespace dsb
{
namespace fmilib
{


/// The standard message format used by dsb::fmilib::StreamLogger.
const std::string streamLoggerFormat = "%3$s: %4$s\n";


/**
\brief  A logger which logs messages to an output stream.
\see dsb::fmilib::StdStreamLogger()
*/
class StreamLogger : public dsb::fmilib::ILogger
{
public:
    /**
    \brief  Constructs a new StreamLogger.

    This class uses [Boost Format](http://www.boost.org/doc/libs/release/libs/format/)
    internally, and thus supports the same format strings.  The arguments
    that are passed to the formatter are, in order,

      1. Module name (`jm_string`, which is a `typedef` for `const char*`)
      2. Log level (`jm_log_level_enu_t`, which is an `enum`)
      3. Log level name (`jm_string`)
      4. Message (`jm_string`)

    Positional arguments may be used to rearrange and/or elide arguments.
    For example, the format string `"%3$s: (%1$s) %4$s"` would result in
    messages like

        DEBUG: (FMICAPI) There was a problem

    The format string is used immediately to format a throwaway test message
    in the constructor, so that the function throws an
    [exception](http://www.boost.org/doc/libs/release/libs/format/doc/format.html#exceptions)
    if the string is ill-formed.

    \param [in] stream  A reference to the output stream.
    \param [in] format  A format string used for each message.

    \throws boost::format_error if `format` is ill-formed.
    */
    StreamLogger(std::shared_ptr<std::ostream> stream,
                 const std::string& format = dsb::fmilib::streamLoggerFormat);

    /// Implements dsb::fmilib::ILogger::Log()
    void Log(jm_string module,
             jm_log_level_enu_t logLevel,
             jm_string message) DSB_FINAL override;

private:
    std::shared_ptr<std::ostream> m_stream;
    boost::format m_format;
};


/**
\brief  Creates and returns an dsb::fmilib::StreamLogger instance which logs to the standard
        log stream (as defined by `std::clog`).

\param [in] format  The message format.  This is passed straight to
                    dsb::fmilib::StreamLogger::StreamLogger().
*/
std::shared_ptr<dsb::fmilib::StreamLogger> StdStreamLogger(
    const std::string& format = dsb::fmilib::streamLoggerFormat);


}} // namespace
#endif // header guard
