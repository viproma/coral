/**
\file
\brief Defines the dsb::master::Execution class and related functionality.
*/
#ifndef DSB_MASTER_EXECUTION_HPP
#define DSB_MASTER_EXECUTION_HPP

#include <chrono>
#include <future>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "dsb/config.h"
#include "dsb/model.hpp"
#include "dsb/net.hpp"


namespace dsb
{
namespace master
{


/// Constants used to indicate the result of `Execution::Step()`
enum class StepResult
{
    failed    = 0,
    completed = 1
};


/**
\brief  Used in `Execution::Reconstitute()` to specify a slave which should
        be added to the simulation.

This class is used to supply information about the slave which is to be
added, and to obtain information about the slave after it has been added
(including any errors that may have occurred in the process).

Before an object of this type is passed to `Reconstitute()`, the `#locator`
and `#name` fields must be set.  After `Reconstitute()` has completed
successfully, the `#id` field contains the ID number of the new slave.
If the function throws, the `#error` field may be queried to figure out
whether this particular slave is the source of the failure, and if so, why.
*/
struct AddedSlave
{
    /// [Input] The slave's network location
    dsb::net::SlaveLocator locator;

    /// [Input] A name for the slave, unique in the execution
    std::string name;

    /// [Output] The slave's unique ID number
    dsb::model::SlaveID id = dsb::model::INVALID_SLAVE_ID;

    /// [Output] The error reported by the slave, if any
    std::error_code error;

    /// Default constructor
    AddedSlave() DSB_NOEXCEPT { }

    /// Constructor which sets the `#locator` and `#name` fields.
    AddedSlave(dsb::net::SlaveLocator locator_, std::string name_)
        : locator{std::move(locator_)}, name(std::move(name_))
    { }
};


/**
\brief  Used in `Execution::Reconfigure()` to specify variable value
        and connection changes.

This class is used to supply information about the changes which are to
be effected for one particular slave, and to obtain information about any
failures the slave might have reported.

Before an object of this type is passed to `Reconfigure()`, the `#slaveID`
and `#variableSettings` fields must be set.  If `Reconfigure()` throws,
the `#error` field may be queried to figure out whether this particular
slave contributed to the failure, and if so, why.
*/
struct SlaveConfig
{
    /// [Input] The ID number of the slave whose variables are to be configured
    dsb::model::SlaveID slaveID = dsb::model::INVALID_SLAVE_ID;

    /// [Input] The variable value/connection changes
    std::vector<dsb::model::VariableSetting> variableSettings;

    /// [Output] The error reported by the slave, if any
    std::error_code error;

    /// Default constructor
    SlaveConfig() DSB_NOEXCEPT { }

    /// Constructor which sets the `#slaveID` and `#variableSettings` fields
    SlaveConfig(
        dsb::model::SlaveID slaveID_,
        std::vector<dsb::model::VariableSetting> variableSettings_)
        : slaveID(slaveID_)
        , variableSettings(std::move(variableSettings_))
    { }
};



/**
\brief  Master execution controller.

This class is used by the master entity in an execution to initialize,
run and shut down the simulation.
*/
class Execution
{
public:
    /**
    \brief  Constructor.

    \param [in] executionName
        A (preferably unique) name for the execution.
    \param [in] startTime
        The start time of the simulation.
    \param [in] maxTime
        The maximum simulation time point.  This may be dsb::model::ETERNITY
        (the default), signifying that there is no predefined maximum time.
        Otherwise, it must be greater than `startTime`.
    */
    explicit Execution(
        const std::string& executionName,
        dsb::model::TimePoint startTime = 0.0,
        dsb::model::TimePoint maxTime = dsb::model::ETERNITY);

    /// Destructor
    ~Execution() DSB_NOEXCEPT;

    Execution(const Execution&) = delete;
    Execution& operator=(const Execution&) = delete;

    /// Move constructor
    Execution(Execution&&) DSB_NOEXCEPT;

    /// Move assignment operator
    Execution& operator=(Execution&&) DSB_NOEXCEPT;

