#include "dsb/bus/execution_agent.hpp"

#include <iostream> // TEMPORARY
#include <limits>
#include "dsb/comm.hpp"
#include "dsb/control.hpp"
#include "dsb/util.hpp"


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


ExecutionAgentPrivate::ExecutionAgentPrivate()
    : startTime(0.0),
      stopTime(std::numeric_limits<double>::infinity()),
      rpcInProgress(NO_RPC)
{ }


void ExecutionAgentPrivate::UpdateState()
{
    if (m_nextState) {
        m_state = std::move(m_nextState);
    }
}



ExecutionAgent::ExecutionAgent(
    zmq::socket_t& userSocket,
    zmq::socket_t& slaveSocket)
{
    m_data.ChangeState<ExecutionInitializing>(userSocket, slaveSocket);
    m_data.UpdateState();
}


void ExecutionAgent::UserMessage(
    std::deque<zmq::message_t>& msg,
    zmq::socket_t& userSocket,
    zmq::socket_t& slaveSocket)
{
    m_data.m_state->UserMessage(m_data, msg, userSocket, slaveSocket);
    m_data.UpdateState();
}


void ExecutionAgent::SlaveMessage(
    std::deque<zmq::message_t>& msg,
    zmq::socket_t& userSocket,
    zmq::socket_t& slaveSocket)
{
    std::deque<zmq::message_t> envelope;
    dsb::comm::PopMessageEnvelope(msg, &envelope);

    // The last frame of envelope must be a 16-bit integer, i.e. the slave's
    // ID number.  If not, the message does not appear to come from a DSB
    // participant, and we just ignore it altogether.
    assert (!envelope.empty());
    if (envelope.back().size() != 2) return;
    const auto slaveId = dsb::util::DecodeUint16(
        reinterpret_cast<char*>(envelope.back().data()));

    std::clog << "Received message from slave '" << slaveId << "': ";

    // Pass on the message to the appropriate slave handler, send the
    // reply immediately if necessary.
    auto slaveHandler = m_data.slaves.find(slaveId);
    if (slaveHandler != m_data.slaves.end()) {
        if (!slaveHandler->second.RequestReply(slaveSocket, envelope, msg)) {
            m_data.m_state->SlaveWaiting(m_data, slaveHandler->second,
                                         userSocket, slaveSocket);
            m_data.UpdateState();
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


}} // namespace
