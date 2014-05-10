#include "dsb/protocol/control.hpp"

#include <cstring>

#include "dsb/protobuf.hpp"
#include "dsb/protocol/error.hpp"
#include "dsb/util/encoding.hpp"


namespace dsb { namespace protocol { namespace control {


uint16_t ParseMessageType(const zmq::message_t& header)
{
    if (header.size() < 2) {
        throw ProtocolViolationException("Invalid message header (frame too short)");
    }
    return dsb::util::DecodeUint16(static_cast<const char*>(header.data()));
}


void SendHello(
    zmq::socket_t& socket,
    uint16_t protocolVersion,
    const google::protobuf::MessageLite& body)
{
    const size_t headerSize = 8;
    char headerFrame[headerSize] = { '\x00', '\x00', 'D', 'S', 'C', 'P', '\xFF', '\xFF' };
    dsb::util::EncodeUint16(static_cast<uint16_t>(protocolVersion), headerFrame + 6);

    zmq::message_t bodyFrame;
    dsb::protobuf::SerializeToFrame(body, &bodyFrame);

    socket.send(headerFrame, headerSize, ZMQ_SNDMORE);
    socket.send(bodyFrame);
}


uint16_t ParseProtocolVersion(const zmq::message_t& header)
{
    if (header.size() != 8 || std::memcmp(header.data(), "\x00\x00""DSCP", 6) != 0) {
        throw ProtocolViolationException("Invalid message header (not a HELLO message)");
    }
    return dsb::util::DecodeUint16(static_cast<const char*>(header.data()) + 6);
}


void SendMessage(
    zmq::socket_t& socket,
    dsbproto::control::MessageType type,
    const google::protobuf::MessageLite& body)
{
    const size_t headerSize = 2;
    char headerFrame[headerSize] = { '\xFF', '\xFF' };
    dsb::util::EncodeUint16(static_cast<uint16_t>(type), headerFrame);

    zmq::message_t bodyFrame;
    dsb::protobuf::SerializeToFrame(body, &bodyFrame);

    socket.send(headerFrame, headerSize, ZMQ_SNDMORE);
    socket.send(bodyFrame);
}


}}} // namespace
