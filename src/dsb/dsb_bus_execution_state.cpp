#include "dsb/bus/execution_state.hpp"

#include <iostream>
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


namespace
{
    void SendOk(zmq::socket_t& socket)
    {
        auto m = dsb::comm::ToFrame("OK");
        socket.send(m);
    }

    void SendFailed(zmq::socket_t& socket, const std::string& reason)
    {
        auto m0 = dsb::comm::ToFrame("FAILED");
        auto m1 = dsb::comm::ToFrame(reason);
        socket.send(m0, ZMQ_SNDMORE);
        socket.send(m1);
    }

    // This function handles an SET_SIMULATION_TIME call from the user.
    // It sends a (more or less) immediate reply, which is either OK or FAILED.
    // The latter happens if the start time is greater than the stop time, or
    // if the function is called after slaves have been added.
    void PerformSetSimulationTimeRPC(
        ExecutionAgentPrivate& self,
        std::deque<zmq::message_t>& msg,
        zmq::socket_t& userSocket)
    {
        assert (self.rpcInProgress == ExecutionAgentPrivate::NO_RPC
                && "Cannot perform SET_SIMULATION_TIME when another RPC is in progress");
        assert (msg.size() == 3
                && "SET_SIMULATION_TIME message must be exactly three frames");
        assert (dsb::comm::ToString(msg.front()) == "SET_SIMULATION_TIME"
                && "PerformSetSimulationTimeRPC() received non-SET_SIMULATION_TIME command");

        const auto startTime = dsb::comm::DecodeRawDataFrame<double>(msg[1]);
        const auto stopTime  = dsb::comm::DecodeRawDataFrame<double>(msg[2]);
        if (startTime > stopTime) {
            SendFailed(userSocket, "Attempted to set start time greater than stop time");
        } else if (!self.slaves.empty()) {
            SendFailed(userSocket, "Simulation time must be set before slaves are added");
        } else {
            self.startTime = startTime;
            self.stopTime  = stopTime;
            SendOk(userSocket);
        }
    }

    // This function handles an ADD_SLAVE call from the user.  It sends a (more or
    // less) immediate reply, which is either OK or FAILED.
    // The latter happens if the supplied slave ID already exists.
    void PerformAddSlaveRPC(
        ExecutionAgentPrivate& self,
        std::deque<zmq::message_t>& msg,
        zmq::socket_t& userSocket)
    {
        assert (self.rpcInProgress == ExecutionAgentPrivate::NO_RPC
                && "Cannot perform ADD_SLAVE when another RPC is in progress");
        assert (msg.size() == 2
                && "ADD_SLAVE message must be exactly two frames");
        assert (dsb::comm::ToString(msg.front()) == "ADD_SLAVE"
                && "PerformAddSlaveRPC() received non-ADD_SLAVE command");

        const auto slaveId = dsb::comm::DecodeRawDataFrame<uint16_t>(msg[1]);
        if (self.slaves.insert(std::make_pair(slaveId, dsb::bus::SlaveTracker(self.startTime, self.stopTime))).second) {
            SendOk(userSocket);
        } else {
            SendFailed(userSocket, "Slave already added");
        }
    }

    // This function handles a SET_VARS call from the user.  It sends a (more or
    // less) immediate reply, which is either OK or FAILED.
    // The latter happens if the supplied slave ID is invalid.  Any errors
    // reported by the slave in question are reported asynchronously, and not
    // handled by this function at all.
    void PerformSetVarsRPC(
        ExecutionAgentPrivate& self,
        std::deque<zmq::message_t>& msg,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket)
    {
        assert (self.rpcInProgress == ExecutionAgentPrivate::NO_RPC
                && "Cannot perform SET_VARS when another RPC is in progress");
        assert (msg.size() >= 2
                && "SET_VARS message must be at least two frames");
        assert (dsb::comm::ToString(msg.front()) == "SET_VARS"
                && "PerformSetVarsRPC() received non-SET_VARS command");

        const auto slaveId = dsb::comm::DecodeRawDataFrame<uint16_t>(msg[1]);
        auto it = self.slaves.find(slaveId);
        if (it == self.slaves.end()) {
            SendFailed(userSocket, "Invalid slave ID");
        } else {
            dsbproto::control::SetVarsData data;
            for (size_t i = 2; i < msg.size(); i+=2) {
                auto& newVar = *data.add_variable();
                newVar.set_id(dsb::comm::DecodeRawDataFrame<uint16_t>(msg[i]));
                newVar.mutable_value()->set_real_value(
                    dsb::comm::DecodeRawDataFrame<double>(msg[i+1]));
            }
            it->second.EnqueueSetVars(slaveSocket, data);
            SendOk(userSocket);
        }
    }

