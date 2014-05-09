#ifndef DSB_COMM_HELPERS_HPP
#define DSB_COMM_HELPERS_HPP

#include <cstdint>
#include "zmq.hpp"
#include "control.pb.h"


namespace dsb { namespace comm {


void SendControlHello(
    zmq::socket_t& socket,
    uint16_t protocolVersion,
    const google::protobuf::MessageLite& body);

void SendControlMessage(
    zmq::socket_t& socket,
    dsbproto::control::MessageType type,
    const google::protobuf::MessageLite& body);


}}      // namespace
#endif  // header guard
