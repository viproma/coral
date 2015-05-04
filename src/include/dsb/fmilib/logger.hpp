#ifndef DSB_FMILIB_LOGGER_HPP
#define DSB_FMILIB_LOGGER_HPP

#include "boost/noncopyable.hpp"
#include "fmilib.h"


namespace dsb
{
namespace fmilib
{


/// An interface for objects that log status messages from FMI Library.
class ILogger : boost::noncopyable
{
public:
    /**
    \brief  Logs a single status message.

    \param [in] module      The reporting module.
    \param [in] logLevel    The message type.
    \param [in] message     The message.
    */
    virtual void Log(jm_string module,
                     jm_log_level_enu_t logLevel,
                     jm_string message) = 0;

    virtual ~ILogger() { }
};


}} // namespace
#endif // header guard
