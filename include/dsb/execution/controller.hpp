/**
\file
\brief Functionality for starting and controlling an execution.
*/
#ifndef DSB_EXECUTION_CONTROLLER_HPP
#define DSB_EXECUTION_CONTROLLER_HPP

#include <chrono>
#include <future>
#include <iterator>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "dsb/config.h"
#include "dsb/model.hpp"
#include "dsb/net.hpp"


// Forward declaration to avoid dependency on ZMQ headers
namespace zmq { class socket_t; }


namespace dsb
{
namespace execution
{


enum StepResult
{
    STEP_FAILED     = 0,
    STEP_COMPLETE   = 1
};


/**
\brief  Master execution controller.

This class represents the master entity in an execution, and is used to
initialize, run and shut down the simulation.
Use SpawnExecution() to spawn an execution, and then create a Controller object
for it.

Objects of this class are movable, but not copyable, thus ensuring that exactly
one Controller object is attached to an execution at any time.

This class is actually just a frontend for a background thread in which all
communication with the slaves takes place.  If the background thread dies due
to an unexpected error, `std::terminate()` will be called to terminate the
whole program, as this is (for the time being) an unrecoverable situation.
An error message will be printed to the standard error stream with a
description of the failure.
In the future, this may be handled better.
*/
class Controller
{
public:
    /// Constructor.
    explicit Controller(const dsb::net::ExecutionLocator& locator);

    /// Move constructor.
    Controller(Controller&& other) DSB_NOEXCEPT;

    /// Move assignment operator.
    Controller& operator=(Controller&& other) DSB_NOEXCEPT;

    /// Destructor which terminates the simulation.
    ~Controller();

    /**
    \brief  Terminates the execution.

    This function will tell all participants to terminate, and then return
    immediately.  It does not (and can not) verify that the participants do
    in fact terminate.  Once all participants have been notified, the execution
    itself, and the thread it is running in, will terminate.

    No other methods may be called after a successful Terminate() call.
    */
    void Terminate();

    /**
    \brief  Enters configuration mode.

    In configuration mode, slaves may be added and connections may be made.
    */
    void BeginConfig();

    /**
    \brief  Leaves configuration mode and enters simulation mode.

    Note that it is possible to leave configuration mode even if a previous
    AddSlave() or SetVariables() call failed.  Use the returned futures from
    those functions to detect any errors.
    */
    void EndConfig();

    /**
    \brief  Sets the start time and, optionally, the stop time of the
            simulation.

    This function must be called in configuration mode, before any AddSlave()
    calls are made.  The reason is that this information must be transmitted
    to the slaves as they connect and initialize, to allow them to report
    whether their model is valid within the given boundaries and/or to allocate
    memory for their internal state.

    If this function is never called, a default start time of 0.0 and an
    undefined stop time is used.

    \param [in] startTime
        The start time of the simulation.
    \param [in] stopTime
        The stop time of the simulation.  This may be dsb::model::ETERNITY
        (the default), signifying that no particular stop time is defined.
        Otherwise, it must be greater than `startTime`.
    */
    void SetSimulationTime(
        dsb::model::TimePoint startTime,
        dsb::model::TimePoint stopTime = dsb::model::ETERNITY);

    /**
    \brief  Adds a slave to the execution.

    This function will connect to the slave whose network location is given
    by `slaveLocator` and assign a numeric ID to it.  This happens
    asynchronously, meaning that this function returns immediately so that
    other slaves may be added in parallel.

    The function returns a `std::future` object which, upon success, will
    refer to the ID of the added slave.  If the connection fails for some
    reason, an exception will be associated with the future instead.

    \param [in] slaveLocator
        Information by which the slave may be located.
    \param [in] commTimeout
        The maximum time to wait for replies for commands sent to the slave.
        If this time is exceeded, the operation is considered to have failed.

    \returns a future slave ID, which is used to refer to this slave in other
        functions.
    */
    std::future<dsb::model::SlaveID> AddSlave(
        dsb::net::SlaveLocator slaveLocator,
        std::chrono::milliseconds commTimeout);

    /**
    \brief  Sets the values of and/or connects one or more of a slave's
            variables.

    `VariableValueRange` may be any type for which std::begin and std::end
    yield a pair of iterators that can be used to access a sequence of elements
    of type dsb::model::VariableSetting (e.g. a container).

    This function returns a `std::future` object which does not contain any
    value, but which may be queried to verify that the operation succeeded.
    If it failed, `std::future::get()` will throw an exception.

    \param [in] slave
        The ID of a slave which is part of the execution (i.e., has been
        successfully added with AddSlave()).
    \param [in] variableSettings
        Variable values and connections.
    \param [in] timeout
        The maximum time to wait for a confirmation from the slave.
        If this time is exceeded, the operation is considered to have failed.

    \throws std::logic_error if the ID does not correspond to a slave which
        is part of this execution.
    */
    template<typename VariableSettingRange>
    std::future<void> SetVariables(
        dsb::model::SlaveID slave,
        const VariableSettingRange& variableSettings,
        std::chrono::milliseconds timeout);

    // Specialisation of the above for std::vector.
    std::future<void> SetVariables(
        dsb::model::SlaveID slave,
        const std::vector<dsb::model::VariableSetting>& variableSettings,
        std::chrono::milliseconds timeout);

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

private:
    // NOTE: When adding members here, remember to update the move constructor
    // and the move assignment operator!
    std::unique_ptr<zmq::socket_t> m_rpcSocket;
    bool m_active;
    std::thread m_thread;
};


/**
\brief  Spawns a new execution.

This will start a new execution on the domain, and return a locator which can
be used to connect to it.

\param [in] domainLocator   The domain in which the execution should run.
\param [in] executionName   A unique name for the execution.
\param [in] commTimeout     A time after which the execution broker and the
                            slaves should self-terminate if communication is
                            lost.

\throws std::invalid_argument if commTimeout is nonpositive.
*/
dsb::net::ExecutionLocator SpawnExecution(
    const dsb::net::DomainLocator& domainLocator,
    const std::string& executionName = std::string(),
    std::chrono::seconds commTimeout = std::chrono::seconds(3600));


// =============================================================================
// Template definitions
// =============================================================================

template<typename VariableSettingRange>
std::future<void> Controller::SetVariables(
    dsb::model::SlaveID slave,
    const VariableSettingRange& variableSettings,
    std::chrono::milliseconds timeout)
{
    return SetVariables(
        slave,
        std::vector<dsb::model::VariableSetting>(begin(variableSettings), end(variableSettings)),
        timeout);
}


}}      //namespace
#endif  // header guard
