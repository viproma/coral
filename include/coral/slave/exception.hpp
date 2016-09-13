/**
\file
\brief Exceptions specific to coral::slave.
*/
#ifndef CORAL_SLAVE_EXCEPTION_HPP_INCLUDED
#define CORAL_SLAVE_EXCEPTION_HPP_INCLUDED

#include <chrono>
#include <stdexcept>
#include "coral/config.h"


namespace coral
{
namespace slave
{


/// Thrown when a communications timeout is reached.
class TimeoutException : public std::runtime_error
{
public:
    explicit TimeoutException(std::chrono::seconds timeoutDuration) CORAL_NOEXCEPT
        : std::runtime_error("Slave timed out due to lack of communication"),
          m_timeoutDuration(timeoutDuration)
    {
    }

    /// The duration of the timeout that was reached.
    std::chrono::seconds TimeoutDuration() const CORAL_NOEXCEPT
    {
        return m_timeoutDuration;
    }

private:
    std::chrono::seconds m_timeoutDuration;
};


}} // namespace
#endif // header guard
