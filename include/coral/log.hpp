/**
\file
\brief  Main header file for coral::log (but also contains a few macros).
\copyright
    Copyright 2013-2017, SINTEF Ocean and the Coral contributors.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifndef CORAL_LOG_HPP
#define CORAL_LOG_HPP

#include <string>
#include "boost/format.hpp"
#include "coral/config.h"


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
void Log(Level level, const char* message) CORAL_NOEXCEPT;

/// Writes a plain C++ string to the global logger.
void Log(Level level, const std::string& message) CORAL_NOEXCEPT;

/// Writes a formatted message to the global logger.
void Log(Level level, const boost::format& message) CORAL_NOEXCEPT;


namespace detail
{
    // These are intended for use in the macros below
    void LogLoc(Level level, const char* file, int line, const char* message) CORAL_NOEXCEPT;
    void LogLoc(Level level, const char* file, int line, const std::string& message) CORAL_NOEXCEPT;
    void LogLoc(Level level, const char* file, int line, const boost::format& message) CORAL_NOEXCEPT;
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

/// Sets the global log level, i.e., which log messages get written.
void SetLevel(Level level) CORAL_NOEXCEPT;

}} // namespace
#endif // header guard
