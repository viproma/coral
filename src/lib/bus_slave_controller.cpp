#include "coral/bus/slave_controller.hpp"

#include <cassert>
#include <utility>
#include "coral/error.hpp"


namespace coral
{
namespace bus
{


SlaveController::SlaveController(
    coral::net::Reactor& reactor,
    const coral::net::SlaveLocator& slaveLocator,
    coral::model::SlaveID slaveID,
    const std::string& slaveName,
    const SlaveSetup& setup,
    std::chrono::milliseconds timeout,
    ConnectHandler onComplete,
    int maxConnectionAttempts)
{
    CORAL_INPUT_CHECK(slaveID != coral::model::INVALID_SLAVE_ID);
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
                    slaveName,
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


SlaveState SlaveController::State() const CORAL_NOEXCEPT
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
            coral::model::SlaveDescription());
    }
}


void SlaveController::SetVariables(
    const std::vector<coral::model::VariableSetting>& settings,
    std::chrono::milliseconds timeout,
    SetVariablesHandler onComplete)
{
    CORAL_INPUT_CHECK(!settings.empty());
    if (m_messenger) {
        m_messenger->SetVariables(settings, timeout, std::move(onComplete));
    } else {
        onComplete(std::make_error_code(std::errc::not_connected));
    }
}


void SlaveController::SetPeers(
    const std::vector<coral::net::Endpoint>& peers,
    std::chrono::milliseconds timeout,
    SetPeersHandler onComplete)
{
    if (m_messenger) {
        m_messenger->SetPeers(peers, timeout, std::move(onComplete));
    } else {
        onComplete(std::make_error_code(std::errc::not_connected));
    }
}


void SlaveController::Prime(
    std::chrono::milliseconds timeout,
    PrimeHandler onComplete)
{
    if (m_messenger) {
        m_messenger->Prime(timeout, std::move(onComplete));
    } else {
        onComplete(std::make_error_code(std::errc::not_connected));
    }
}


void SlaveController::Step(
    coral::model::StepID stepID,
    coral::model::TimePoint currentT,
    coral::model::TimeDuration deltaT,
    std::chrono::milliseconds timeout,
    StepHandler onComplete)
{
    CORAL_INPUT_CHECK(deltaT >= 0.0);
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
