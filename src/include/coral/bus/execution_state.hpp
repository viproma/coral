/**
\file
\brief  Defines the coral::bus::ExecutionState class, along with its subclasses
        which represent the various states of an execution.
*/
#ifndef CORAL_BUS_EXECUTION_STATE_HPP
#define CORAL_BUS_EXECUTION_STATE_HPP

// For the sake of maintainability, we can skip the headers which are already
// included by execution_manager.hpp, and which are only needed here because
// ExecutionState duplicates ExecutionManager's method signatures.
#include "coral/bus/execution_manager.hpp"
#include "coral/config.h"
#include "coral/error.hpp"


namespace coral
{
namespace bus
{


/**
\brief  The superclass of all classes that represent execution states.

By default, all functions throw a coral::error::PreconditionViolation when
called.  All supported functions for a given state must therefore be overridden
in the corresponding subclass.
*/
class ExecutionState
{
public:
    virtual void StateEntered(
        ExecutionManagerPrivate& self)
    { }

    virtual void Reconstitute(
        ExecutionManagerPrivate& self,
        const std::vector<AddedSlave>& slavesToAdd,
        std::chrono::milliseconds commTimeout,
        ExecutionManager::ReconstituteHandler onComplete,
        ExecutionManager::SlaveReconstituteHandler onSlaveComplete)
    { NotAllowed(__FUNCTION__); }

    virtual void Reconfigure(
        ExecutionManagerPrivate& self,
        const std::vector<SlaveConfig>& slaveConfigs,
        std::chrono::milliseconds commTimeout,
        ExecutionManager::ReconstituteHandler onComplete,
        ExecutionManager::SlaveReconstituteHandler onSlaveComplete)
    { NotAllowed(__FUNCTION__); }

    virtual void Prime(
        ExecutionManagerPrivate& self,
        int maxAttempts,
        std::chrono::milliseconds commTimeout,
        std::function<void(const std::error_code&)> onComplete)
    { NotAllowed(__FUNCTION__); }

    virtual void Step(
        ExecutionManagerPrivate& self,
        coral::model::TimeDuration stepSize,
        std::chrono::milliseconds timeout,
        ExecutionManager::StepHandler onComplete,
        ExecutionManager::SlaveStepHandler onSlaveStepComplete)
    { NotAllowed(__FUNCTION__); }

    virtual void AcceptStep(
        ExecutionManagerPrivate& self,
        std::chrono::milliseconds timeout,
        ExecutionManager::AcceptStepHandler onComplete,
        ExecutionManager::SlaveAcceptStepHandler onSlaveAcceptStepComplete)
    { NotAllowed(__FUNCTION__); }

    virtual void Terminate(ExecutionManagerPrivate& self)
    { NotAllowed(__FUNCTION__); }

    virtual ~ExecutionState() CORAL_NOEXCEPT { }

private:
    CORAL_NORETURN void NotAllowed(const std::string& func) const
    {
        throw coral::error::PreconditionViolation(
            func + ": Method call not allowed in present state");
    }
};


class ReadyExecutionState : public ExecutionState
{
    void Reconstitute(
        ExecutionManagerPrivate& self,
        const std::vector<AddedSlave>& slavesToAdd,
        std::chrono::milliseconds commTimeout,
        ExecutionManager::ReconstituteHandler onComplete,
        ExecutionManager::SlaveReconstituteHandler onSlaveComplete) override;

    void Reconfigure(
        ExecutionManagerPrivate& self,
        const std::vector<SlaveConfig>& slaveConfigs,
        std::chrono::milliseconds commTimeout,
        ExecutionManager::ReconstituteHandler onComplete,
        ExecutionManager::SlaveReconstituteHandler onSlaveComplete) override;

    void Prime(
        ExecutionManagerPrivate& self,
        int maxAttempts,
        std::chrono::milliseconds commTimeout,
        std::function<void(const std::error_code&)> onComplete) override;

    void Step(
        ExecutionManagerPrivate& self,
        coral::model::TimeDuration stepSize,
        std::chrono::milliseconds timeout,
        ExecutionManager::StepHandler onComplete,
        ExecutionManager::SlaveStepHandler onSlaveStepComplete) override;

