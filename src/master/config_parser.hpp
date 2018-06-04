/*
Copyright 2013-present, SINTEF Ocean.
This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifndef CORALMASTER_CONFIG_PARSER_HPP
#define CORALMASTER_CONFIG_PARSER_HPP

#include <chrono>
#include <functional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <coral/config.h>
#include <coral/master.hpp>
#include <coral/model.hpp>


struct SimulationEvent
{
    SimulationEvent(
        coral::model::TimePoint t,
        coral::model::SlaveID s,
        coral::model::VariableID v,
        const coral::model::ScalarValue& n);
    coral::model::TimePoint timePoint;
    coral::model::SlaveID slave;
    coral::model::VariableID variable;
    coral::model::ScalarValue newValue;
};


/**
\brief  Sets up the system to be simulated in an execution based on a
        configuration file.

\param [in] path        The path to the configuration file.
\param [in] execution   The execution controller.

\throws std::runtime_error if there were errors in the configuraiton file.
*/
//TODO: Split this into two functions: one which reads the configuration
// and one which applies it to the controller.
void ParseSystemConfig(
    const std::string& path,
    coral::master::ProviderCluster& providers,
    coral::master::Execution& execution,
    std::vector<SimulationEvent>& scenario,
    std::chrono::milliseconds commTimeout,
    std::chrono::milliseconds instantiationTimeout,
    std::ostream* warningLog,
    std::function<void()> postInstantiationHook);


class SetVariablesException : public std::runtime_error
{
public:
    SetVariablesException();
    const char* what() const noexcept override;
    void AddSlaveError(const std::string& slaveName, const std::string& errMsg);
private:
    std::string m_msg;
    std::vector<std::pair<std::string, std::string>> m_slaveErrors;
};


/// Configuration parameters for an execution run.
struct ExecutionConfig
{
    ExecutionConfig();
    /// Simulation start time
    double startTime;

    /// Simulation stop time
    double stopTime;

    /// Simulation step size
    double stepSize;

    /**
    \brief  General command/communications timeout, in milliseconds

    This is how long the master will wait for replies to commands sent to a
    slave before it considers the connection to be broken.  It should generally
    be a short duration, as it is used for "cheap" operations (everything
    besides the "step" command).  The default value is 1 second.
    */
    std::chrono::milliseconds commTimeout;

    /**
    \brief  Time step timeout multiplier

    This controls the amount of time the slaves get to carry out a time step.
    The timeout is set equal to `stepTimeoutMultiplier` times the step size,
    where the step size is assumed to be in seconds.

    The default value is 100, allowing for a simulation which runs at, at most,
    a hundredth of real-time speed.
    */
    double stepTimeoutMultiplier;

    /**
    \brief  Slave instantiation timeout, in milliseconds.

    This controls how long each slave gets from the moment the instantiation
    command is issued to the moment it is ready for a command from the master
    node.
    */
    std::chrono::milliseconds instantiationTimeout;
};


/**
\brief  Parses an execution configuration file.
\throws std::runtime_error if there were errors in the configuration file.
*/
ExecutionConfig ParseExecutionConfig(const std::string& path);


#endif // header guard
