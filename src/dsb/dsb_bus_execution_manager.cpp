#include "dsb/bus/execution_manager.hpp"

#include "dsb/bus/execution_manager_private.hpp"


namespace dsb
{
namespace bus
{


ExecutionManager::ExecutionManager(
    dsb::comm::Reactor& reactor,
    const std::string& executionName,
    dsb::model::TimePoint startTime,
    dsb::model::TimePoint maxTime)
    : m_private(std::make_unique<ExecutionManagerPrivate>(
        reactor, executionName, startTime, maxTime))
{
}


ExecutionManager::~ExecutionManager()
{
    // For the moment, the destructor does nothing.  We just need it to be able
    // to use std::unique_ptr (for m_private) with an undefined type (i.e.,
    // ExecutionManagerPrivate) in the header.
}


void ExecutionManager::Reconstitute(
    const std::vector<AddedSlave>& slavesToAdd,
    std::chrono::milliseconds commTimeout,
    ReconstituteHandler onComplete,
    SlaveReconstituteHandler onSlaveComplete)
{
    m_private->Reconstitute(
        slavesToAdd, commTimeout,
        std::move(onComplete), std::move(onSlaveComplete));
}


void ExecutionManager::Reconfigure(
    const std::vector<SlaveConfig>& slaveConfigs,
    std::chrono::milliseconds commTimeout,
    ReconfigureHandler onComplete,
    SlaveReconfigureHandler onSlaveComplete)
{
    m_private->Reconfigure(
        slaveConfigs, commTimeout,
        std::move(onComplete), std::move(onSlaveComplete));
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


void ExecutionManager::Terminate()
{
    m_private->Terminate();
}


const std::string& ExecutionManager::SlaveName(dsb::model::SlaveID id) const
{
    return m_private->slaves.at(id).description.Name();
}


}} // namespace
