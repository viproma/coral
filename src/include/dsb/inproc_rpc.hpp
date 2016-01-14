/**
\file
\brief Main header file for dsb::inproc_rpc.
*/
#ifndef DSB_INPROC_RPC_HPP
#define DSB_INPROC_RPC_HPP

#include <vector>
#include "zmq.hpp"
#include "google/protobuf/message_lite.h"

#ifdef DSB_USE_OLD_DOMAIN_INPROC_RPC
#   include <vector>
#   include "dsb/domain/controller.hpp"
#   include "domain_controller.pb.h"
#endif


namespace dsb
{
/// Definitions and functions for in-process (inter-thread) procedure calls.
namespace inproc_rpc
{

void Call(
    zmq::socket_t& socket,
    int call,
    const google::protobuf::MessageLite* args = nullptr,
    google::protobuf::MessageLite* returnValue = nullptr);

int GetCallType(const std::vector<zmq::message_t>& msg);

void UnmarshalArgs(
    const std::vector<zmq::message_t>& msg,
    google::protobuf::MessageLite& args);

void ReturnSuccess(
    zmq::socket_t& socket,
    const google::protobuf::MessageLite* returnValue = nullptr);

void ThrowLogicError(zmq::socket_t& socket, const std::string& what);

void ThrowRuntimeError(zmq::socket_t& socket, const std::string& what);


#ifdef DSB_USE_OLD_DOMAIN_INPROC_RPC
// =============================================================================
// Domain controller RPC stuff. To be phased out, as was done with the
// execution controller RPC.  (When this is done, also remember to remove the
// macro definition from the Doxygen configuration file (PREDEFINED property).

enum CallType
{
    GET_SLAVE_TYPES_CALL,
    INSTANTIATE_SLAVE_CALL,
};

void ReturnSuccess(
    zmq::socket_t& socket,
    std::vector<zmq::message_t>& returnValues);

void CallGetSlaveTypes(
    zmq::socket_t& socket,
    std::vector<dsb::domain::Controller::SlaveType>& slaveTypes);

void ReturnGetSlaveTypes(
    zmq::socket_t& socket,
    dsbproto::domain_controller::SlaveTypeList& slaveTypes);

dsb::net::SlaveLocator CallInstantiateSlave(
    zmq::socket_t& socket,
    const std::string& slaveTypeUUID,
    std::chrono::milliseconds timeout,
    const std::string& provider);

void UnmarshalInstantiateSlave(
    std::vector<zmq::message_t>& msg,
    std::string& slaveTypeUUID,
    std::chrono::milliseconds& timeout,
    std::string& provider);

void ReturnInstantiateSlave(
    zmq::socket_t& socket,
    const dsb::net::SlaveLocator& slaveLocator);

#endif // DSB_OLD_DOMAIN_INPROC_RPC


}}      // namespace
#endif  // header guard
