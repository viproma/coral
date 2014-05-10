#include "dsb/comm/helpers.hpp"

#include <algorithm>
#include <cstring>
#include "dsb/comm/error.hpp"
#include "dsb/comm/protobuf.hpp"
#include "dsb/util/encoding.hpp"
#include "dsb/util/error.hpp"


uint16_t dsb::comm::ParseControlMessageType(const zmq::message_t& header)
{
    if (header.size() < 2) {
        throw ProtocolViolationException("Invalid message header (frame too short)");
    }
    return dsb::util::DecodeUint16(static_cast<const char*>(header.data()));
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
    SerializeToFrame(body, &bodyFrame);

    socket.send(headerFrame, headerSize, ZMQ_SNDMORE);
    socket.send(bodyFrame);
}


uint16_t dsb::comm::ParseControlProtocolVersion(const zmq::message_t& header)
{
    if (header.size() != 8 || std::memcmp(header.data(), "\x00\x00""DSCP", 6) != 0) {
        throw ProtocolViolationException("Invalid message header (not a HELLO message)");
    }
    return dsb::util::DecodeUint16(static_cast<const char*>(header.data()) + 6);
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
    SerializeToFrame(body, &bodyFrame);

    socket.send(headerFrame, headerSize, ZMQ_SNDMORE);
    socket.send(bodyFrame);
}


void dsb::comm::RecvMessage(
    zmq::socket_t& socket,
    std::deque<zmq::message_t>* message)
{
    DSB_INPUT_CHECK(message != nullptr);
    message->clear();
    do {
        message->emplace_back();
        socket.recv(&message->back());
    } while (message->back().more());
}


size_t dsb::comm::PopMessageEnvelope(
    std::deque<zmq::message_t>& message,
    std::deque<zmq::message_t>* envelope)
{
    auto delim = std::find_if(message.begin(), message.end(),
                              [](const zmq::message_t& m) { return m.size() == 0; });
    if (delim == message.end()) {
        if (envelope) envelope->clear();
        return 0;
    }

    const auto envSize = delim - message.begin();
    if (envelope) {
        envelope->resize(envSize);
        std::move(message.begin(), delim, envelope->begin());
    }
    message.erase(message.begin(), ++delim);
    return envSize + 1;
}
