/**
\file
\brief  Main header file for dsb::log (but also contains a few macros).
*/
#ifndef DSB_LOG_HPP
#define DSB_LOG_HPP

#include <string>
#include "boost/format.hpp"
#include "dsb/config.h"


namespace dsb
{
/// Program logging facilities.
namespace log
{


/// Log levels.
enum Level
{
    trace,
    debug,
    info,
    warning,
    error
};

/// Writes a plain C string to the global logger.
void Log(Level level, const char* message) DSB_NOEXCEPT;

/// Writes a plain C++ string to the global logger.
void Log(Level level, const std::string& message) DSB_NOEXCEPT;

/// Writes a formatted message to the global logger.
void Log(Level level, const boost::format& message) DSB_NOEXCEPT;


namespace detail
{
    // These are intended for use in the macros below
    void LogLoc(Level level, const char* file, int line, const char* message) DSB_NOEXCEPT;
    void LogLoc(Level level, const char* file, int line, const std::string& message) DSB_NOEXCEPT;
    void LogLoc(Level level, const char* file, int line, const boost::format& message) DSB_NOEXCEPT;
}


/**
\def    DSB_LOG_TRACE(args)
\brief  If the macro DSB_LOG_TRACE_ENABLED is defined, this is equivalent
        to calling `Log(trace, args)`, except that the file and line number
        are also logged.  Otherwise, it is a no-op.
*/
#ifdef DSB_LOG_TRACE_ENABLED
#   define DSB_LOG_TRACE(...) dsb::log::detail::LogLoc(dsb::log::trace, __FILE__, __LINE__, __VA_ARGS__)
#else
#   define DSB_LOG_TRACE(...) ((void)0)
#endif

/**
\def    DSB_LOG_DEBUG(args)
\brief  If either of the macros DSB_LOG_DEBUG_ENABLED or DSB_LOG_TRACE_ENABLED
        are defined, this is equivalent to calling `Log(debug, args)`, except
        that the file and line number are also logged.  Otherwise, it is a no-op.
*/
#if defined(DSB_LOG_DEBUG_ENABLED) || defined(DSB_LOG_TRACE_ENABLED)
#   define DSB_LOG_DEBUG(...) dsb::log::detail::LogLoc(dsb::log::debug, __FILE__, __LINE__, __VA_ARGS__)
#else
#   define DSB_LOG_DEBUG(...) ((void)0)
#endif

/// Sets the global log level, i.e., which log messages get written.
void SetLevel(Level level) DSB_NOEXCEPT;

}} // namespace
#endif // header guard
