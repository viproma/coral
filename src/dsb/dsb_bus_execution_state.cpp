#include "dsb/bus/execution_state.hpp"

#include <iostream>
#include "boost/foreach.hpp"

#include "dsb/bus/execution_agent.hpp"
#include "dsb/bus/slave_tracker.hpp"
#include "dsb/comm.hpp"
#include "dsb/inproc_rpc.hpp"
#include "dsb/protocol/execution.hpp"
#include "execution.pb.h"


namespace dsb
{
namespace bus
{


namespace
{
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

        double startTime, stopTime;
        dsb::inproc_rpc::UnmarshalSetSimulationTime(msg, startTime, stopTime);
        if (startTime > stopTime) {
            dsb::inproc_rpc::ThrowLogicError(
                userSocket,
                "Attempted to set start time greater than stop time");
        } else if (!self.slaves.empty()) {
            dsb::inproc_rpc::ThrowLogicError(
                userSocket,
                "Simulation time must be set before slaves are added");
        } else {
            self.startTime = startTime;
            self.stopTime  = stopTime;
            dsb::inproc_rpc::ReturnSuccess(userSocket);
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

        uint16_t slaveId = 0;
        dsb::inproc_rpc::UnmarshalAddSlave(msg, slaveId);
        if (self.slaves.insert(std::make_pair(slaveId, dsb::bus::SlaveTracker(self.startTime, self.stopTime))).second) {
            dsb::inproc_rpc::ReturnSuccess(userSocket);
        } else {
            dsb::inproc_rpc::ThrowLogicError(userSocket, "Slave already added");
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

        uint16_t slaveId = 0;
        dsbproto::execution::SetVarsData data;
        dsb::inproc_rpc::UnmarshalSetVariables(msg, slaveId, data);
        auto it = self.slaves.find(slaveId);
        if (it == self.slaves.end()) {
            dsb::inproc_rpc::ThrowLogicError(userSocket, "Invalid slave ID");
        } else {
            it->second.EnqueueSetVars(slaveSocket, data);
            dsb::inproc_rpc::ReturnSuccess(userSocket);
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

        uint16_t slaveId = 0;
        dsbproto::execution::ConnectVarsData data;
        dsb::inproc_rpc::UnmarshalConnectVariables(msg, slaveId, data);
        auto it = self.slaves.find(slaveId);
        if (it == self.slaves.end()) {
            dsb::inproc_rpc::ThrowLogicError(userSocket, "Invalid slave ID");
        } else {
            BOOST_FOREACH (const auto& conn, data.connection()) {
                if (!self.slaves.count(conn.output_var().slave_id())) {
                    dsb::inproc_rpc::ThrowLogicError(
                        userSocket,
                        "Invalid slave ID in output variable specification");
                    return;
                }
            }
            it->second.EnqueueConnectVars(slaveSocket, data);
            dsb::inproc_rpc::ReturnSuccess(userSocket);
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
    switch (dsb::comm::DecodeRawDataFrame<dsb::inproc_rpc::CallType>(msg[0])) {
        case dsb::inproc_rpc::SET_SIMULATION_TIME_CALL:
            PerformSetSimulationTimeRPC(self, msg, userSocket);
            break;
        case dsb::inproc_rpc::SET_VARIABLES_CALL:
            PerformSetVarsRPC(self, msg, userSocket, slaveSocket);
            break;
        case dsb::inproc_rpc::CONNECT_VARIABLES_CALL:
            PerformConnectVarsRPC(self, msg, userSocket, slaveSocket);
            break;
        case dsb::inproc_rpc::WAIT_FOR_READY_CALL:
            self.rpcInProgress = ExecutionAgentPrivate::WAIT_FOR_READY_RPC;
            break;
        case dsb::inproc_rpc::TERMINATE_CALL:
            self.ChangeState<ExecutionTerminating>(userSocket, slaveSocket);
            dsb::inproc_rpc::ReturnSuccess(userSocket);
            break;
        case dsb::inproc_rpc::ADD_SLAVE_CALL:
            PerformAddSlaveRPC(self, msg, userSocket);
            break;
        default:
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
        dsb::inproc_rpc::ReturnSuccess(userSocket);
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
    switch (dsb::comm::DecodeRawDataFrame<dsb::inproc_rpc::CallType>(msg[0])) {
        case dsb::inproc_rpc::STEP_CALL: {
            double time = 0.0;
            double stepSize = 0.0;
            dsbproto::execution::StepData stepData;
            dsb::inproc_rpc::UnmarshalStep(msg, stepData);

            // TODO: Some checks we may want to insert here (and send FAIL if they
            // do not pass):
            //   - Is the time point of the first step equal to the start time?
            //   - Is the time point plus the step size greater than the stop time?
            //   - Is the time point equal to the previous time point plus the
            //     previous step size?
            //   - Have any slaves been added to the simulation?

            // Create the STEP message body
            BOOST_FOREACH(auto& slave, self.slaves) {
                slave.second.SendStep(slaveSocket, stepData);
            }
            self.rpcInProgress = ExecutionAgentPrivate::STEP_RPC;
            self.ChangeState<ExecutionStepping>(userSocket, slaveSocket);
            break; }
        case dsb::inproc_rpc::TERMINATE_CALL:
            self.ChangeState<ExecutionTerminating>(userSocket, slaveSocket);
            dsb::inproc_rpc::ReturnSuccess(userSocket);
            break;
        case dsb::inproc_rpc::ADD_SLAVE_CALL:
            PerformAddSlaveRPC(self, msg, userSocket);
            break;
        case dsb::inproc_rpc::SET_VARIABLES_CALL:
            PerformSetVarsRPC(self, msg, userSocket, slaveSocket);
            self.ChangeState<ExecutionInitializing>(userSocket, slaveSocket);
            break;
        case dsb::inproc_rpc::CONNECT_VARIABLES_CALL:
            PerformConnectVarsRPC(self, msg, userSocket, slaveSocket);
            self.ChangeState<ExecutionInitializing>(userSocket, slaveSocket);
            break;
        case dsb::inproc_rpc::WAIT_FOR_READY_CALL:
            dsb::inproc_rpc::ReturnSuccess(userSocket);
            break;
        default:
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
    assert (slaveHandler.State() != SLAVE_STEP_FAILED
            && "A slave was unable to perform its time step, and we don't handle that too well yet...");
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


ExecutionTerminating::ExecutionTerminating()
{
}

void ExecutionTerminating::StateEntered(
    ExecutionAgentPrivate& self,
    zmq::socket_t& userSocket,
    zmq::socket_t& slaveSocket)
{
    bool readyToShutdown = true;
    BOOST_FOREACH (auto& slave, self.slaves) {
        if (slave.second.State() & TERMINATABLE_STATES) {
            slave.second.SendTerminate(slaveSocket);
        } else if (slave.second.State() != SLAVE_UNKNOWN) {
            readyToShutdown = false;
        }
    }
    if (readyToShutdown) self.Shutdown();
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

    bool readyToShutdown = true;
    BOOST_FOREACH (auto& slave, self.slaves) {
        if (slave.second.State() != SLAVE_TERMINATED) {
            readyToShutdown = false;
            break;
        }
    }
    if (readyToShutdown) self.Shutdown();
}


}} // namespace
