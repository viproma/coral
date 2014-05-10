/**
 * \file
 * \brief Functions for communication over the "control" protocol.
 * */
#ifndef DSB_PROTOCOL_CONTROL_HPP
#define DSB_PROTOCOL_CONTROL_HPP

#include <cstdint>
#include "google/protobuf/message_lite.h"
#include "zmq.hpp"
#include "control.pb.h"


namespace dsb { namespace protocol { namespace control
{


uint16_t ParseMessageType(const zmq::message_t& header);

void SendHello(
    zmq::socket_t& socket,
    uint16_t protocolVersion,
    const google::protobuf::MessageLite& body);

uint16_t ParseProtocolVersion(const zmq::message_t& header);

void SendMessage(
    zmq::socket_t& socket,
    dsbproto::control::MessageType type,
    const google::protobuf::MessageLite& body);


}}}     // namespace
#endif  // header guard
