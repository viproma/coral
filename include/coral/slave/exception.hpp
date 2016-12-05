/**
\file
\brief Exceptions specific to coral::slave.
*/
#ifndef CORAL_SLAVE_EXCEPTION_HPP_INCLUDED
#define CORAL_SLAVE_EXCEPTION_HPP_INCLUDED

#include <chrono>
#include <stdexcept>
#include <string>
#include "coral/config.h"


namespace coral
{
namespace slave
{


/// Thrown when a communications timeout is reached.
class TimeoutException : public std::runtime_error
{
public:
    explicit TimeoutException(std::chrono::milliseconds timeoutDuration) CORAL_NOEXCEPT
        : std::runtime_error("Slave timed out due to lack of communication"),
          m_timeoutDuration(timeoutDuration)
    {
    }

    TimeoutException(
        const std::string& message,
        std::chrono::milliseconds timeoutDuration) CORAL_NOEXCEPT
        : std::runtime_error(
            message + " (timeout: " + std::to_string(timeoutDuration.count()) + " ms)")
        , m_timeoutDuration(timeoutDuration)
    {
    }

    /// The duration of the timeout that was reached.
    std::chrono::milliseconds TimeoutDuration() const CORAL_NOEXCEPT
    {
        return m_timeoutDuration;
    }

private:
    std::chrono::milliseconds m_timeoutDuration;
};


}} // namespace
#endif // header guard