    /**
    \brief  Adds new slaves to the execution.

    On input, the `slavesToAdd` vector must contain a list of slaves to add,
    the name and location of each specified in an `AddedSlave` object.
    When the function returns successfully, these objects will have been
    updated with the ID numbers assigned to the respective slaves.

    If the function throws an exception, and the error is related to
    one or more of the slaves, the corresponding `AddedSlave` objects
    will contain information about the errors.

    The naming of this function reflects the fact that, in a future version,
    it is intended to also support *removing* slaves from an execution, and
    not just adding.

    \param [in, out] slavesToAdd
        A list of slaves to add.  If empty, the function returns vacuously.
        Contains information about the slaves on output.
    \param [in] commTimeout
        The communications timeout used to detect loss of communication
        with the slaves.
    */
    void Reconstitute(
        std::vector<AddedSlave>& slavesToAdd,
        std::chrono::milliseconds commTimeout);

    /**
    \brief  Sets the values of and/or connects variables.

    On input`, the `slaveConfigs` vector must contain a list of slaves whose
    variables are to be modified, (re)connected and/or disconnected.
    It must contain exactly one `SlaveConfig` object for each slave whose
    configuration is to be changed.

    When a connection is made between an output variable and an input
    variable, or such a connection is to be broken, this is specified in
    the `SlaveConfig` object for the slave which owns the *input* variable.

    If the function throws an exception, and the error originates in one
    or more of the slaves, the `error` fields of the corresponding
    `SlaveConfig` objects will be set to values that describe the errors.

    \param [in, out] slaveConfigs
        A list of slave configuration change specifications, at most one
        entry per slave.
    \param [in] commTimeout
        The communications timeout used to detect loss of communication
        with the slaves.
    */
    void Reconfigure(
        std::vector<SlaveConfig>& slaveConfigs,
        std::chrono::milliseconds commTimeout);

    /**
    \brief  Steps the simulation forward.

    The function returns whether the step succeeded or not.  If all slaves
    were able to carry out their computations for the time step, STEP_COMPLETE
    will be returned.  If one or more slaves were only able to carry out a
    partial time step, or could not carry out the time step at all, STEP_FAILED
    will be returned.  It may then be possible to discard the step and retry
    with a shorter step size, but this requires that all slaves support this
    functionality.  Otherwise, the only option is to terminate the simulation.

    A list of which slaves succeeded and which failed may be obtained by
    supplying an output vector via the `slaveResults` parameter.

    Note that general errors (e.g. network failure) are reported via exceptions,
    and are generally irrecoverable.

    \note
        Support for discarding and redoing time steps has not been implemented
        yet, so if the function returns STEP_FAILED, the only possible course
        of action is currently to terminate the simulation.

    \param [in] stepSize
        The time step size. Must be a positive number.
    \param [in] timeout
        The amount of time given to each slave for completing its calculations
        in this time step and reporting back to the master.  Note that if a
        slave fails to report back within this time, it is considered a fatal
        error which will cause the entire simulation to shut down.
        (Essentially, it is assumed that we have lost connection with the slave
        for some reason.) Must be a positive number.
    \param [in] slaveResults
        An optional vector which, if given, will be cleared and filled with
        the result reported by each slave.
    */
    StepResult Step(
        dsb::model::TimeDuration stepSize,
        std::chrono::milliseconds timeout,
        std::vector<std::pair<dsb::model::SlaveID, StepResult>>* slaveResults = nullptr);

    /**
    \brief  Accepts a time step, making the simulation ready to execute the next

    This function causes each slave to verify that it has received all its
    inputs for the time step just performed, and return to a state where it is
    ready to receive a new step command.

    The function may only be called if the previous step succeeded, and must
    be called before a new step can be taken.

    \param [in] timeout
        The amount of time given to each slave for confirming that it is ready
        for a new step.  Note that if a
        slave fails to report back within this time, it is considered a fatal
        error which will cause the entire simulation to shut down.
        (Essentially, it is assumed that we have lost connection with the slave
        for some reason.)
    */
    void AcceptStep(std::chrono::milliseconds timeout);

    /**
    \brief  Terminates the execution.

    This function will tell all participants to terminate, and then return
    immediately.  It does not (and can not) verify that the participants do
    in fact terminate.  Once all participants have been notified, the
    execution itself, and the thread it is running in, will terminate.

    No other methods may be called after a successful Terminate() call.
    */
    void Terminate();

private:
    class Private;
    std::unique_ptr<Private> m_private;
};


}}      //namespace
#endif  // header guard
