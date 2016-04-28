#include "dsb/fmi/dsblogger.hpp"

#include "dsb/log.hpp"
#include "fmilib.h"


namespace dsb
{
namespace fmi
{

namespace
{
    dsb::log::Level ConvertLogLevel(int jmLogLevel)
    {
        switch (jmLogLevel)
        {
        case jm_log_level_debug:
            return dsb::log::debug;
        case jm_log_level_verbose:
        case jm_log_level_info:
            return dsb::log::info;
        case jm_log_level_warning:
            return dsb::log::warning;
        case jm_log_level_error:
        case jm_log_level_fatal:
        case jm_log_level_nothing:
        case jm_log_level_all:
        default:
            // The last two cases + default should never match, and if
            // they do, we at least make sure it prints an error message.
            return dsb::log::error;
        }
    }
}


void DSBLogger::Log(const char* module, int logLevel, const char* message)
{
    dsb::log::Log(
        ConvertLogLevel(logLevel),
        boost::format(" FMI [%s]: %s") % module % message);
}


}} // namespace
