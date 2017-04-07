/**
\file
\brief Defines the coral::master::Execution class and related functionality.
\copyright
    Copyright 2013-2017, SINTEF Ocean and the Coral contributors.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifndef CORAL_MASTER_EXECUTION_HPP
#define CORAL_MASTER_EXECUTION_HPP

#include <chrono>
#include <future>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include <coral/config.h>
#include <coral/master/execution_options.hpp>
#include <coral/model.hpp>
#include <coral/net.hpp>


namespace coral
{
namespace master
{


/// Constants used to indicate the result of `Execution::Step()`
enum class StepResult
{
    /// Step failed
    failed    = 0,

    /// Step succeeded
    completed = 1
};


/**
 *  \brief
 *  Specifies a slave which is to be added to an execution.
 *
 *  This class is used in calls to `Execution::Reconstitute()` to supply
 *  information about the slave which is to be added, and to obtain new
 *  information about the slave after it has been added (including any
 *  errors that may have occurred in the process).
 *
 *  Before an object of this type is passed to `Reconstitute()`, the `#locator`
 *  and `#name` fields must be set.  After `Reconstitute()` has completed
 *  successfully, the `#id` field contains the ID number of the new slave.
 *  If the function throws, the `#error` field may be queried to figure out
 *  whether this particular slave is the source of the failure, and if so, why.
 */
struct AddedSlave
{
    /// [Input] Information about the slave's network location.
    coral::net::SlaveLocator locator;

    /**
     *  \brief
     *  [Input] A name for the slave, unique in the execution.
     *
     *  Slave names may only consist of letters (a-z, A-Z), digits (0-9)
     *  and underscores (_). The first character must be a letter.
     *  If the string is empty, a unique name will be generated.
     */
    std::string name;

    /// [Output] The slave's unique ID.
    coral::model::SlaveID id = coral::model::INVALID_SLAVE_ID;

    /// [Output] The error reported by the slave, if any.
    std::error_code error;

    /// Default constructor.
    AddedSlave() CORAL_NOEXCEPT { }

    /// Constructor which sets the `#locator` and `#name` fields.
    AddedSlave(coral::net::SlaveLocator locator_, std::string name_)
        : locator{std::move(locator_)}, name(std::move(name_))
    { }
};


/**
 *  \brief
 *  Specifies variable values and connection changes for a single slave.
 *
 *  This class is used in calls to `Execution::Reconfigure()` to
 *  specify the changes which are to be effected for one particular
 *  slave, and to obtain information about any failures the slave
 *  might have reported regarding these changes.
 *
 *  Before an object of this type is passed to `Reconfigure()`,
 *  the `#slaveID` and `#variableSettings` fields must be set.
 *  If `Reconfigure()` throws, the `#error` field may be queried
 *  to figure out whether this particular slave contributed to
 *  the failure, and if so, why.
 */
struct SlaveConfig
{
    /**
     *  \brief
     *  [Input] The ID number of the slave whose variables are to
     *  be configured.
     */
    coral::model::SlaveID slaveID = coral::model::INVALID_SLAVE_ID;

    /// [Input] The variable value/connection changes.
    std::vector<coral::model::VariableSetting> variableSettings;

    /// [Output] The error reported by the slave, if any.
    std::error_code error;

    /// Default constructor.
    SlaveConfig() CORAL_NOEXCEPT { }

    /// Constructor which sets the `#slaveID` and `#variableSettings` fields.
    SlaveConfig(
        coral::model::SlaveID slaveID_,
        std::vector<coral::model::VariableSetting> variableSettings_)
        : slaveID(slaveID_)
        , variableSettings(std::move(variableSettings_))
    { }
};



/**
 *  \brief
 *  Creates and controls an execution.
 *  
 *  This class is used to set up and control an execution, i.e. a single
 *  simulation run.  This includes connecting and initialising slaves and
 *  executing time steps.
 */
class Execution
{
public:
    /**
     *  \brief
     *  Constructor which creates a new execution.
     *
     *  \param [in] executionName
     *      A (preferably unique) name for the execution.
     *  \param [in] options
     *      Configuration settings for the execution.
     */
    explicit Execution(
        const std::string& executionName,
        const ExecutionOptions& options = ExecutionOptions{});

    /// Destructor
    ~Execution() CORAL_NOEXCEPT;

    Execution(const Execution&) = delete;
    Execution& operator=(const Execution&) = delete;

    /// Move constructor
    Execution(Execution&&) CORAL_NOEXCEPT;

    /// Move assignment operator
    Execution& operator=(Execution&&) CORAL_NOEXCEPT;

