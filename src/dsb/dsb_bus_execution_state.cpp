#include "dsb/bus/execution_state.hpp"

#include "boost/foreach.hpp"

#include "dsb/bus/execution_agent.hpp"
#include "dsb/bus/slave_tracker.hpp"
#include "dsb/comm.hpp"
#include "dsb/control.hpp"
#include "control.pb.h"


namespace dsb
{
namespace bus
{


// =============================================================================
// Initializing
// =============================================================================

void ExecutionInitializing::StateEntered(
    ExecutionAgent& self,
    zmq::socket_t& userSocket,
    zmq::socket_t& slaveSocket)
{
}

void ExecutionInitializing::UserMessage(
    ExecutionAgent& self,
    std::deque<zmq::message_t>& msg,
    zmq::socket_t& userSocket,
    zmq::socket_t& slaveSocket)
{
    //  if message is SET_VARS
    //      store variable values/connections
    assert (false);
}

void ExecutionInitializing::SlaveWaiting(
    ExecutionAgent& self,
    SlaveTracker& slaveHandler,
    std::deque<zmq::message_t>& msg,
    zmq::socket_t& userSocket,
    zmq::socket_t& slaveSocket)
{
    // Check whether all slaves are Ready, and if so, switch to Ready state.
    bool allReady = true;
    BOOST_FOREACH (const auto& slave, self.slaves) {
        if (slave.second.State() != SLAVE_READY) allReady = false;
    }
    if (allReady) self.ChangeState<ExecutionReady>(userSocket, slaveSocket);
}

// =============================================================================
// Ready
// =============================================================================

void ExecutionReady::StateEntered(
    ExecutionAgent& self,
    zmq::socket_t& userSocket,
    zmq::socket_t& slaveSocket)
{
    userSocket.send(dsb::comm::ToFrame("ALL_READY"));
}

void ExecutionReady::UserMessage(
    ExecutionAgent& self,
    std::deque<zmq::message_t>& msg,
    zmq::socket_t& userSocket,
    zmq::socket_t& slaveSocket)
{
    assert (!msg.empty());
    const auto msgType = dsb::comm::ToString(msg[0]);
    if (msgType == "STEP") {
        assert (msg.size() == 3);
        const auto time     = dsb::comm::DecodeRawDataFrame<double>(msg[1]);
        const auto stepSize = dsb::comm::DecodeRawDataFrame<double>(msg[2]);

        // Create the STEP message body
        dsbproto::control::StepData stepData;
        stepData.set_timepoint(time);
        stepData.set_stepsize(stepSize);
        // Create the multipart STEP message
        std::deque<zmq::message_t> stepMsg;
        dsb::control::CreateMessage(stepMsg, dsbproto::control::MSG_STEP, stepData);

        // For each slave, make a copy of the STEP message and send it.
        BOOST_FOREACH(auto& slave, self.slaves) {
            std::deque<zmq::message_t> stepMsgCopy;
            dsb::comm::CopyMessage(stepMsg, stepMsgCopy);
            slave.second.SendStep(slaveSocket, stepMsgCopy);
        }
        self.ChangeState<ExecutionStepping>(userSocket, slaveSocket);
    } else if (msgType == "TERMINATE") {
        self.ChangeState<ExecutionTerminating>(userSocket, slaveSocket);
    }
    //  else if message is SET_VARS
    //      send SET_VARS to appropriate slave
    //      go back to Init state
    else assert (false);
}

void ExecutionReady::SlaveWaiting(
    ExecutionAgent& self,
    SlaveTracker& slaveHandler,
    std::deque<zmq::message_t>& msg,
    zmq::socket_t& userSocket,
    zmq::socket_t& slaveSocket)
{
}

// =============================================================================
// Stepping
// =============================================================================

void ExecutionStepping::StateEntered(
    ExecutionAgent& self,
    zmq::socket_t& userSocket,
    zmq::socket_t& slaveSocket)
{
}

void ExecutionStepping::UserMessage(
    ExecutionAgent& self,
    std::deque<zmq::message_t>& msg,
    zmq::socket_t& userSocket,
    zmq::socket_t& slaveSocket)
{
    assert (false);
}

void ExecutionStepping::SlaveWaiting(
    ExecutionAgent& self,
    SlaveTracker& slaveHandler,
    std::deque<zmq::message_t>& msg,
    zmq::socket_t& userSocket,
    zmq::socket_t& slaveSocket)
{
    bool allPublished = true;
    BOOST_FOREACH (const auto& slave, self.slaves) {
        if (slave.second.IsSimulating() && slave.second.State() != SLAVE_PUBLISHED) {
            allPublished = false;
        }
    }
    if (allPublished) {
        self.ChangeState<ExecutionPublished>(userSocket, slaveSocket);
    }
}

// =============================================================================
// Published
// =============================================================================

void ExecutionPublished::StateEntered(
    ExecutionAgent& self,
    zmq::socket_t& userSocket,
    zmq::socket_t& slaveSocket)
{
    // Create RECV_VARS message
    std::deque<zmq::message_t> recvVarsMsg;
    dsb::control::CreateMessage(recvVarsMsg, dsbproto::control::MSG_RECV_VARS);
    // Send this to all slaves which are in simulation
    BOOST_FOREACH (auto& slave, self.slaves) {
        if (slave.second.IsSimulating()) {
            // Make a copy of the message and send it
            std::deque<zmq::message_t> recvVarsMsgCopy;
            dsb::comm::CopyMessage(recvVarsMsg, recvVarsMsgCopy);
            slave.second.SendRecvVars(slaveSocket, recvVarsMsgCopy);
        }
    }
}

void ExecutionPublished::UserMessage(
    ExecutionAgent& self,
    std::deque<zmq::message_t>& msg,
    zmq::socket_t& userSocket,
    zmq::socket_t& slaveSocket)
{
    assert (!msg.empty());
    if (dsb::comm::ToString(msg.front()) == "TERMINATE") {
        self.ChangeState<ExecutionTerminating>(userSocket, slaveSocket);
    } else assert (false);
}

void ExecutionPublished::SlaveWaiting(
    ExecutionAgent& self,
    SlaveTracker& slaveHandler,
    std::deque<zmq::message_t>& msg,
    zmq::socket_t& userSocket,
    zmq::socket_t& slaveSocket)
{
    // Check whether all slaves are Ready, and if so, switch to Ready state.
    bool allReady = true;
    BOOST_FOREACH (const auto& slave, self.slaves) {
        if (slave.second.State() != SLAVE_READY) allReady = false;
    }
    if (allReady) self.ChangeState<ExecutionReady>(userSocket, slaveSocket);
}

// =============================================================================
// Terminating
// =============================================================================

ExecutionTerminating::ExecutionTerminating()
{
    dsb::control::CreateMessage(m_termMsg, dsbproto::control::MSG_TERMINATE);
}

void ExecutionTerminating::StateEntered(
    ExecutionAgent& self,
    zmq::socket_t& userSocket,
    zmq::socket_t& slaveSocket)
{
    BOOST_FOREACH (auto& slave, self.slaves) {
        if (slave.second.State() & TERMINATABLE_STATES) {
            std::deque<zmq::message_t> termMsgCopy;
            dsb::comm::CopyMessage(m_termMsg, termMsgCopy);
            slave.second.SendTerminate(slaveSocket, termMsgCopy);
        }
    }
}

void ExecutionTerminating::UserMessage(
    ExecutionAgent& self,
    std::deque<zmq::message_t>& msg,
    zmq::socket_t& userSocket,
    zmq::socket_t& slaveSocket)
{
    assert (false);
}

void ExecutionTerminating::SlaveWaiting(
    ExecutionAgent& self,
    SlaveTracker& slaveHandler,
    std::deque<zmq::message_t>& msg,
    zmq::socket_t& userSocket,
    zmq::socket_t& slaveSocket)
{
    assert (slaveHandler.State() & TERMINATABLE_STATES);
            
    std::deque<zmq::message_t> termMsgCopy;
    dsb::comm::CopyMessage(m_termMsg, termMsgCopy);
    slaveHandler.SendTerminate(slaveSocket, termMsgCopy);
}


}} // namespace
