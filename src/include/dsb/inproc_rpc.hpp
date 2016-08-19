/**
\file
\brief Main header file for dsb::inproc_rpc.
*/
#ifndef DSB_INPROC_RPC_HPP
#define DSB_INPROC_RPC_HPP

#include <vector>
#include "zmq.hpp"
#include "google/protobuf/message_lite.h"


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


}}      // namespace
#endif  // header guard
