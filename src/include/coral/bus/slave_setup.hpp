/**
\file
\brief  Defines the coral::bus::SlaveSetup class
\copyright
    Copyright 2013-present, SINTEF Ocean.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifndef CORAL_BUS_SLAVE_SETUP_HPP
#define CORAL_BUS_SLAVE_SETUP_HPP

#include <chrono>
#include <coral/model.hpp>


namespace coral
{
namespace bus
{


/**
\brief  Configuration data which is sent to each slave as they are added
        to the simulation.
*/
struct SlaveSetup
{
    SlaveSetup();
    SlaveSetup(
        coral::model::TimePoint startTime,
        coral::model::TimePoint stopTime,
        const std::string& executionName,
        std::chrono::milliseconds variableRecvTimeout);
    coral::model::TimePoint startTime;
    coral::model::TimePoint stopTime;
    std::string executionName;

    /**
    \brief  How long a slave should wait to receive variable values from
            other slaves before assuming that the connection is broken or
            that a subscription has failed to take effect.
    */
    std::chrono::milliseconds variableRecvTimeout;
};


}} // namespace
#endif // header guard
