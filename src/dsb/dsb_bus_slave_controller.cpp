#include "dsb/bus/slave_controller.hpp"

#include <cassert>
#include <utility>
#include "dsb/error.hpp"


namespace dsb
{
namespace bus
{


SlaveController::SlaveController(
    dsb::comm::Reactor& reactor,
    const dsb::net::SlaveLocator& slaveLocator,
    dsb::model::SlaveID slaveID,
    const SlaveSetup& setup,
    std::chrono::milliseconds timeout,
    ConnectHandler onComplete,
    int maxConnectionAttempts)
{
    DSB_INPUT_CHECK(slaveID != dsb::model::INVALID_SLAVE_ID);
    m_pendingConnection = ConnectToSlave(
        reactor,
        slaveLocator,
        maxConnectionAttempts,
        timeout,
        [=] (const std::error_code& ec, SlaveControlConnection scc) {
            if (!ec) {
                m_messenger = MakeSlaveControlMessenger(
                    std::move(scc),
                    slaveID,
                    setup,
                    onComplete);
            } else {
                onComplete(ec);
            }
        });
}


SlaveController::~SlaveController()
{
    // Intentionally does nothing.
    // We just need it to be able to use SlaveControlMessenger as an undefined
    // type in the header.
}


void SlaveController::Close()
{
    m_pendingConnection.Close();
    if (m_messenger) m_messenger->Close();
}


SlaveState SlaveController::State() const DSB_NOEXCEPT
{
    if (m_messenger) return m_messenger->State();
    else if (m_pendingConnection) return SLAVE_BUSY;
    else return SLAVE_NOT_CONNECTED;
}


void SlaveController::GetDescription(
    std::chrono::milliseconds timeout,
    GetDescriptionHandler onComplete)
{
    if (m_messenger) {
        m_messenger->GetDescription(timeout, std::move(onComplete));
    } else {
        onComplete(
            std::make_error_code(std::errc::not_connected),
            dsb::model::SlaveDescription());
    }
}


void SlaveController::SetVariables(
    const std::vector<dsb::model::VariableSetting>& settings,
    std::chrono::milliseconds timeout,
    SetVariablesHandler onComplete)
{
    DSB_INPUT_CHECK(!settings.empty());
    if (m_messenger) {
        m_messenger->SetVariables(settings, timeout, std::move(onComplete));
    } else {
        onComplete(std::make_error_code(std::errc::not_connected));
    }
}


void SlaveController::Step(
    dsb::model::StepID stepID,
    dsb::model::TimePoint currentT,
    dsb::model::TimeDuration deltaT,
    std::chrono::milliseconds timeout,
    StepHandler onComplete)
{
    DSB_INPUT_CHECK(deltaT > 0.0);
    if (m_messenger) {
        m_messenger->Step(stepID, currentT, deltaT, timeout, std::move(onComplete));
    } else {
        onComplete(std::make_error_code(std::errc::not_connected));
    }
}

void SlaveController::AcceptStep(
    std::chrono::milliseconds timeout,
    AcceptStepHandler onComplete)
{
    if (m_messenger) {
        m_messenger->AcceptStep(timeout, std::move(onComplete));
    } else {
        onComplete(std::make_error_code(std::errc::not_connected));
    }
}


void SlaveController::Terminate()
{
    m_pendingConnection.Close();
    if (m_messenger) {
        m_messenger->Terminate();
    }
}


}} // namespace
