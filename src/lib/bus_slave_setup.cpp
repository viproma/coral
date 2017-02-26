/*
Copyright 2013-2017, SINTEF Ocean and the Coral contributors.
This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#include "coral/bus/slave_setup.hpp"

#include <cassert>
#include <limits>


namespace coral
{
namespace bus
{


SlaveSetup::SlaveSetup()
    : startTime(std::numeric_limits<coral::model::TimePoint>::signaling_NaN()),
      stopTime(std::numeric_limits<coral::model::TimePoint>::signaling_NaN())
{
}


SlaveSetup::SlaveSetup(
    coral::model::TimePoint startTime_,
    coral::model::TimePoint stopTime_,
    const std::string& executionName_,
    std::chrono::milliseconds variableRecvTimeout_)
    : startTime(startTime_),
      stopTime(stopTime_),
      executionName(executionName_),
      variableRecvTimeout(variableRecvTimeout_)
{
    assert(startTime <= stopTime);
    assert(variableRecvTimeout > std::chrono::milliseconds(0));
}


}} // namespace
