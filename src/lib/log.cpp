/*
Copyright 2013-2017, SINTEF Ocean and the Coral contributors.
This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#include <coral/log.hpp>

#include <atomic>
#include <iostream>
#include <mutex>


namespace coral
{
namespace log
{


namespace
{
    // Globals
    std::atomic<Level> g_logLevel{error};
    std::mutex g_clogMutex;

    // Returns a space-padded, human-readable string for each log level.
    const char* LevelNamePadded(Level level)
    {
        switch (level) {
            case trace:   return " trace ";
            case debug:   return " debug ";
            case info:    return " info  ";
            case warning: return "warning";
            case error:   return " error ";
            default:      return "unknown";
        }
    }
}


#define CORAL_IMPLEMENT_LOG \
    if (level >= g_logLevel) { \
        std::lock_guard<std::mutex> lock(g_clogMutex); \
        std::clog << '[' << LevelNamePadded(level) << "] " \
                  << message << std::endl; \
    }
#define CORAL_IMPLEMENT_LOG_LOC \
    if (level >= g_logLevel) { \
        std::lock_guard<std::mutex> lock(g_clogMutex); \
        std::clog << '[' << LevelNamePadded(level) << "] " \
                  << message \
                  << " (" << file << ':' << line << ')' << std::endl; \
    }


void Log(Level level, const char* message) noexcept
{
    CORAL_IMPLEMENT_LOG
}


void Log(Level level, const std::string& message) noexcept
{
    CORAL_IMPLEMENT_LOG
}


void Log(Level level, const boost::format& message) noexcept
{
    CORAL_IMPLEMENT_LOG
}


void detail::LogLoc(Level level, const char* file, int line, const char* message) noexcept
{
    CORAL_IMPLEMENT_LOG_LOC
}


void detail::LogLoc(Level level, const char* file, int line, const std::string& message) noexcept
{
    CORAL_IMPLEMENT_LOG_LOC
}


void detail::LogLoc(Level level, const char* file, int line, const boost::format& message) noexcept
{
    CORAL_IMPLEMENT_LOG_LOC
}


void SetLevel(Level level) noexcept
{
    g_logLevel = level;
}


}} // namespace
