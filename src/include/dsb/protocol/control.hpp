/**
 * \file
 * \brief Functions for communication over the "control" protocol.
 * */
#ifndef DSB_PROTOCOL_CONTROL_HPP
#define DSB_PROTOCOL_CONTROL_HPP

#include <cstdint>
#include <deque>
#include "google/protobuf/message_lite.h"
#include "zmq.hpp"
#include "control.pb.h"


namespace dsb { namespace protocol { namespace control
{


uint16_t ParseMessageType(const zmq::message_t& header);

void CreateHelloMessage(
    uint16_t protocolVersion,
    std::deque<zmq::message_t>& message);

void CreateHelloMessage(
    uint16_t protocolVersion,
    const google::protobuf::MessageLite& body,
    std::deque<zmq::message_t>& message);

uint16_t ParseProtocolVersion(const zmq::message_t& header);

void CreateMessage(
    dsbproto::control::MessageType type,
    std::deque<zmq::message_t>& message);

void CreateMessage(
    dsbproto::control::MessageType type,
    const google::protobuf::MessageLite& body,
    std::deque<zmq::message_t>& message);


}}}     // namespace
#endif  // header guard
