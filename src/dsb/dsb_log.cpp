#include "dsb/log.hpp"

#include <atomic>
#include <iostream>
#include <mutex>


namespace dsb
{
namespace log
{


namespace
{
    // Globals
    std::atomic<Level> g_logLevel = error;
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


#define DSB_IMPLEMENT_LOG \
    if (level >= g_logLevel) { \
        std::lock_guard<std::mutex> lock(g_clogMutex); \
        std::clog << '[' << LevelNamePadded(level) << "] " \
                  << message << std::endl; \
    }
#define DSB_IMPLEMENT_LOG_LOC \
    if (level >= g_logLevel) { \
        std::lock_guard<std::mutex> lock(g_clogMutex); \
        std::clog << '[' << LevelNamePadded(level) << "] " \
                  << message \
                  << " (" << file << ':' << line << ')' << std::endl; \
    }


void Log(Level level, const char* message) DSB_NOEXCEPT
{
    DSB_IMPLEMENT_LOG
}


void Log(Level level, const std::string& message) DSB_NOEXCEPT
{
    DSB_IMPLEMENT_LOG
}


void Log(Level level, const boost::format& message) DSB_NOEXCEPT
{
    DSB_IMPLEMENT_LOG
}


void detail::LogLoc(Level level, const char* file, int line, const char* message) DSB_NOEXCEPT
{
    DSB_IMPLEMENT_LOG_LOC
}


void detail::LogLoc(Level level, const char* file, int line, const std::string& message) DSB_NOEXCEPT
{
    DSB_IMPLEMENT_LOG_LOC
}


void detail::LogLoc(Level level, const char* file, int line, const boost::format& message) DSB_NOEXCEPT
{
    DSB_IMPLEMENT_LOG_LOC
}


void SetLevel(Level level) DSB_NOEXCEPT
{
    g_logLevel = level;
}


}} // namespace
