#include "coral/bus/variable_io.hpp"

#include <utility>
#include "zmq.hpp"

#include "coral/error.hpp"
#include "coral/log.hpp"
#include "coral/net/zmqx.hpp"
#include "coral/protocol/exe_data.hpp"


namespace coral
{
namespace bus
{

namespace
{
    void EnforceConnected(const std::unique_ptr<zmq::socket_t>& s, bool state)
    {
        if (!s == state) {
            throw coral::error::PreconditionViolation(
                state ? "Not connected" : "Already connected");
        }
    }
}


// =============================================================================
// class VariablePublisher
// =============================================================================

VariablePublisher::VariablePublisher()
{ }


void VariablePublisher::Bind(const coral::net::Endpoint& endpoint)
{
    EnforceConnected(m_socket, false);
    m_socket = std::make_unique<zmq::socket_t>(coral::net::zmqx::GlobalContext(), ZMQ_PUB);
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


coral::net::Endpoint VariablePublisher::BoundEndpoint() const
{
    EnforceConnected(m_socket, true);
    return coral::net::Endpoint{coral::net::zmqx::LastEndpoint(*m_socket)};
}


void VariablePublisher::Publish(
    coral::model::StepID stepID,
    coral::model::SlaveID slaveID,
    coral::model::VariableID variableID,
    coral::model::ScalarValue value)
{
    EnforceConnected(m_socket, true);
    coral::protocol::exe_data::Message m = {
        coral::model::Variable(slaveID, variableID),
        stepID,
        value
    };
    std::vector<zmq::message_t> d;
    coral::protocol::exe_data::CreateMessage(m, d);
    coral::net::zmqx::Send(*m_socket, d);
}


// =============================================================================
// class VariableSubscriber
// =============================================================================


VariableSubscriber::VariableSubscriber()
    : m_currentStepID(coral::model::INVALID_STEP_ID)
{ }


void VariableSubscriber::Connect(
    const coral::net::Endpoint* endpoints,
    std::size_t endpointsSize)
{
    m_socket = std::make_unique<zmq::socket_t>(coral::net::zmqx::GlobalContext(), ZMQ_SUB);
    try {
        m_socket->setsockopt(ZMQ_SNDHWM, 0);
        m_socket->setsockopt(ZMQ_RCVHWM, 0);
        m_socket->setsockopt(ZMQ_LINGER, 0);
        for (std::size_t i = 0; i < endpointsSize; ++i) {
            m_socket->connect(endpoints[i].URL().c_str());
        }
        for (const auto& variable : m_values) {
            coral::protocol::exe_data::Subscribe(*m_socket, variable.first);
        }
    } catch (...) {
        m_socket.reset();
        throw;
    }
}


void VariableSubscriber::Subscribe(const coral::model::Variable& variable)
{
    EnforceConnected(m_socket, true);
    coral::protocol::exe_data::Subscribe(*m_socket, variable);
    m_values.insert(std::make_pair(variable, ValueQueue()));
}


void VariableSubscriber::Unsubscribe(const coral::model::Variable& variable)
{
    EnforceConnected(m_socket, true);
    if (m_values.erase(variable)) {
        coral::protocol::exe_data::Unsubscribe(*m_socket, variable);
    }
}


bool VariableSubscriber::Update(
    coral::model::StepID stepID,
    std::chrono::milliseconds timeout)
{
    CORAL_PRECONDITION_CHECK(stepID >= m_currentStepID);
    m_currentStepID = stepID;

    std::vector<zmq::message_t> rawMsg;
    for (auto& entry : m_values) {
        auto& valQueue = entry.second;
        // Pop off old data
        while (!valQueue.empty() && valQueue.front().first < m_currentStepID) {
            valQueue.pop();
        }
        // If necessary, wait for new data
        while (valQueue.empty()) {
            if (!coral::net::zmqx::WaitForIncoming(*m_socket, timeout)) {
                CORAL_LOG_DEBUG(
                    boost::format("Timeout waiting for variable %d from slave %d")
                    % entry.first.ID() % entry.first.Slave());
                return false;
            }
            coral::net::zmqx::Receive(*m_socket, rawMsg);
            const auto msg = coral::protocol::exe_data::ParseMessage(rawMsg);
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
    return true;
}


const coral::model::ScalarValue& VariableSubscriber::Value(
   const coral::model::Variable& variable) const
{
    const auto& valQueue = m_values.at(variable);
    if (valQueue.empty()) {
        throw std::logic_error("Variable not updated yet");
    }
    return valQueue.front().second;
}

}} // header guard
