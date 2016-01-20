#include "dsb/bus/execution_manager.hpp"

#include "dsb/bus/execution_manager_private.hpp"


namespace dsb
{
namespace bus
{


ExecutionManager::ExecutionManager(const dsb::net::ExecutionLocator& execLoc)
    : m_private(std::make_unique<ExecutionManagerPrivate>(execLoc))
{
}


ExecutionManager::~ExecutionManager()
{
    // For the moment, the destructor does nothing.  We just need it to be able
    // to use std::unique_ptr (for m_private) with an undefined type (i.e.,
    // ExecutionManagerPrivate) in the header.
}


void ExecutionManager::BeginConfig(BeginConfigHandler onComplete)
{
    m_private->BeginConfig(std::move(onComplete));
}


void ExecutionManager::EndConfig(BeginConfigHandler onComplete)
{
    m_private->EndConfig(std::move(onComplete));
}


void ExecutionManager::Terminate()
{
    m_private->Terminate();
}


void ExecutionManager::SetSimulationTime(
    dsb::model::TimePoint startTime,
    dsb::model::TimePoint stopTime)
{
    m_private->SetSimulationTime(startTime, stopTime);
}


dsb::model::SlaveID ExecutionManager::AddSlave(
    const dsb::net::SlaveLocator& slaveLocator,
    const std::string& slaveName,
    dsb::comm::Reactor& reactor,
    std::chrono::milliseconds commTimeout,
    AddSlaveHandler onComplete)
{
    return m_private->AddSlave(
        slaveLocator, slaveName, reactor, commTimeout, std::move(onComplete));
}


void ExecutionManager::SetVariables(
    dsb::model::SlaveID slave,
    const std::vector<dsb::model::VariableSetting>& settings,
    std::chrono::milliseconds timeout,
    SetVariablesHandler onComplete)
{
    m_private->SetVariables(slave, settings, timeout, std::move(onComplete));
}


void ExecutionManager::Step(
    dsb::model::TimeDuration stepSize,
    std::chrono::milliseconds timeout,
    StepHandler onComplete,
    ExecutionManager::SlaveStepHandler onSlaveStepComplete)
{
    m_private->Step(
        stepSize,
        timeout,
        std::move(onComplete),
        std::move(onSlaveStepComplete));
}


void ExecutionManager::AcceptStep(
    std::chrono::milliseconds timeout,
    AcceptStepHandler onComplete,
    SlaveAcceptStepHandler onSlaveAcceptStepComplete)
{
    m_private->AcceptStep(
        timeout,
        std::move(onComplete),
        std::move(onSlaveAcceptStepComplete));
}


const std::string& ExecutionManager::SlaveName(dsb::model::SlaveID id) const
{
    return m_private->slaves.at(id).name;
}


}} // namespace
