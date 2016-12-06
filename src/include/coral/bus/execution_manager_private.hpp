/**
\file
\brief  Defines the coral::bus::ExecutionManagerPrivate class
*/
#ifndef CORAL_BUS_EXECUTION_MANAGER_PRIVATE_HPP
#define CORAL_BUS_EXECUTION_MANAGER_PRIVATE_HPP

// For the sake of maintainability, we can skip the headers which are already
// included by execution_manager.hpp, and which are only needed here because
// ExecutionManagerPrivate duplicates ExecutionManager's method signatures.
#include <functional>
#include <map>
#include <memory>
#include <system_error>

#include "boost/noncopyable.hpp"

#include "coral/config.h"
#include "coral/model.hpp"
#include "coral/net.hpp"

#include "coral/bus/execution_manager.hpp"
#include "coral/bus/slave_controller.hpp"
#include "coral/bus/slave_setup.hpp"


namespace coral
{
namespace bus
{

class ExecutionState;


/**
\brief  Implementation class for coral::bus::ExecutionManager.

This class contains the internal data of the execution manager, including a
pointer to a coral::bus::ExecutionState object which performs the actions for
the current state.

The public data and functions in this class are available to the state objects
and maintained across different states.
*/
class ExecutionManagerPrivate : boost::noncopyable
{
public:
    ExecutionManagerPrivate(
        coral::net::Reactor& reactor,
        const std::string& executionName,
        const coral::master::ExecutionOptions& options);

    ~ExecutionManagerPrivate();

    // External methods, i.e. those that forward to state-specific objects.
    // =========================================================================
    void Reconstitute(
        const std::vector<AddedSlave>& slavesToAdd,
        std::chrono::milliseconds commTimeout,
        ExecutionManager::ReconstituteHandler onComplete,
        ExecutionManager::SlaveReconstituteHandler onSlaveComplete);

    void Reconfigure(
        const std::vector<SlaveConfig>& slaveConfigs,
        std::chrono::milliseconds commTimeout,
        ExecutionManager::ReconfigureHandler onComplete,
        ExecutionManager::SlaveReconfigureHandler onSlaveComplete);

    void Step(
        coral::model::TimeDuration stepSize,
        std::chrono::milliseconds timeout,
        ExecutionManager::StepHandler onComplete,
        ExecutionManager::SlaveStepHandler onSlaveStepComplete);

    void AcceptStep(
        std::chrono::milliseconds timeout,
        ExecutionManager::AcceptStepHandler onComplete,
        ExecutionManager::SlaveAcceptStepHandler onSlaveAcceptStepComplete);

    void Terminate();

    // Internal methods, i.e. those that are used by the state-specific objects.
    // =========================================================================

    // Performs the termination routine.
    // Terminatable states should simply forward their Terminate() method to
    // this function, possibly after doing state-specific cleanup.
    // This function will enter the TERMINATED state before its return, so
    // the calling state object (which will now be deleted) should not use its
    // member variables afterwards.
    void DoTerminate();

    // Functions for retrieving and updating the current simulation time and ID.
    coral::model::StepID NextStepID();
    coral::model::TimePoint CurrentSimTime() const;
    void AdvanceSimTime(coral::model::TimeDuration delta);

    // To be called when a per-slave operation has started and completed,
    // respectively.
    void SlaveOpStarted() CORAL_NOEXCEPT;
    void SlaveOpComplete();

    typedef std::function<void(const std::error_code&)> AllSlaveOpsCompleteHandler;

    /*
    Specifies an action to take when all ongoing per-slave operations are
    complete.

    If no per-slave operations are currently in progress, `handler` is called
    immediately.  If there are operations in progress, `handler` will be stored
    for later and called when they are all complete or when the execution enters
    a different state.  In the latter case, the handler will be called with
    error code coral::error::generic_error::aborted. In other words, the handler
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

    struct Slave
    {
        Slave(
            std::unique_ptr<coral::bus::SlaveController> slave,
            coral::net::SlaveLocator locator,
            const coral::model::SlaveDescription& description);

        Slave(const Slave&) = delete;
        Slave& operator=(const Slave&) = delete;

        CORAL_DEFINE_DEFAULT_MOVE(Slave, slave, locator, description)

        std::unique_ptr<coral::bus::SlaveController> slave;
        coral::net::SlaveLocator locator;
        coral::model::SlaveDescription description;
    };

    // Data which is available to the state objects
    coral::net::Reactor& reactor;
    coral::bus::SlaveSetup slaveSetup;
    coral::model::SlaveID lastSlaveID;
    std::map<coral::model::SlaveID, Slave> slaves;

private:
    // Make class nonmovable in addition to noncopyable, because we leak
    // pointers to it in lambda functions.
    ExecutionManagerPrivate(ExecutionManagerPrivate&&);
    ExecutionManagerPrivate& operator=(ExecutionManagerPrivate&&);

    // Performs the actual aborting of the "wait for all slave ops" thingy
    void AbortSlaveOpWaiting() CORAL_NOEXCEPT;

    // An object that represents, and performs the actions for, the current
    // execution state.
    std::unique_ptr<ExecutionState> m_state;

    // How many per-slave operations are currently in progress.
    int m_operationCount;

    // An action to take when all per-slave operations complete.
    // This is reset on every state change.
    AllSlaveOpsCompleteHandler m_allSlaveOpsCompleteHandler;

    // The ID of the time step currently in progress or just completed.
    coral::model::StepID m_currentStepID;
};


}} // namespace
#endif // header guard
