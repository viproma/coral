/**
\file
\brief Exceptions specific to dsb::slave.
*/
#ifndef DSB_SLAVE_EXCEPTION_HPP_INCLUDED
#define DSB_SLAVE_EXCEPTION_HPP_INCLUDED

#include <chrono>
#include <stdexcept>
#include "dsb/config.h"


namespace dsb
{
namespace slave
{


/// Thrown when a communications timeout is reached.
class TimeoutException : public std::runtime_error
{
public:
    explicit TimeoutException(std::chrono::seconds timeoutDuration) DSB_NOEXCEPT
        : std::runtime_error("Slave timed out due to lack of communication"),
          m_timeoutDuration(timeoutDuration)
    {
    }

    /// The duration of the timeout that was reached.
    std::chrono::seconds TimeoutDuration() const DSB_NOEXCEPT
    {
        return m_timeoutDuration;
    }

private:
    std::chrono::seconds m_timeoutDuration;
};


}} // namespace
#endif // header guard