    // This function handles a CONNECT_VARS call from the user.  It sends a
    // (more or less) immediate reply, which is either OK or FAILED.
    // The latter happens if the supplied slave ID is invalid.  Any errors
    // reported by the slave in question are reported asynchronously, and not
    // handled by this function at all.
    void PerformConnectVarsRPC(
        ExecutionAgentPrivate& self,
        std::deque<zmq::message_t>& msg,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket)
    {
        assert (self.rpcInProgress == ExecutionAgentPrivate::NO_RPC
                && "Cannot perform CONNECT_VARS when another RPC is in progress");
        assert (msg.size() >= 2
                && "CONNECT_VARS message must be at least two frames");
        assert (dsb::comm::ToString(msg.front()) == "CONNECT_VARS"
                && "PerformConnectVarsRPC() received non-CONNECT_VARS command");

        const auto slaveId = dsb::comm::DecodeRawDataFrame<uint16_t>(msg[1]);
        auto it = self.slaves.find(slaveId);
        if (it == self.slaves.end()) {
            SendFailed(userSocket, "Invalid slave ID");
        } else {
            dsbproto::control::ConnectVarsData data;
            for (size_t i = 2; i < msg.size(); i+=3) {
                auto& newVar = *data.add_connection();
                newVar.set_input_var_id(dsb::comm::DecodeRawDataFrame<uint16_t>(msg[i]));
                newVar.mutable_output_var()->set_slave_id(
                    dsb::comm::DecodeRawDataFrame<uint16_t>(msg[i+1]));
                newVar.mutable_output_var()->set_var_id(
                    dsb::comm::DecodeRawDataFrame<uint16_t>(msg[i+2]));

                if (!self.slaves.count(newVar.output_var().slave_id())) {
                    SendFailed(userSocket, "Invalid slave ID in output variable specification");
                    return;
                }
            }
            it->second.EnqueueConnectVars(slaveSocket, data);
            SendOk(userSocket);
        }
    }
}


// =============================================================================
// Initializing
// =============================================================================

ExecutionInitializing::ExecutionInitializing() : m_waitingForReady(false) { }

void ExecutionInitializing::StateEntered(
    ExecutionAgentPrivate& self,
    zmq::socket_t& userSocket,
    zmq::socket_t& slaveSocket)
{
    // This assert may be removed in the future, if we add RPCs that may cross
    // into the "initialized" state.
    assert (self.rpcInProgress == ExecutionAgentPrivate::NO_RPC);
}

void ExecutionInitializing::UserMessage(
    ExecutionAgentPrivate& self,
    std::deque<zmq::message_t>& msg,
    zmq::socket_t& userSocket,
    zmq::socket_t& slaveSocket)
{
    assert (self.rpcInProgress == ExecutionAgentPrivate::NO_RPC);
    assert (!msg.empty());
    const auto msgType = dsb::comm::ToString(msg[0]);
    if (msgType == "SET_SIMULATION_TIME") {
        PerformSetSimulationTimeRPC(self, msg, userSocket);
    } else if (msgType == "SET_VARS") {
        PerformSetVarsRPC(self, msg, userSocket, slaveSocket);
    } else if (msgType == "CONNECT_VARS") {
        PerformConnectVarsRPC(self, msg, userSocket, slaveSocket);
    } else if (msgType == "WAIT_FOR_READY") {
        self.rpcInProgress = ExecutionAgentPrivate::WAIT_FOR_READY_RPC;
    } else if (msgType == "TERMINATE") {
        self.ChangeState<ExecutionTerminating>(userSocket, slaveSocket);
        SendOk(userSocket);
    } else if (msgType == "ADD_SLAVE") {
        PerformAddSlaveRPC(self, msg, userSocket);
    } else {
        assert (!"Invalid command received while execution is in 'initializing' state");
    }
}

void ExecutionInitializing::SlaveWaiting(
    ExecutionAgentPrivate& self,
    SlaveTracker& slaveHandler,
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
    ExecutionAgentPrivate& self,
    zmq::socket_t& userSocket,
    zmq::socket_t& slaveSocket)
{
    // Any RPC in progress will by definition have succeeded when this state is
    // reached.
    if (self.rpcInProgress != ExecutionAgentPrivate::NO_RPC) {
        assert (self.rpcInProgress == ExecutionAgentPrivate::WAIT_FOR_READY_RPC
                || self.rpcInProgress == ExecutionAgentPrivate::STEP_RPC);
        SendOk(userSocket);
        self.rpcInProgress = ExecutionAgentPrivate::NO_RPC;
    }
}

