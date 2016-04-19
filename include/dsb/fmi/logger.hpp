#ifndef DSB_FMI_LOGGER_HPP
#define DSB_FMI_LOGGER_HPP


namespace dsb
{
namespace fmi
{


/// An interface for objects that log status messages from FMI Library.
class Logger
{
public:
    /**
    \brief  Logs a single status message.

    \param [in] module      The reporting module.
    \param [in] logLevel    The message type.
    \param [in] message     The message.
    */
    virtual void Log(const char* module, int logLevel, const char* message) = 0;

    virtual ~Logger() { }
};


}} // namespace
#endif // header guard
