/**
\file
\brief  Main header file for coral::log (but also contains a few macros).
\copyright
    Copyright 2013-present, SINTEF Ocean.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifndef CORAL_LOG_HPP
#define CORAL_LOG_HPP

#include <memory>
#include <ostream>
#include <string>
#include <boost/format.hpp>
#include <coral/config.h>


namespace coral
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
void Log(Level level, const char* message) noexcept;

/// Writes a plain C++ string to the global logger.
void Log(Level level, const std::string& message) noexcept;

/// Writes a formatted message to the global logger.
void Log(Level level, const boost::format& message) noexcept;


namespace detail
{
    // These are intended for use in the macros below
    void LogLoc(Level level, const char* file, int line, const char* message) noexcept;
    void LogLoc(Level level, const char* file, int line, const std::string& message) noexcept;
    void LogLoc(Level level, const char* file, int line, const boost::format& message) noexcept;
}


/**
\def    CORAL_LOG_TRACE(args)
\brief  If the macro CORAL_LOG_TRACE_ENABLED is defined, this is equivalent
        to calling `Log(trace, args)`, except that the file and line number
        are also logged.  Otherwise, it is a no-op.
*/
#ifdef CORAL_LOG_TRACE_ENABLED
#   define CORAL_LOG_TRACE(...) coral::log::detail::LogLoc(coral::log::trace, __FILE__, __LINE__, __VA_ARGS__)
#else
#   define CORAL_LOG_TRACE(...) ((void)0)
#endif

/**
\def    CORAL_LOG_DEBUG(args)
\brief  If either of the macros CORAL_LOG_DEBUG_ENABLED or CORAL_LOG_TRACE_ENABLED
        are defined, this is equivalent to calling `Log(debug, args)`, except
        that the file and line number are also logged.  Otherwise, it is a no-op.
*/
#if defined(CORAL_LOG_DEBUG_ENABLED) || defined(CORAL_LOG_TRACE_ENABLED)
#   define CORAL_LOG_DEBUG(...) coral::log::detail::LogLoc(coral::log::debug, __FILE__, __LINE__, __VA_ARGS__)
#else
#   define CORAL_LOG_DEBUG(...) ((void)0)
#endif


/**
\brief Adds a new log sink.

Until the first time this function is called, the library will use a default
sink that prints messages to `std::clog` and which filters out anything below
level `error`.

The first time this function is called, the default sink will be *replaced*
with the new one. Subsequent calls will add new sinks.
*/
void AddSink(std::shared_ptr<std::ostream> stream, Level level = error);


/// Convenience function for making a `std::shared_ptr` to `std::clog`.
std::shared_ptr<std::ostream> CLogPtr() noexcept;


}} // namespace
#endif // header guard
