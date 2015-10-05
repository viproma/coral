#define NOMINMAX
#include "dsb/bus/execution_state.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>

#include "dsb/bus/execution_manager_private.hpp"
#include "dsb/bus/slave_control_messenger.hpp"
#include "dsb/bus/slave_controller.hpp"
#include "dsb/compat_helpers.hpp"
#include "dsb/util.hpp"


namespace dsb
{
namespace bus
{


// =============================================================================


void ConfigExecutionState::Terminate(ExecutionManagerPrivate& self)
{
    self.DoTerminate();
}


void ConfigExecutionState::BeginConfig(
    ExecutionManagerPrivate& /*self*/,
    ExecutionManager::BeginConfigHandler onComplete)
{
    // Do nothing, we're already here.
    onComplete(std::error_code());
}


void ConfigExecutionState::EndConfig(
    ExecutionManagerPrivate& self,
    ExecutionManager::BeginConfigHandler onComplete)
{
    self.SwapState(
        std::make_unique<PrimingExecutionState>(std::move(onComplete)));
}


void ConfigExecutionState::SetSimulationTime(
    ExecutionManagerPrivate& self,
    dsb::model::TimePoint startTime,
    dsb::model::TimePoint stopTime)
{
    DSB_PRECONDITION_CHECK(self.slaves.empty());
    DSB_INPUT_CHECK(startTime <= stopTime);
    self.slaveSetup.startTime = startTime;
    self.slaveSetup.stopTime = stopTime;
}


// NOTE:
// None of the per-slave operation completion handlers in the CONFIG state
// should capture the 'this' pointer of the ConfigExecutionState object.
// The reason is that if the user calls EndConfig() while operations are still
// pending, the operations will not complete in this state, but in the PRIMING
// state.  Hence, the object will have been deleted and replaced with a
// PrimingExecutionState object.

dsb::model::SlaveID ConfigExecutionState::AddSlave(
    ExecutionManagerPrivate& self,
    const dsb::net::SlaveLocator& slaveLocator,
    dsb::comm::Reactor& reactor,
    boost::chrono::milliseconds timeout,
    ExecutionManager::AddSlaveHandler onComplete)
{
    DSB_INPUT_CHECK(!!onComplete);
    if (self.lastSlaveID == std::numeric_limits<dsb::model::SlaveID>::max()) {
        throw std::length_error("Maximum number of slaves reached");
    }
    const auto id = ++self.lastSlaveID;
    const auto selfPtr = &self;
    self.slaves.insert(std::make_pair(id, std::make_unique<dsb::bus::SlaveController>(
        reactor,
        slaveLocator,
        id,
        self.slaveSetup,
        timeout,
        [id, onComplete, selfPtr] (const std::error_code& ec) {
            if (!ec) {
                onComplete(std::error_code(), id);
            } else {
                selfPtr->slaves[id]->Close();
                selfPtr->slaves.erase(id);
                onComplete(ec, dsb::model::INVALID_SLAVE_ID);
            }
            selfPtr->SlaveOpComplete();
        })));
    selfPtr->SlaveOpStarted();
    return id;
}


void ConfigExecutionState::SetVariables(
    ExecutionManagerPrivate& self,
    dsb::model::SlaveID slave,
    const std::vector<dsb::model::VariableSetting>& settings,
    boost::chrono::milliseconds timeout,
    ExecutionManager::SetVariablesHandler onComplete)
{
    const auto selfPtr = &self;
    self.slaves.at(slave)->SetVariables(
        settings,
        timeout,
        [onComplete, selfPtr](const std::error_code& ec) {
            const auto onExit = dsb::util::OnScopeExit([=]() {
                selfPtr->SlaveOpComplete();
            });
            onComplete(ec);
        });
    self.SlaveOpStarted();
}


// =============================================================================


PrimingExecutionState::PrimingExecutionState(
    ExecutionManager::EndConfigHandler onComplete)
    : m_onComplete(std::move(onComplete))
{
}


void PrimingExecutionState::StateEntered(ExecutionManagerPrivate& self)
{
    const auto selfPtr = &self;
    self.WhenAllSlaveOpsComplete([selfPtr, this] (const std::error_code& ec) {
        assert(!ec);
        const auto keepMeAlive = selfPtr->SwapState(std::make_unique<ReadyExecutionState>());
        assert(keepMeAlive.get() == this);
        dsb::util::MoveAndCall(m_onComplete, std::error_code());
   });
}


// =============================================================================


void ReadyExecutionState::Terminate(ExecutionManagerPrivate& self)
{
    self.DoTerminate();
}


void ReadyExecutionState::BeginConfig(
    ExecutionManagerPrivate& self,
    ExecutionManager::BeginConfigHandler onComplete)
{
    self.SwapState(std::make_unique<ConfigExecutionState>());
    onComplete(std::error_code());
}


void ReadyExecutionState::Step(
    ExecutionManagerPrivate& self,
    dsb::model::TimeDuration stepSize,
    boost::chrono::milliseconds timeout,
    ExecutionManager::StepHandler onComplete,
    ExecutionManager::SlaveStepHandler onSlaveStepComplete)
{
    self.SwapState(std::make_unique<SteppingExecutionState>(
        stepSize, timeout, std::move(onComplete), std::move(onSlaveStepComplete)));
}


// =============================================================================


SteppingExecutionState::SteppingExecutionState(
    dsb::model::TimeDuration stepSize,
    boost::chrono::milliseconds timeout,
    ExecutionManager::StepHandler onComplete,
    ExecutionManager::SlaveStepHandler onSlaveStepComplete)
    : m_stepSize(stepSize),
      m_timeout(timeout),
      m_onComplete(std::move(onComplete)),
      m_onSlaveStepComplete(std::move(onSlaveStepComplete))
{
}


void SteppingExecutionState::StateEntered(ExecutionManagerPrivate& self)
{
    const auto selfPtr = &self; // Because we can't capture references in lambdas
    for (auto it = begin(self.slaves); it != end(self.slaves); ++it) {
        const auto slaveID = it->first;
        it->second->Step(
            self.CurrentSimTime(),
            m_stepSize,
            m_timeout,
            [selfPtr, slaveID, this] (const std::error_code& ec) {
                const auto onExit = dsb::util::OnScopeExit([=]() {
                    selfPtr->SlaveOpComplete();
                });
                if (m_onSlaveStepComplete) m_onSlaveStepComplete(ec, slaveID);
            });
        self.SlaveOpStarted();
    }
    self.WhenAllSlaveOpsComplete([selfPtr, this] (const std::error_code& ec) {
        assert(!ec);
        bool stepFailed = false;
        bool fatalError = false;
        for (auto it = begin(selfPtr->slaves); it != end(selfPtr->slaves); ++it) {
            if (it->second->State() == SLAVE_STEP_OK) {
                // do nothing
            } else if (it->second->State() == SLAVE_STEP_FAILED) {
                stepFailed = true;
            } else {
                assert(it->second->State() == SLAVE_NOT_CONNECTED);
                fatalError = true;
                break; // because there's no point in continuing
            }
        }
        if (fatalError) {
            const auto keepMeAlive = selfPtr->SwapState(
                std::make_unique<FatalErrorExecutionState>());
            assert(keepMeAlive.get() == this);
            m_onComplete(make_error_code(dsb::error::generic_error::operation_failed));
        } else if (stepFailed) {
            const auto keepMeAlive = selfPtr->SwapState(
                std::make_unique<StepFailedExecutionState>());
            assert(keepMeAlive.get() == this);
            m_onComplete(dsb::error::sim_error::cannot_perform_timestep);
        } else {
            const auto keepMeAlive = selfPtr->SwapState(
                std::make_unique<StepOkExecutionState>(m_stepSize));
            assert(keepMeAlive.get() == this);
            m_onComplete(std::error_code());
        }
    });           
}


// =============================================================================


StepOkExecutionState::StepOkExecutionState(dsb::model::TimeDuration stepSize)
    : m_stepSize(stepSize)
{
}


void StepOkExecutionState::Terminate(ExecutionManagerPrivate& self)
{
    self.DoTerminate();
}


void StepOkExecutionState::AcceptStep(
    ExecutionManagerPrivate& self,
    boost::chrono::milliseconds timeout,
    ExecutionManager::AcceptStepHandler onComplete,
    ExecutionManager::SlaveAcceptStepHandler onSlaveAcceptStepComplete)
{
    self.AdvanceSimTime(m_stepSize);
    self.SwapState(std::make_unique<AcceptingExecutionState>(
        timeout, std::move(onComplete), std::move(onSlaveAcceptStepComplete)));
}


// =============================================================================


AcceptingExecutionState::AcceptingExecutionState(
    boost::chrono::milliseconds timeout,
    ExecutionManager::AcceptStepHandler onComplete,
    ExecutionManager::SlaveAcceptStepHandler onSlaveAcceptStepComplete)
    : m_timeout(timeout),
      m_onComplete(std::move(onComplete)),
      m_onSlaveAcceptStepComplete(std::move(onSlaveAcceptStepComplete))
{
}


void AcceptingExecutionState::StateEntered(ExecutionManagerPrivate& self)
{
    const auto selfPtr = &self; // Because we can't capture references in lambdas
    for (auto it = begin(self.slaves); it != end(self.slaves); ++it) {
        const auto slaveID = it->first;
        it->second->AcceptStep(
            m_timeout,
            [selfPtr, slaveID, this] (const std::error_code& ec) {
                const auto onExit = dsb::util::OnScopeExit([=]() {
                    selfPtr->SlaveOpComplete();
                });
                if (m_onSlaveAcceptStepComplete) {
                    m_onSlaveAcceptStepComplete(ec, slaveID);
                }
            });
        self.SlaveOpStarted();
    }
    self.WhenAllSlaveOpsComplete([selfPtr, this] (const std::error_code& ec) {
        assert(!ec);
        bool error = false;
        for (auto it = begin(selfPtr->slaves); it != end(selfPtr->slaves); ++it) {
            if (it->second->State() != SLAVE_READY) {
                assert(it->second->State() == SLAVE_NOT_CONNECTED);
                error = true;
                break;
            }
        }
        if (error) {
            const auto keepMeAlive = selfPtr->SwapState(
                std::make_unique<FatalErrorExecutionState>());
            assert(keepMeAlive.get() == this);
            m_onComplete(make_error_code(dsb::error::generic_error::operation_failed));
            return;
        } else {
            const auto keepMeAlive = selfPtr->SwapState(
                std::make_unique<ReadyExecutionState>());
            assert(keepMeAlive.get() == this);
            m_onComplete(std::error_code());
        }
    });
}


// =============================================================================


void StepFailedExecutionState::Terminate(ExecutionManagerPrivate& self)
{
    self.DoTerminate();
}


// =============================================================================


void FatalErrorExecutionState::Terminate(ExecutionManagerPrivate& self)
{
    self.DoTerminate();
}


// =============================================================================


void TerminatedExecutionState::Terminate(ExecutionManagerPrivate& self)
{
    // Do nothing, we're already here.
}


}} // namespace