    /**
     *  \brief
     *  Adds new slaves to the execution.
     *
     *  On input, the `slavesToAdd` vector must contain a list of slaves
     *  to add, the name and location of each specified in an `AddedSlave`
     *  object.  When the function returns successfully, these objects
     *  will have been updated with the ID numbers assigned to the
     *  respective slaves.
     *
     *  If the function throws an exception, and the error is related to
     *  one or more of the slaves, the corresponding `AddedSlave` objects
     *  will contain information about the errors.
     *
     *  The naming of this function reflects the fact that, in a future
     *  version, it is intended to also support *removing* slaves from an
     *  execution, and not just adding.
     *
     *  \param [in, out] slavesToAdd
     *      A list of slaves to add.  If empty, the function returns
     *      vacuously.  The `AddedSlave` objects will have been
     *      updated with information about the slaves on return.
     *  \param [in] commTimeout
     *      The communications timeout used to detect loss of communication
     *      with slaves.  A negative value means no timeout.
     */
    void Reconstitute(
        std::vector<AddedSlave>& slavesToAdd,
        std::chrono::milliseconds commTimeout);

    /**
     *  \brief
     *  Sets input variable values and establishes connections between
     *  output and input variables.
     *
     *  On input, the `slaveConfigs` vector must contain a list of slaves
     *  whose variables are to be modified, (re)connected and/or
     *  disconnected.  It must contain exactly one `SlaveConfig`
     *  object for each slave whose configuration is to be changed.
     *
     *  When a connection is made between an output variable and an input
     *  variable, or such a connection is to be broken, this is specified
     *  in the `SlaveConfig` object for the slave which owns the *input*
     *  variable.
     *
     *  If the function throws an exception, and the error originates in
     *  one or more of the slaves, the `SlaveConfig::error` fields
     *  of the corresponding `SlaveConfig` objects will be set to values
     *  that describe the errors.
     *
     *  \param [in, out] slaveConfigs
     *      A list of slave configuration change specifications,
     *      at most one entry per slave.
     *  \param [in] commTimeout
     *      The communications timeout used to detect loss of communication
     *      with the slaves.  A negative value means no timeout.
     */
    void Reconfigure(
        std::vector<SlaveConfig>& slaveConfigs,
        std::chrono::milliseconds commTimeout);

    /**
     *  \brief
     *  Initiates a time step.
     *
     *  This function requests that the simulation be advanced with the
     *  logical time specified by `stepSize`.  It returns a value
     *  that specifies whether the slaves succeeded in performing their
     *  calculations for the time step.  If the step was successful,
     *  i.e., the result is `StepResult::completed`, the operation may
     *  be confirmed and completed by calling AcceptStep().
     *
     *  The function may fail in two ways:
     *
     *    - It may return `StepResult::failed`, which means that one
     *      or more slaves failed to complete a time step of the given
     *      length, but that they might have succeeded with a shorter step
     *      length.
     *
     *    - It may throw an exception, which signals an irrecoverable error,
     *      e.g. network failure.
     *
     *  \note
     *  **Currently, discarding and retrying time steps are not
     *  supported, and both of the above must be considered irrecoverable
     *  failures.** In future versions, it will be possible to call
     *  a `DiscardStep()` function in the first case, to
     *  thereafter call `Step()` again with a shorter step
     *  length. (This is the reason why two function calls,
     *  `Step()` and `AcceptStep()`, are required per time step.)
     *
     *  \param [in] stepSize
     *      How much the simulation should be advanced in time.
     *      This must be a positive number.
     *  \param [in] timeout
     *      The communications timeout used to detect loss of communication
     *      with slaves.  A negative value means no timeout.
     *  \param [in] slaveResults
     *      An optional vector which, if given, will be cleared and filled
     *      with the result reported by each slave.
     *
     *  \returns
     *      Whether the operation was successful.
     */
    StepResult Step(
        coral::model::TimeDuration stepSize,
        std::chrono::milliseconds timeout,
        std::vector<std::pair<coral::model::SlaveID, StepResult>>* slaveResults = nullptr);

    /**
     *  \brief
     *  Confirms and completes a time step.
     *
     *  This method must be called after a successful `Step()` call,
     *  before any other operations are performed.
     *  See the `Step()` documentation for details.
     *
     *  \param timeout
     *      The communications timeout used to detect loss of communication
     *      with slaves.  A negative value means no timeout.
     */
    void AcceptStep(std::chrono::milliseconds timeout);

    /**
     *  \brief
     *  Terminates the execution.
     *
     *  No other methods may be called after a successful Terminate() call.
     */
    void Terminate();

private:
    class Private;
    std::unique_ptr<Private> m_private;
};


}}      //namespace
#endif  // header guard
