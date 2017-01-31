/**
\file
\brief Configuration options for an execution.
\copyright
    Copyright 2013-2017, SINTEF Ocean and the Coral contributors.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifndef CORAL_MASTER_EXECUTION_OPTIONS_HPP
#define CORAL_MASTER_EXECUTION_OPTIONS_HPP

#include <chrono>
#include "coral/model.hpp"


namespace coral
{
namespace master
{


struct ExecutionOptions
{
    /// The start time of the simulation.
    coral::model::TimePoint startTime = 0.0;

    /**
    \brief  The maximum possible simulation time point.

    This may be coral::model::ETERNITY (the default), signifying that there
    is no predefined maximum time.  Otherwise, it must be greater than
    `startTime`.

    This is currently not used by Coral itself, but may be used by some
    slaves, e.g. to pre-allocate resources such as memory.
    */
    coral::model::TimePoint maxTime = coral::model::ETERNITY;

    /**
    \brief  Timeout used by the slaves to detect loss of communication with
            other slaves.

    This is used when slaves exchange variable values among themselves.
    */
    std::chrono::milliseconds slaveVariableRecvTimeout = std::chrono::seconds(1);
};


}} // namespace
#endif // header guard
