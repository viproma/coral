#ifndef DSBEXEC_CONFIG_PARSER_HPP
#define DSBEXEC_CONFIG_PARSER_HPP

#include <chrono>
#include <ostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "dsb/config.h"
#include "dsb/master.hpp"
#include "dsb/model.hpp"


struct SimulationEvent
{
    SimulationEvent(
        dsb::model::TimePoint t,
        dsb::model::SlaveID s,
        dsb::model::VariableID v,
        const dsb::model::ScalarValue& n);
    dsb::model::TimePoint timePoint;
    dsb::model::SlaveID slave;
    dsb::model::VariableID variable;
    dsb::model::ScalarValue newValue;
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
    dsb::master::Cluster& providers,
    dsb::master::Execution& execution,
    std::vector<SimulationEvent>& scenario,
    std::chrono::milliseconds commTimeout,
    std::chrono::milliseconds instantiationTimeout,
    std::ostream* warningLog);


class SetVariablesException : public std::runtime_error
{
public:
    SetVariablesException();
    const char* what() const DSB_NOEXCEPT override;
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
    \brief  Slave timeout, in seconds

    This controls how long the slaves (and the execution broker, if this is
    used) will wait for commands from the master.  This should generally be
    a long duration, as the execution master could for instance be waiting
    for some user input before starting/continuing the simulation.

    The default value is 1 hour.
    */
    std::chrono::seconds slaveTimeout;

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
