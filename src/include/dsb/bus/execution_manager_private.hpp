/**
\file
\brief  Defines the dsb::bus::ExecutionManagerPrivate class
*/
#ifndef DSB_BUS_EXECUTION_MANAGER_PRIVATE_HPP
#define DSB_BUS_EXECUTION_MANAGER_PRIVATE_HPP

// For the sake of maintainability, we can skip the headers which are already
// included by execution_manager.hpp, and which are only needed here because
// ExecutionManagerPrivate duplicates ExecutionManager's method signatures.
#include <functional>
#include <map>
#include <memory>
#include <system_error>

#include "boost/noncopyable.hpp"

#include "dsb/config.h"
#include "dsb/model.hpp"
#include "dsb/net.hpp"

#include "dsb/bus/execution_manager.hpp"
#include "dsb/bus/slave_controller.hpp"
#include "dsb/bus/slave_setup.hpp"


namespace dsb
{
namespace bus
{

class ExecutionState;


/**
\brief  Implementation class for dsb::bus::ExecutionManager.

This class contains the internal data of the execution manager, including a
pointer to a dsb::bus::ExecutionState object which performs the actions for
the current state.

The public data and functions in this class are available to the state objects
and maintained across different states.
*/
class ExecutionManagerPrivate : boost::noncopyable
{
public:
    ExecutionManagerPrivate(const dsb::net::ExecutionLocator& execLoc);

    ~ExecutionManagerPrivate();

    // External methods, i.e. those that forward to state-specific objects.
    // =========================================================================
    void Terminate();

    void BeginConfig(ExecutionManager::BeginConfigHandler onComplete);

    void EndConfig(ExecutionManager::EndConfigHandler onComplete);

    void SetSimulationTime(
        dsb::model::TimePoint startTime,
        dsb::model::TimePoint stopTime);

    dsb::model::SlaveID AddSlave(
        const dsb::net::SlaveLocator& slaveLocator,
        dsb::comm::Reactor& reactor,
        boost::chrono::milliseconds timeout,
        ExecutionManager::AddSlaveHandler onComplete);

    void SetVariables(
        dsb::model::SlaveID slave,
        const std::vector<dsb::model::VariableSetting>& settings,
        boost::chrono::milliseconds timeout,
        ExecutionManager::SetVariablesHandler onComplete);

    void Step(
        dsb::model::TimeDuration stepSize,
        boost::chrono::milliseconds timeout,
        ExecutionManager::StepHandler onComplete,
        ExecutionManager::SlaveStepHandler onSlaveStepComplete);

    void AcceptStep(
        boost::chrono::milliseconds timeout,
        ExecutionManager::AcceptStepHandler onComplete,
        ExecutionManager::SlaveAcceptStepHandler onSlaveAcceptStepComplete);

    // Internal methods, i.e. those that are used by the state-specific objects.
    // =========================================================================

    // Performs the termination routine.
    // Terminatable states should simply forward their Terminate() method to
    // this function, possibly after doing state-specific cleanup.
    // This function will enter the TERMINATED state before its return, so
    // the calling state object (which will now be deleted) should not use its
    // member variables afterwords.
    void DoTerminate();

    // Functions for retrieving and updating the current simulation time and ID.
    dsb::model::StepID NextStepID();
    dsb::model::TimePoint CurrentSimTime() const;
    void AdvanceSimTime(dsb::model::TimeDuration delta);

    // To be called when a per-slave operation has started and completed,
    // respectively.
    void SlaveOpStarted() DSB_NOEXCEPT;
    void SlaveOpComplete();
    
    typedef std::function<void(const std::error_code&)> AllSlaveOpsCompleteHandler;

    /*
    Specifies an action to take when all ongoing per-slave operations are
    complete.

    If no per-slave operations are currently in progress, `handler` is called
    immediately.  If there are operations in progress, `handler` will be stored
    for later and called when they are all complete or when the execution enters
    a different state.  In the latter case, the handler will be called with
    error code dsb::error::generic_error::aborted. In other words, the handler
    will only be called while the current state object is the active one.

    Once a completion handler has been set, this function may not be called
    again until the handler has been called.

    `handler` may not throw.
    */
    void WhenAllSlaveOpsComplete(AllSlaveOpsCompleteHandler handler);

    /*
    Switches to another state, and returns the current state object (for when
    the object needs to be kept alive a little bit more).
    */
    std::unique_ptr<ExecutionState> SwapState(std::unique_ptr<ExecutionState> next);

    // Data which is available to the state objects
    dsb::bus::SlaveSetup slaveSetup;
    dsb::model::SlaveID lastSlaveID;
    std::map<dsb::model::SlaveID, std::unique_ptr<dsb::bus::SlaveController>> slaves;

private:
    // Make class nonmovable in addition to noncopyable, because we leak
    // pointers to it in lambda functions.
    ExecutionManagerPrivate(ExecutionManagerPrivate&&);
    ExecutionManagerPrivate& operator=(ExecutionManagerPrivate&&);

    // Performs the actual aborting of the "wait for all slave ops" thingy
    void AbortSlaveOpWaiting() DSB_NOEXCEPT;

    // An object that represents, and performs the actions for, the current
    // execution state.
    std::unique_ptr<ExecutionState> m_state;

    // How many per-slave operations are currently in progress.
    int m_operationCount;

    // An action to take when all per-slave operations complete.
    // This is reset on every state change.
    AllSlaveOpsCompleteHandler m_allSlaveOpsCompleteHandler;

    // The ID of the time step currently in progress or just completed.
    dsb::model::StepID m_currentStepID;
};


}} // namespace
#endif // header guard
