/*
Copyright 2013-present, SINTEF Ocean.
This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#include <coral/log.hpp>

#include <iostream>
#include <mutex>
#include <vector>


namespace coral
{
namespace log
{


namespace
{
    struct Sink
    {
        Level level;
        std::shared_ptr<std::ostream> stream;
    };
    std::mutex g_mutex;
    std::vector<Sink> g_sinks{{error, CLogPtr()}};
    bool g_sinksAdded = false;

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
    std::lock_guard<std::mutex> lock(g_mutex); \
    for (const auto& sink : g_sinks) { \
        if (level >= sink.level) { \
            *sink.stream \
                << '[' << LevelNamePadded(level) << "] " \
                << message << std::endl; \
        } \
    }

#define CORAL_IMPLEMENT_LOG_LOC \
    std::lock_guard<std::mutex> lock(g_mutex); \
    for (const auto& sink : g_sinks) { \
        if (level >= sink.level) { \
            *sink.stream \
                << '[' << LevelNamePadded(level) << "] " \
                << message \
                << " (" << file << ':' << line << ')' << std::endl; \
        } \
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


void AddSink(std::shared_ptr<std::ostream> stream, Level level)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_sinksAdded) {
        g_sinks.front().level = level;
        g_sinks.front().stream = stream;
        g_sinksAdded = true;
    } else {
        g_sinks.push_back({level, stream});
    }
}


std::shared_ptr<std::ostream> CLogPtr() noexcept
{
    return std::shared_ptr<std::ostream>(&std::clog, [] (void*) { });
}


}} // namespace
