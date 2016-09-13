#include "coral/protocol/execution.hpp"

#include <cassert>
#include <cstring>
#include <sstream>

#include "coral/config.h"
#include "coral/error.hpp"
#include "coral/protobuf.hpp"
#include "coral/util.hpp"


namespace
{
    const size_t helloPrefixSize = 6;
    const char helloPrefix[helloPrefixSize] = { '\x01', '\x00', 'D', 'S', 'C', 'P' };
    const size_t deniedHeaderSize = 2;
    const char deniedHeader[deniedHeaderSize] = { '\x00', '\x00' };
}


void coral::protocol::execution::CreateHelloMessage(
    std::vector<zmq::message_t>& message,
    uint16_t protocolVersion)
{
    message.clear();
    message.emplace_back(helloPrefixSize + 2);
    const auto headerBuf = static_cast<char*>(message.back().data());
    std::memcpy(headerBuf, helloPrefix, helloPrefixSize);
    coral::util::EncodeUint16(protocolVersion, headerBuf + helloPrefixSize);
}


void coral::protocol::execution::CreateHelloMessage(
    std::vector<zmq::message_t>& message,
    uint16_t protocolVersion,
    const google::protobuf::MessageLite& body)
{
    CreateHelloMessage(message, protocolVersion);
    message.emplace_back();
    coral::protobuf::SerializeToFrame(body, message.back());
}


void coral::protocol::execution::CreateDeniedMessage(
    std::vector<zmq::message_t>& message,
    const std::string& reason)
{
    message.clear();
    message.emplace_back(deniedHeaderSize);
    message.emplace_back(reason.size());
    std::memcpy(message[0].data(), deniedHeader, deniedHeaderSize);
    std::memcpy(message[1].data(), reason.data(), reason.size());
}


void coral::protocol::execution::CreateMessage(
    std::vector<zmq::message_t>& message,
    coralproto::execution::MessageType type)
{
    message.clear();
    message.emplace_back(2);
    coral::util::EncodeUint16(type, static_cast<char*>(message.back().data()));
}


void coral::protocol::execution::CreateMessage(
    std::vector<zmq::message_t>& message,
    coralproto::execution::MessageType type,
    const google::protobuf::MessageLite& body)
{
    CreateMessage(message, type);
    message.emplace_back();
    coral::protobuf::SerializeToFrame(body, message.back());
}


void coral::protocol::execution::CreateErrorMessage(
    std::vector<zmq::message_t>& message,
    coralproto::execution::ErrorInfo::Code code,
    const std::string& details)
{
    coralproto::execution::ErrorInfo errorInfo;
    errorInfo.set_code(code);
    if (!details.empty()) {
        errorInfo.set_details(details);
    }
    CreateMessage(message, coralproto::execution::MSG_ERROR, errorInfo);
}


uint16_t coral::protocol::execution::ParseMessageType(const zmq::message_t& header)
{
    if (header.size() < 2) {
        throw coral::error::ProtocolViolationException(
            "Invalid message header (frame too short)");
    }
    return coral::util::DecodeUint16(static_cast<const char*>(header.data()));
}


uint16_t coral::protocol::execution::NonErrorMessageType(
    const std::vector<zmq::message_t>& message)
{
    CORAL_INPUT_CHECK(!message.empty());
    const auto type = ParseMessageType(message.front());
    if (type == coralproto::execution::MSG_ERROR) {
        coralproto::execution::ErrorInfo errorInfo;
        if (message.size() > 1) {
            coral::protobuf::ParseFromFrame(message[1], errorInfo);
        }
        throw RemoteErrorException(errorInfo);
    }
    return type;
}


namespace
{
    // Returns a standard error message corresponding to the given code.
    const char* RemoteErrorString(const coralproto::execution::ErrorInfo::Code& code)
    {
        switch (code) {
            case coralproto::execution::ErrorInfo::INVALID_REQUEST:
                return "Invalid request";
            default:
                assert (code == coralproto::execution::ErrorInfo::UNKNOWN_ERROR
                        && "RemoteErrorString() received an undefined error code");
                return "Unknown error";
        }
    }

    // Returns a detailed error message on the form
    // "standard msg. (details)"
    std::string DetailedRemoteErrorString(
        const coralproto::execution::ErrorInfo& errorInfo)
    {
        std::stringstream s;
        s << RemoteErrorString(errorInfo.code())
          << " (" << errorInfo.details() << ')';
        return s.str();
    }
}


coral::protocol::execution::RemoteErrorException::RemoteErrorException(
    const std::string& deniedReason)
    : std::runtime_error("Connection denied: " + deniedReason)
{ }

coral::protocol::execution::RemoteErrorException::RemoteErrorException(
    const coralproto::execution::ErrorInfo& errorInfo)
    : std::runtime_error(DetailedRemoteErrorString(errorInfo))
{ }


uint16_t coral::protocol::execution::ParseHelloMessage(const std::vector<zmq::message_t>& message)
{
    CORAL_INPUT_CHECK(!message.empty());
    if (message.front().size() == 8
        && std::memcmp(message.front().data(), helloPrefix, helloPrefixSize) == 0)
    {
        return coral::util::DecodeUint16(
            static_cast<const char*>(message.front().data()) + helloPrefixSize);
    }
    else if (message.size() == 2
        && message.front().size() == 2
        && std::memcmp(message.front().data(), deniedHeader, deniedHeaderSize) == 0)
    {
        throw RemoteErrorException(
            std::string(static_cast<const char*>(message[1].data()), message[1].size()));
    } else {
        throw coral::error::ProtocolViolationException(
            "Invalid message (not a HELLO or DENIED message)");
    }
}
