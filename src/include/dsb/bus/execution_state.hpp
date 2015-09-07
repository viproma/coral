/**
\file
\brief  Defines the dsb::bus::ExecutionState class, along with its subclasses
        which represent the various states of an execution.
*/
#ifndef DSB_BUS_EXECUTION_STATE_HPP
#define DSB_BUS_EXECUTION_STATE_HPP

// For the sake of maintainability, we can skip the headers which are already
// included by execution_manager.hpp, and which are only needed here because
// ExecutionState duplicates ExecutionManager's method signatures.
#include "dsb/bus/execution_manager.hpp"
#include "dsb/config.h"
#include "dsb/error.hpp"


namespace dsb
{
namespace bus
{


/**
\brief  The superclass of all classes that represent execution states.

By default, all functions throw a dsb::error::PreconditionViolation when
called.  All supported functions for a given state must therefore be overridden
in the corresponding subclass.
*/
class ExecutionState
{
public:
    virtual void StateEntered(
        ExecutionManagerPrivate& self)
    { }

    virtual void Terminate(ExecutionManagerPrivate& self)
    { NotAllowed(__FUNCTION__); }

    virtual void BeginConfig(
        ExecutionManagerPrivate& self,
        ExecutionManager::BeginConfigHandler onComplete)
    { NotAllowed(__FUNCTION__); }

    virtual void EndConfig(
        ExecutionManagerPrivate& self,
        ExecutionManager::BeginConfigHandler onComplete)
    { NotAllowed(__FUNCTION__); }

    virtual void SetSimulationTime(
        ExecutionManagerPrivate& self,
        dsb::model::TimePoint startTime,
        dsb::model::TimePoint stopTime)
    { NotAllowed(__FUNCTION__); }

    virtual dsb::model::SlaveID AddSlave(
        ExecutionManagerPrivate& self,
        const dsb::net::SlaveLocator& slaveLocator,
        dsb::comm::Reactor& reactor,
        boost::chrono::milliseconds timeout,
        ExecutionManager::AddSlaveHandler onComplete)
    { NotAllowed(__FUNCTION__); }

    virtual void SetVariables(
        ExecutionManagerPrivate& self,
        dsb::model::SlaveID slave,
        const std::vector<dsb::model::VariableSetting>& settings,
        boost::chrono::milliseconds timeout,
        ExecutionManager::SetVariablesHandler onComplete)
    { NotAllowed(__FUNCTION__); }

    virtual void Step(
        ExecutionManagerPrivate& self,
        dsb::model::TimeDuration stepSize,
        boost::chrono::milliseconds timeout,
        ExecutionManager::StepHandler onComplete,
        ExecutionManager::SlaveStepHandler onSlaveStepComplete)
    { NotAllowed(__FUNCTION__); }

    virtual void AcceptStep(
        ExecutionManagerPrivate& self,
        boost::chrono::milliseconds timeout,
        ExecutionManager::AcceptStepHandler onComplete,
        ExecutionManager::SlaveAcceptStepHandler onSlaveAcceptStepComplete)
    { NotAllowed(__FUNCTION__); }

    virtual ~ExecutionState() DSB_NOEXCEPT { }

private:
    DSB_NORETURN void NotAllowed(const std::string& func) const
    {
        throw dsb::error::PreconditionViolation(
            func + ": Method call not allowed in present state");
    }
};


class ConfigExecutionState : public ExecutionState
{
    void Terminate(ExecutionManagerPrivate& self) override;

    void BeginConfig(
        ExecutionManagerPrivate& self,
        ExecutionManager::BeginConfigHandler onComplete) override;

    void EndConfig(
        ExecutionManagerPrivate& self,
        ExecutionManager::BeginConfigHandler onComplete) override;

    void SetSimulationTime(
        ExecutionManagerPrivate& self,
        dsb::model::TimePoint startTime,
        dsb::model::TimePoint stopTime) override;

    dsb::model::SlaveID AddSlave(
        ExecutionManagerPrivate& self,
        const dsb::net::SlaveLocator& slaveLocator,
        dsb::comm::Reactor& reactor,
        boost::chrono::milliseconds timeout,
        ExecutionManager::AddSlaveHandler onComplete) override;

    void SetVariables(
        ExecutionManagerPrivate& self,
        dsb::model::SlaveID slave,
        const std::vector<dsb::model::VariableSetting>& settings,
        boost::chrono::milliseconds timeout,
        ExecutionManager::SetVariablesHandler onComplete) override;
};


// This class is so named in anticipation of issue VIPROMA-46 getting fixed.
class PrimingExecutionState : public ExecutionState
{
public:
    PrimingExecutionState(ExecutionManager::EndConfigHandler onComplete);

private:
    void StateEntered(ExecutionManagerPrivate& self) override;

    ExecutionManager::EndConfigHandler m_onComplete;
};


class ReadyExecutionState : public ExecutionState
{
    void Terminate(ExecutionManagerPrivate& self) override;

    void BeginConfig(
        ExecutionManagerPrivate& self,
        ExecutionManager::BeginConfigHandler onComplete) override;

    void Step(
        ExecutionManagerPrivate& self,
        dsb::model::TimeDuration stepSize,
        boost::chrono::milliseconds timeout,
        ExecutionManager::StepHandler onComplete,
        ExecutionManager::SlaveStepHandler onSlaveStepComplete) override;
};


class SteppingExecutionState : public ExecutionState
{
public:
    SteppingExecutionState(
        dsb::model::TimeDuration stepSize,
        boost::chrono::milliseconds timeout,
        ExecutionManager::StepHandler onComplete,
        ExecutionManager::SlaveStepHandler onSlaveStepComplete);

private:
    void StateEntered(ExecutionManagerPrivate& self) override;

    const dsb::model::TimeDuration m_stepSize;
    boost::chrono::milliseconds m_timeout;
    ExecutionManager::StepHandler m_onComplete;
    ExecutionManager::SlaveStepHandler m_onSlaveStepComplete;
};


class StepOkExecutionState : public ExecutionState
{
public:
    StepOkExecutionState(dsb::model::TimeDuration stepSize);

private:
    void Terminate(ExecutionManagerPrivate& self) override;

    void AcceptStep(
        ExecutionManagerPrivate& self,
        boost::chrono::milliseconds timeout,
        ExecutionManager::AcceptStepHandler onComplete,
        ExecutionManager::SlaveAcceptStepHandler onSlaveAcceptStepComplete)
            override;

    const dsb::model::TimeDuration m_stepSize;
};


class AcceptingExecutionState : public ExecutionState
{
public:
    AcceptingExecutionState(
        boost::chrono::milliseconds timeout,
        ExecutionManager::AcceptStepHandler onComplete,
        ExecutionManager::SlaveAcceptStepHandler onSlaveAcceptStepComplete);

private:
    void StateEntered(ExecutionManagerPrivate& self) override;

    boost::chrono::milliseconds m_timeout;
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
