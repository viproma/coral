#include "dsb/comm/helpers.hpp"

#include "dsb/util/encoding.hpp"


namespace
{
    void SerializeToMsg(
        const google::protobuf::MessageLite& source,
        zmq::message_t& target)
    {
        const auto size = source.ByteSize();
        target.rebuild(size);
        source.SerializeToArray(target.data(), size);
    }
}


void dsb::comm::SendControlHello(
    zmq::socket_t& socket,
    uint16_t protocolVersion,
    const google::protobuf::MessageLite& body)
{
    const size_t headerSize = 8;
    char headerFrame[headerSize] = { '\x00', '\x00', 'D', 'S', 'C', 'P', '\xFF', '\xFF' };
    dsb::util::EncodeUint16(static_cast<uint16_t>(protocolVersion), headerFrame + 6);

    zmq::message_t bodyFrame;
    SerializeToMsg(body, bodyFrame);

    socket.send(headerFrame, headerSize, ZMQ_SNDMORE);
    socket.send(bodyFrame);
}


void dsb::comm::SendControlMessage(
    zmq::socket_t& socket,
    dsbproto::control::MessageType type,
    const google::protobuf::MessageLite& body)
{
    const size_t headerSize = 2;
    char headerFrame[headerSize] = { '\xFF', '\xFF' };
    dsb::util::EncodeUint16(static_cast<uint16_t>(type), headerFrame);

    zmq::message_t bodyFrame;
    SerializeToMsg(body, bodyFrame);

    socket.send(headerFrame, headerSize, ZMQ_SNDMORE);
    socket.send(bodyFrame);
}

