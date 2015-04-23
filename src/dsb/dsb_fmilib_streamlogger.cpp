#include "dsb/fmilib/streamlogger.hpp"

#include <iostream>
#include "boost/format.hpp"
#include "fmilib.h"


namespace
{
    // This function defines the order in which arguments are formatted.
    boost::format& FormatArgs(
        boost::format& format,
        jm_string module,
        jm_log_level_enu_t logLevel,
        jm_string message)
    {
        return format % module
                      % logLevel
                      % jm_log_level_to_string(logLevel)
                      % message;
    }
}


dsb::fmilib::StreamLogger::StreamLogger(std::shared_ptr<std::ostream> stream,
                                   const std::string& format)
    : m_stream(stream),
      m_format(format)
{
    // Test format string.  This will throw an exception if it is ill-formed.
    boost::str(FormatArgs(m_format, "module", jm_log_level_nothing, "message"));
}


void dsb::fmilib::StreamLogger::Log(
    jm_string module,
    jm_log_level_enu_t logLevel,
    jm_string message)
{
    m_format.clear();
    *m_stream << FormatArgs(m_format, module, logLevel, message);
}


namespace
{
    // A "deleter" which does nothing, for use in StdStreamLogger().
    void NoOpDeleter(std::ostream*) { }
}


std::shared_ptr<dsb::fmilib::StreamLogger> dsb::fmilib::StdStreamLogger(
    const std::string& format)
{
    // We don't actually want to delete std::clog when the last shared pointer
    // to it goes out of scope, so we supply a custom deleter which does
    // nothing.
    return std::make_shared<dsb::fmilib::StreamLogger>(
        std::shared_ptr<std::ostream>(&std::clog, NoOpDeleter),
        format);
}
