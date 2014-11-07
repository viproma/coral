/**
\file
\brief Main header file for dsb::inproc_rpc.
*/
#ifndef DSB_INPROC_RPC_HPP
#define DSB_INPROC_RPC_HPP

#include <deque>
#include "zmq.hpp"

#include "dsb/domain/controller.hpp"
#include "dsb/model.hpp"
#include "dsb/sequence.hpp"
#include "execution.pb.h"
#include "inproc_rpc.pb.h"


namespace dsb
{
/// Definitions and functions for in-process (inter-thread) procedure calls.
namespace inproc_rpc
{


enum CallType
{
    // Domain commands
    GET_SLAVE_TYPES,
    // Execution commands
    SET_SIMULATION_TIME_CALL,
    ADD_SLAVE_CALL,
    SET_VARIABLES_CALL,
    CONNECT_VARIABLES_CALL,
    TERMINATE_CALL,
    WAIT_FOR_READY_CALL,
    STEP_CALL,
};

void ReturnSuccess(zmq::socket_t& socket);

void ReturnSuccess(
    zmq::socket_t& socket,
    std::deque<zmq::message_t>& returnValues);

void ThrowLogicError(zmq::socket_t& socket, const std::string& what);

void ThrowRuntimeError(zmq::socket_t& socket, const std::string& what);

void CallGetSlaveTypes(
    zmq::socket_t& socket,
    std::vector<dsb::domain::Controller::SlaveType>& slaveTypes);

void ReturnGetSlaveTypes(
    zmq::socket_t& socket,
    dsbproto::inproc_rpc::SlaveTypeList& slaveTypes);

void CallSetSimulationTime(
    zmq::socket_t& socket,
    dsb::model::TimePoint startTime,
    dsb::model::TimePoint stopTime);

void UnmarshalSetSimulationTime(
    const std::deque<zmq::message_t>& msg,
    dsb::model::TimePoint& startTime,
    dsb::model::TimePoint& stopTime);

void CallAddSlave(zmq::socket_t& socket, dsb::model::SlaveID slaveId);

void UnmarshalAddSlave(
    const std::deque<zmq::message_t>& msg,
    dsb::model::SlaveID& slaveId);

void CallSetVariables(
    zmq::socket_t& socket,
    dsb::model::SlaveID slaveId,
    dsb::sequence::Sequence<dsb::model::VariableValue> variables);

void UnmarshalSetVariables(
    const std::deque<zmq::message_t>& msg,
    dsb::model::SlaveID& slaveId,
    dsbproto::execution::SetVarsData& setVarsData);

void CallConnectVariables(
    zmq::socket_t& socket,
    dsb::model::SlaveID slaveId,
    dsb::sequence::Sequence<dsb::model::VariableConnection> variables);

void UnmarshalConnectVariables(
    const std::deque<zmq::message_t>& msg,
    dsb::model::SlaveID& slaveId,
    dsbproto::execution::ConnectVarsData& setVarsData);

void CallWaitForReady(zmq::socket_t& socket);

void CallStep(
    zmq::socket_t& socket,
    dsb::model::TimePoint t,
    dsb::model::TimeDuration dt);

void UnmarshalStep(
    const std::deque<zmq::message_t>& msg,
    dsbproto::execution::StepData& stepData);

void CallTerminate(zmq::socket_t& socket);


}}      // namespace
#endif  // header guard