void ExecutionReady::UserMessage(
    ExecutionAgentPrivate& self,
    std::deque<zmq::message_t>& msg,
    zmq::socket_t& userSocket,
    zmq::socket_t& slaveSocket)
{
    assert (self.rpcInProgress == ExecutionAgentPrivate::NO_RPC);
    assert (!msg.empty());
    const auto msgType = dsb::comm::ToString(msg[0]);
    if (msgType == "STEP") {
        assert (msg.size() == 3);
        const auto time     = dsb::comm::DecodeRawDataFrame<double>(msg[1]);
        const auto stepSize = dsb::comm::DecodeRawDataFrame<double>(msg[2]);

        // TODO: Some checks we may want to insert here (and send FAIL if they
        // do not pass):
        //   - Is the time point of the first step equal to the start time?
        //   - Is the time point plus the step size greater than the stop time?
        //   - Is the time point equal to the previous time point plus the
        //     previous step size?
        //   - Have any slaves been added to the simulation?

        // Create the STEP message body
        dsbproto::control::StepData stepData;
        stepData.set_timepoint(time);
        stepData.set_stepsize(stepSize);
        BOOST_FOREACH(auto& slave, self.slaves) {
            slave.second.SendStep(slaveSocket, stepData);
        }
        self.rpcInProgress = ExecutionAgentPrivate::STEP_RPC;
        self.ChangeState<ExecutionStepping>(userSocket, slaveSocket);
    } else if (msgType == "TERMINATE") {
        self.ChangeState<ExecutionTerminating>(userSocket, slaveSocket);
        SendOk(userSocket);
    } else if (msgType == "ADD_SLAVE") {
        PerformAddSlaveRPC(self, msg, userSocket);
    } else if (msgType == "SET_VARS") {
        PerformSetVarsRPC(self, msg, userSocket, slaveSocket);
        self.ChangeState<ExecutionInitializing>(userSocket, slaveSocket);
    } else if (msgType == "CONNECT_VARS") {
        PerformConnectVarsRPC(self, msg, userSocket, slaveSocket);
        self.ChangeState<ExecutionInitializing>(userSocket, slaveSocket);
    } else if (msgType == "WAIT_FOR_READY") {
        SendOk(userSocket);
    } else {
        assert (!"Invalid command received while execution is in 'ready' state");
    }
}

void ExecutionReady::SlaveWaiting(
    ExecutionAgentPrivate& self,
    SlaveTracker& slaveHandler,
    zmq::socket_t& userSocket,
    zmq::socket_t& slaveSocket)
{
}

// =============================================================================
// Stepping
// =============================================================================

void ExecutionStepping::StateEntered(
    ExecutionAgentPrivate& self,
    zmq::socket_t& userSocket,
    zmq::socket_t& slaveSocket)
{
    assert (self.rpcInProgress == ExecutionAgentPrivate::STEP_RPC);
}

void ExecutionStepping::UserMessage(
    ExecutionAgentPrivate& self,
    std::deque<zmq::message_t>& msg,
    zmq::socket_t& userSocket,
    zmq::socket_t& slaveSocket)
{
    assert (false);
}

void ExecutionStepping::SlaveWaiting(
    ExecutionAgentPrivate& self,
    SlaveTracker& slaveHandler,
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
    ExecutionAgentPrivate& self,
    zmq::socket_t& userSocket,
    zmq::socket_t& slaveSocket)
{
    assert (self.rpcInProgress == ExecutionAgentPrivate::STEP_RPC);
    BOOST_FOREACH (auto& slave, self.slaves) {
        if (slave.second.IsSimulating()) {
            slave.second.SendRecvVars(slaveSocket);
        } else assert (false);
    }
}

void ExecutionPublished::UserMessage(
    ExecutionAgentPrivate& self,
    std::deque<zmq::message_t>& msg,
    zmq::socket_t& userSocket,
    zmq::socket_t& slaveSocket)
{
    assert (false);
}

void ExecutionPublished::SlaveWaiting(
    ExecutionAgentPrivate& self,
    SlaveTracker& slaveHandler,
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

// TODO: Shut down ExecutionAgent too!

ExecutionTerminating::ExecutionTerminating()
{
}

void ExecutionTerminating::StateEntered(
    ExecutionAgentPrivate& self,
    zmq::socket_t& userSocket,
    zmq::socket_t& slaveSocket)
{
    BOOST_FOREACH (auto& slave, self.slaves) {
        if (slave.second.State() & TERMINATABLE_STATES) {
            slave.second.SendTerminate(slaveSocket);
        }
    }
}

void ExecutionTerminating::UserMessage(
    ExecutionAgentPrivate& self,
    std::deque<zmq::message_t>& msg,
    zmq::socket_t& userSocket,
    zmq::socket_t& slaveSocket)
{
    assert (false);
}

void ExecutionTerminating::SlaveWaiting(
    ExecutionAgentPrivate& self,
    SlaveTracker& slaveHandler,
    zmq::socket_t& userSocket,
    zmq::socket_t& slaveSocket)
{
    assert (slaveHandler.State() & TERMINATABLE_STATES);
    slaveHandler.SendTerminate(slaveSocket);
}


}} // namespace
