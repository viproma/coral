#ifndef DSB_FMI_DSBLOGGER_HPP
#define DSB_FMI_DSBLOGGER_HPP

#include "dsb/config.h"
#include "dsb/fmi/logger.hpp"


namespace dsb
{
namespace fmi
{


/**
\brief  A logger which logs messages using the logging mechanism defined by
        dsb::log.
*/
class DSBLogger : public Logger
{
public:
    // Logger implementations:
    void Log(const char* module, int logLevel, const char* message) override;
};


}} // namespace
#endif // header guard
