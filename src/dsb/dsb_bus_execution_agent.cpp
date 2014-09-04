#include "dsb/bus/execution_agent.hpp"

#include <iostream> // TEMPORARY
#include "dsb/comm.hpp"


namespace dsb
{
namespace bus
{



ExecutionAgent::ExecutionAgent(
    zmq::socket_t& userSocket,
    zmq::socket_t& slaveSocket)
{
    ChangeState<ExecutionInitializing>(userSocket, slaveSocket);
    UpdateState();
}

void ExecutionAgent::UserMessage(
    std::deque<zmq::message_t>& msg,
    zmq::socket_t& userSocket,
    zmq::socket_t& slaveSocket)
{
    m_state->UserMessage(*this, msg, userSocket, slaveSocket);
    UpdateState();
}

void ExecutionAgent::SlaveMessage(
    std::deque<zmq::message_t>& msg,
    zmq::socket_t& userSocket,
    zmq::socket_t& slaveSocket)
{
    std::deque<zmq::message_t> envelope;
    dsb::comm::PopMessageEnvelope(msg, &envelope);
    const auto slaveId = dsb::comm::ToString(envelope.back());
    std::clog << "Received message from slave '" << slaveId << "': ";

    // Pass on the message to the appropriate slave handler, send the
    // reply immediately if necessary.
    auto& slaveHandler = slaves.at(slaveId);
    if (!slaveHandler.RequestReply(slaveSocket, envelope, msg)) {
        m_state->SlaveWaiting(*this, slaveHandler, msg, userSocket, slaveSocket);
        UpdateState();
    }
}


void ExecutionAgent::UpdateState()
{
    if (m_nextState) {
        m_state = std::move(m_nextState);
    }
}


}} // namespace
