#include "dsb/execution/variable_io.hpp"

#include <utility>
#include "zmq.hpp"

#include "dsb/net/messaging.hpp"
#include "dsb/net/util.hpp"
#include "dsb/error.hpp"
#include "dsb/protocol/exe_data.hpp"


namespace dsb
{
namespace execution
{

namespace
{
    void EnforceConnected(const std::unique_ptr<zmq::socket_t>& s, bool state)
    {
        if (!s == state) {
            throw dsb::error::PreconditionViolation(
                state ? "Not connected" : "Already connected");
        }
    }
}


// =============================================================================
// class VariablePublisher
// =============================================================================

VariablePublisher::VariablePublisher()
{ }


void VariablePublisher::Bind(const dsb::net::Endpoint& endpoint)
{
    EnforceConnected(m_socket, false);
    m_socket = std::make_unique<zmq::socket_t>(dsb::net::GlobalContext(), ZMQ_PUB);
    try {
        m_socket->setsockopt(ZMQ_SNDHWM, 0);
        m_socket->setsockopt(ZMQ_RCVHWM, 0);
        m_socket->setsockopt(ZMQ_LINGER, 0);
        m_socket->bind(endpoint.URL().c_str());
    } catch (...) {
        m_socket.reset();
        throw;
    }
}


dsb::net::Endpoint VariablePublisher::BoundEndpoint() const
{
    EnforceConnected(m_socket, true);
    return dsb::net::Endpoint{dsb::net::LastEndpoint(*m_socket)};
}


void VariablePublisher::Publish(
    dsb::model::StepID stepID,
    dsb::model::SlaveID slaveID,
    dsb::model::VariableID variableID,
    dsb::model::ScalarValue value)
{
    EnforceConnected(m_socket, true);
    dsb::protocol::exe_data::Message m = {
        dsb::model::Variable(slaveID, variableID),
        stepID,
        value
    };
    std::vector<zmq::message_t> d;
    dsb::protocol::exe_data::CreateMessage(m, d);
    dsb::net::Send(*m_socket, d);
}


// =============================================================================
// class VariableSubscriber
// =============================================================================


VariableSubscriber::VariableSubscriber()
    : m_currentStepID(dsb::model::INVALID_STEP_ID)
{ }


void VariableSubscriber::Connect(
    const dsb::net::Endpoint* endpoints,
    std::size_t endpointsSize)
{
    m_socket = std::make_unique<zmq::socket_t>(dsb::net::GlobalContext(), ZMQ_SUB);
    try {
        m_socket->setsockopt(ZMQ_SNDHWM, 0);
        m_socket->setsockopt(ZMQ_RCVHWM, 0);
        m_socket->setsockopt(ZMQ_LINGER, 0);
        for (std::size_t i = 0; i < endpointsSize; ++i) {
            m_socket->connect(endpoints[i].URL().c_str());
        }
        for (const auto& variable : m_values) {
            dsb::protocol::exe_data::Subscribe(*m_socket, variable.first);
        }
    } catch (...) {
        m_socket.reset();
        throw;
    }
}


void VariableSubscriber::Subscribe(const dsb::model::Variable& variable)
{
    EnforceConnected(m_socket, true);
    dsb::protocol::exe_data::Subscribe(*m_socket, variable);
    m_values.insert(std::make_pair(variable, ValueQueue()));
}


void VariableSubscriber::Unsubscribe(const dsb::model::Variable& variable)
{
    EnforceConnected(m_socket, true);
    if (m_values.erase(variable)) {
        dsb::protocol::exe_data::Unsubscribe(*m_socket, variable);
    }
}


void VariableSubscriber::Update(
    dsb::model::StepID stepID,
    std::chrono::milliseconds timeout)
{
    DSB_PRECONDITION_CHECK(stepID >= m_currentStepID);
    m_currentStepID = stepID;

    std::vector<zmq::message_t> rawMsg;
    for (auto& entry : m_values) {
        const auto& var = entry.first;
        auto& valQueue = entry.second;
        // Pop off old data
        while (!valQueue.empty() && valQueue.front().first < m_currentStepID) {
            valQueue.pop();
        }
        // If necessary, wait for new data
        while (valQueue.empty()) {
            if (!dsb::net::WaitForIncoming(*m_socket, timeout)) {
                // TODO: Create dedicated exception type
                throw std::runtime_error(
                    "Timeout waiting for variable " + std::to_string(var.ID())
                    + " from slave " + std::to_string(var.Slave()));
            }
            dsb::net::Receive(*m_socket, rawMsg);
            const auto msg = dsb::protocol::exe_data::ParseMessage(rawMsg);
            // Queue the variable value iff it is from the current (or a newer)
            // timestep and it is one we're listening for. (Wrt. the latter,
            // unsubscriptions may take time to come into effect.)
            if (msg.timestepID >= m_currentStepID) {
                auto it = m_values.find(msg.variable);
                if (it != m_values.end()) {
                    it->second.emplace(msg.timestepID, msg.value);
                }
            }
        }
    }
}


const dsb::model::ScalarValue& VariableSubscriber::Value(
   const dsb::model::Variable& variable) const
{
    const auto& valQueue = m_values.at(variable);
    if (valQueue.empty()) {
        throw std::logic_error("Variable not updated yet");
    }
    return valQueue.front().second;
}

}} // header guard
