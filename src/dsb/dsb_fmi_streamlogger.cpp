#include "dsb/fmi/streamlogger.hpp"

#include <iostream>
#include "boost/format.hpp"
#include "fmilib.h"


namespace dsb
{
namespace fmi
{

namespace
{
    // This function defines the order in which arguments are formatted.
    boost::format& FormatArgs(
        boost::format& format,
        const char* module,
        int logLevel,
        const char* message)
    {
        return format
            % module
            % logLevel
            % jm_log_level_to_string(static_cast<jm_log_level_enu_t>(logLevel))
            % message;
    }
}


StreamLogger::StreamLogger(
    std::shared_ptr<std::ostream> stream,
    const std::string& format)
    : m_stream{stream}
    , m_format{format}
{
    // Test format string.  This will throw an exception if it is ill-formed.
    boost::str(FormatArgs(m_format, "module", jm_log_level_nothing, "message"));
}


void StreamLogger::Log(const char* module, int logLevel, const char* message)
{
    m_format.clear();
    *m_stream << FormatArgs(m_format, module, logLevel, message);
}


namespace
{
    // A "deleter" which does nothing, for use in StdStreamLogger().
    void NoOpDeleter(std::ostream*) { }
}


std::shared_ptr<StreamLogger> StdStreamLogger(
    const std::string& format)
{
    // We don't actually want to delete std::clog when the last shared pointer
    // to it goes out of scope, so we supply a custom deleter which does
    // nothing.
    return std::make_shared<StreamLogger>(
        std::shared_ptr<std::ostream>(&std::clog, NoOpDeleter),
        format);
}


}} // namespace
