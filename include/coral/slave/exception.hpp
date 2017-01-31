/**
\file
\brief Exceptions specific to coral::slave.
\copyright
    Copyright 2013-2017, SINTEF Ocean and the Coral contributors.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
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
