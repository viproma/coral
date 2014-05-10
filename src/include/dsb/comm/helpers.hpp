#ifndef DSB_COMM_HELPERS_HPP
#define DSB_COMM_HELPERS_HPP

#include <cstdint>
#include <deque>
#include "zmq.hpp"
#include "control.pb.h"


namespace dsb { namespace comm
{


uint16_t ParseControlMessageType(const zmq::message_t& header);

void SendControlHello(
    zmq::socket_t& socket,
    uint16_t protocolVersion,
    const google::protobuf::MessageLite& body);

uint16_t ParseControlProtocolVersion(const zmq::message_t& header);

void SendControlMessage(
    zmq::socket_t& socket,
    dsbproto::control::MessageType type,
    const google::protobuf::MessageLite& body);

void RecvMessage(
    zmq::socket_t& socket,
    std::deque<zmq::message_t>* message);

size_t PopMessageEnvelope(
    std::deque<zmq::message_t>& message,
    std::deque<zmq::message_t>* envelope = nullptr);


}}      // namespace
#endif  // header guard
