#include "dsb/protocol/control.hpp"

#include <cstring>

#include "dsb/protobuf.hpp"
#include "dsb/protocol/error.hpp"
#include "dsb/util/encoding.hpp"
#include "dsb/util/error.hpp"


namespace
{
    const size_t helloPrefixSize = 6;
    const char helloPrefix[helloPrefixSize] = { '\x00', '\x00', 'D', 'S', 'C', 'P' };
}


uint16_t dsb::protocol::control::ParseMessageType(const zmq::message_t& header)
{
    if (header.size() < 2) {
        throw ProtocolViolationException("Invalid message header (frame too short)");
    }
    return dsb::util::DecodeUint16(static_cast<const char*>(header.data()));
}


void dsb::protocol::control::CreateHelloMessage(
    uint16_t protocolVersion,
    const google::protobuf::MessageLite& body,
    std::deque<zmq::message_t>* message)
{
    DSB_INPUT_CHECK(message != nullptr);
    message->clear();

    message->emplace_back(helloPrefixSize + 2);
    const auto headerBuf = static_cast<char*>(message->back().data());
    std::memcpy(headerBuf, helloPrefix, helloPrefixSize);
    dsb::util::EncodeUint16(protocolVersion, headerBuf + helloPrefixSize);

    message->emplace_back();
    dsb::protobuf::SerializeToFrame(body, &message->back());
}


uint16_t dsb::protocol::control::ParseProtocolVersion(const zmq::message_t& header)
{
    if (header.size() != 8
        || std::memcmp(header.data(), helloPrefix, helloPrefixSize) != 0) {
        throw ProtocolViolationException(
            "Invalid message header (not a HELLO message)");
    }
    return dsb::util::DecodeUint16(
        static_cast<const char*>(header.data()) + helloPrefixSize);
}


void dsb::protocol::control::CreateMessage(
    dsbproto::control::MessageType type,
    const google::protobuf::MessageLite& body,
    std::deque<zmq::message_t>* message)
{
    DSB_INPUT_CHECK(message != nullptr);
    message->clear();
    message->emplace_back(2);
    dsb::util::EncodeUint16(type, static_cast<char*>(message->back().data()));
    message->emplace_back();
    dsb::protobuf::SerializeToFrame(body, &message->back());
}