    void Terminate(ExecutionManagerPrivate& self) override;
};


class ReconstitutingExecutionState : public ExecutionState
{
public:
    ReconstitutingExecutionState(
        const std::vector<AddedSlave>& slavesToAdd,
        std::chrono::milliseconds commTimeout,
        ExecutionManager::ReconstituteHandler onComplete,
        ExecutionManager::SlaveReconstituteHandler onSlaveComplete);

private:
    void StateEntered(ExecutionManagerPrivate& self) override;
    void AllSlavesAdded(ExecutionManagerPrivate& self);
    void Completed(ExecutionManagerPrivate& self);
    void Failed(ExecutionManagerPrivate& self);

    // Input parameters to this state
    const std::vector<AddedSlave> m_slavesToAdd;
    const std::chrono::milliseconds m_commTimeout;
    const ExecutionManager::ReconstituteHandler m_onComplete;
    const ExecutionManager::SlaveReconstituteHandler m_onSlaveComplete;

    // Local variables of this state
    std::vector<coral::model::SlaveID> m_addedSlaves;
};


class ReconfiguringExecutionState : public ExecutionState
{
public:
    ReconfiguringExecutionState(
        const std::vector<SlaveConfig>& slaveConfigs,
        std::chrono::milliseconds commTimeout,
        ExecutionManager::ReconstituteHandler onComplete,
        ExecutionManager::SlaveReconstituteHandler onSlaveComplete);

private:
    void StateEntered(ExecutionManagerPrivate& self) override;

    // Input parameters to this state
    const std::vector<SlaveConfig> m_slaveConfigs;
    const std::chrono::milliseconds m_commTimeout;
    const ExecutionManager::ReconstituteHandler m_onComplete;
    const ExecutionManager::SlaveReconstituteHandler m_onSlaveComplete;
};


class PrimingExecutionState: public ExecutionState
{
public:
    PrimingExecutionState(
        int maxAttempts,
        std::chrono::milliseconds commTimeout,
        std::function<void(const std::error_code&)> onComplete);

private:
    void StateEntered(ExecutionManagerPrivate& self) override;
    void Failed(ExecutionManagerPrivate& self);

    void Try(ExecutionManagerPrivate& self, int attemptsLeft);
    void Fail(ExecutionManagerPrivate& self, const std::error_code& ec);
    void Succeed(ExecutionManagerPrivate& self);

    // Input parameters to this state
    const int m_maxAttempts;
    const std::chrono::milliseconds m_commTimeout;
    const std::function<void(const std::error_code&)> m_onComplete;
};


class SteppingExecutionState : public ExecutionState
{
public:
    SteppingExecutionState(
        coral::model::TimeDuration stepSize,
        std::chrono::milliseconds timeout,
        ExecutionManager::StepHandler onComplete,
        ExecutionManager::SlaveStepHandler onSlaveStepComplete);

private:
    void StateEntered(ExecutionManagerPrivate& self) override;

    const coral::model::TimeDuration m_stepSize;
    std::chrono::milliseconds m_timeout;
    ExecutionManager::StepHandler m_onComplete;
    ExecutionManager::SlaveStepHandler m_onSlaveStepComplete;
};


class StepOkExecutionState : public ExecutionState
{
public:
    StepOkExecutionState(coral::model::TimeDuration stepSize);

private:
    void Terminate(ExecutionManagerPrivate& self) override;

    void AcceptStep(
        ExecutionManagerPrivate& self,
        std::chrono::milliseconds timeout,
        ExecutionManager::AcceptStepHandler onComplete,
        ExecutionManager::SlaveAcceptStepHandler onSlaveAcceptStepComplete)
            override;

    const coral::model::TimeDuration m_stepSize;
};


class AcceptingExecutionState : public ExecutionState
{
public:
    AcceptingExecutionState(
        std::chrono::milliseconds timeout,
        ExecutionManager::AcceptStepHandler onComplete,
        ExecutionManager::SlaveAcceptStepHandler onSlaveAcceptStepComplete);

private:
    void StateEntered(ExecutionManagerPrivate& self) override;

    std::chrono::milliseconds m_timeout;
    ExecutionManager::AcceptStepHandler m_onComplete;
    ExecutionManager::SlaveAcceptStepHandler m_onSlaveAcceptStepComplete;
};


class StepFailedExecutionState : public ExecutionState
{
    void Terminate(ExecutionManagerPrivate& self) override;
};


class FatalErrorExecutionState : public ExecutionState
{
    void Terminate(ExecutionManagerPrivate& self) override;
};


class TerminatedExecutionState : public ExecutionState
{
    void Terminate(ExecutionManagerPrivate& self) override;
};


}} // namespace
#endif // header guard
