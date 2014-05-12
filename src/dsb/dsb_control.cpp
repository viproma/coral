#include "dsb/control.hpp"

#include <cstring>
#include <sstream>

#include "dsb/error.hpp"
#include "dsb/protobuf.hpp"
#include "dsb/util.hpp"


namespace
{
    const size_t helloPrefixSize = 6;
    const char helloPrefix[helloPrefixSize] = { '\x00', '\x00', 'D', 'S', 'C', 'P' };
}


void dsb::control::CreateHelloMessage(
    std::deque<zmq::message_t>& message,
    uint16_t protocolVersion)
{
    message.clear();
    message.emplace_back(helloPrefixSize + 2);
    const auto headerBuf = static_cast<char*>(message.back().data());
    std::memcpy(headerBuf, helloPrefix, helloPrefixSize);
    dsb::util::EncodeUint16(protocolVersion, headerBuf + helloPrefixSize);
}


void dsb::control::CreateHelloMessage(
    std::deque<zmq::message_t>& message,
    uint16_t protocolVersion,
    const google::protobuf::MessageLite& body)
{
    CreateHelloMessage(message, protocolVersion);
    message.emplace_back();
    dsb::protobuf::SerializeToFrame(body, message.back());
}


void dsb::control::CreateMessage(
    std::deque<zmq::message_t>& message,
    dsbproto::control::MessageType type)
{
    message.clear();
    message.emplace_back(2);
    dsb::util::EncodeUint16(type, static_cast<char*>(message.back().data()));
}


void dsb::control::CreateMessage(
    std::deque<zmq::message_t>& message,
    dsbproto::control::MessageType type,
    const google::protobuf::MessageLite& body)
{
    CreateMessage(message, type);
    message.emplace_back();
    dsb::protobuf::SerializeToFrame(body, message.back());
}


void dsb::control::CreateErrorMessage(
    std::deque<zmq::message_t>& message,
    dsbproto::control::ErrorInfo::Code code,
    const std::string& details)
{
    dsbproto::control::ErrorInfo errorInfo;
    errorInfo.set_code(code);
    if (!details.empty()) {
        errorInfo.set_details(details);
    }
    CreateMessage(message, dsbproto::control::ERROR, errorInfo);
}


uint16_t dsb::control::ParseMessageType(const zmq::message_t& header)
{
    if (header.size() < 2) {
        throw dsb::error::ProtocolViolationException(
            "Invalid message header (frame too short)");
    }
    return dsb::util::DecodeUint16(static_cast<const char*>(header.data()));
}


uint16_t dsb::control::NonErrorMessageType(
    const std::deque<zmq::message_t>& message)
{
    DSB_INPUT_CHECK(!message.empty());
    const auto type = ParseMessageType(message.front());
    if (type == dsbproto::control::ERROR) {
        dsbproto::control::ErrorInfo errorInfo;
        if (message.size() > 1) {
            dsb::protobuf::ParseFromFrame(message[1], errorInfo);
        }
        throw RemoteErrorException(errorInfo);
    }
    return type;
}


namespace
{
    // Returns a standard error message corresponding to the given code.
    const char* RemoteErrorString(const dsbproto::control::ErrorInfo::Code& code)
    {
        switch (code) {
            case dsbproto::control::ErrorInfo::INVALID_REQUEST:
                return "Invalid request";
            default:
                return "Unknown error";
        }
    }

    // Returns a detailed error message on the form
    // "standard msg. (details)"
    std::string DetailedRemoteErrorString(
        const dsbproto::control::ErrorInfo& errorInfo)
    {
        std::stringstream s;
        s << RemoteErrorString(errorInfo.code())
          << " (" << errorInfo.details() << ')';
        return s.str();
    }
}


dsb::control::RemoteErrorException::RemoteErrorException(
    const dsbproto::control::ErrorInfo& errorInfo)
    : std::runtime_error(DetailedRemoteErrorString(errorInfo))
{ }


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
