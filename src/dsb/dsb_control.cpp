#include "dsb/control.hpp"

#include <cstring>

#include "dsb/error.hpp"
#include "dsb/protobuf.hpp"
#include "dsb/util.hpp"


namespace
{
    const size_t helloPrefixSize = 6;
    const char helloPrefix[helloPrefixSize] = { '\x00', '\x00', 'D', 'S', 'C', 'P' };
}


uint16_t dsb::control::ParseMessageType(const zmq::message_t& header)
{
    if (header.size() < 2) {
        throw dsb::error::ProtocolViolationException(
            "Invalid message header (frame too short)");
    }
    return dsb::util::DecodeUint16(static_cast<const char*>(header.data()));
}


void dsb::control::CreateHelloMessage(
    uint16_t protocolVersion,
    std::deque<zmq::message_t>& message)
{
    message.clear();
    message.emplace_back(helloPrefixSize + 2);
    const auto headerBuf = static_cast<char*>(message.back().data());
    std::memcpy(headerBuf, helloPrefix, helloPrefixSize);
    dsb::util::EncodeUint16(protocolVersion, headerBuf + helloPrefixSize);
}


void dsb::control::CreateHelloMessage(
    uint16_t protocolVersion,
    const google::protobuf::MessageLite& body,
    std::deque<zmq::message_t>& message)
{
    CreateHelloMessage(protocolVersion, message);
    message.emplace_back();
    dsb::protobuf::SerializeToFrame(body, message.back());
}


uint16_t dsb::control::ParseProtocolVersion(const zmq::message_t& header)
{
    if (header.size() != 8
        || std::memcmp(header.data(), helloPrefix, helloPrefixSize) != 0) {
        throw dsb::error::ProtocolViolationException(
            "Invalid message header (not a HELLO message)");
    }
    return dsb::util::DecodeUint16(
        static_cast<const char*>(header.data()) + helloPrefixSize);
}


void dsb::control::CreateMessage(
    dsbproto::control::MessageType type,
    std::deque<zmq::message_t>& message)
{
    message.clear();
    message.emplace_back(2);
    dsb::util::EncodeUint16(type, static_cast<char*>(message.back().data()));
}


void dsb::control::CreateMessage(
    dsbproto::control::MessageType type,
    const google::protobuf::MessageLite& body,
    std::deque<zmq::message_t>& message)
{
    CreateMessage(type, message);
    message.emplace_back();
    dsb::protobuf::SerializeToFrame(body, message.back());
}
