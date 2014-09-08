#include "dsb/bus/execution_agent.hpp"

#include <iostream> // TEMPORARY
#include "dsb/comm.hpp"
#include "dsb/control.hpp"


namespace dsb
{
namespace bus
{

// ExecutionAgent implementation note:
//
// This class models the state machine of an execution using the "state pattern"
// (https://en.wikipedia.org/wiki/State_pattern).  The functions in the class
// do very little work themselves, and mainly forward incoming messages to an
// object of type ExecutionState which represents the current state of the
// execution.


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
    auto slaveHandler = slaves.find(slaveId);
    if (slaveHandler != slaves.end()) {
        if (!slaveHandler->second.RequestReply(slaveSocket, envelope, msg)) {
            m_state->SlaveWaiting(*this, slaveHandler->second, userSocket, slaveSocket);
            UpdateState();
        }
    } else {
        std::clog << "Unauthorized slave detected" << std::endl;
        std::deque<zmq::message_t> errMsg;
        dsb::control::CreateDeniedMessage(
            errMsg,
            "Participant not in list of expected slaves");
        dsb::comm::AddressedSend(slaveSocket, envelope, errMsg);
    }
}


void ExecutionAgent::UpdateState()
{
    if (m_nextState) {
        m_state = std::move(m_nextState);
    }
}


}} // namespace
