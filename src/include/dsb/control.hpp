/**
\file
\brief Main header file for dsb::control.
*/
#ifndef DSB_CONTROL_HPP
#define DSB_CONTROL_HPP

#include <cstdint>
#include <deque>
#include "google/protobuf/message_lite.h"
#include "zmq.hpp"
#include "control.pb.h"


namespace dsb
{

/// Functions for communication over the "control" protocol.
namespace control
{


/**
\brief  Fills `message` with a body-less HELLO message that requests the
        given protocol version.

Any pre-existing contents of `message` will be replaced.
*/
void CreateHelloMessage(
    std::deque<zmq::message_t>& message,
    uint16_t protocolVersion);


/**
\brief  Fills `message` with a HELLO message that requests the
        given protocol version.

Any pre-existing contents of `message` will be replaced.
*/
void CreateHelloMessage(
    std::deque<zmq::message_t>& message,
    uint16_t protocolVersion,
    const google::protobuf::MessageLite& body);

/**
\brief  Fills 'message' with a DENIED message with the given reason string.

Any pre-existing contents of `message` will be replaced.
*/
void CreateDeniedMessage(
    std::deque<zmq::message_t>& message,
    const std::string& reason = std::string());


/**
\brief  Fills `message` with a body-less message of the given type.

Any pre-existing contents of `message` will be replaced.
*/
void CreateMessage(
    std::deque<zmq::message_t>& message,
    dsbproto::control::MessageType type);


/**
\brief  Fills `message` with a body-less message of the given type.

Any pre-existing contents of `message` will be replaced.
*/
void CreateMessage(
    std::deque<zmq::message_t>& message,
    dsbproto::control::MessageType type,
    const google::protobuf::MessageLite& body);


/**
\brief  Fills `message` with an ERROR message.

Any pre-existing contents of `message` will be replaced.
*/
void CreateErrorMessage(
    std::deque<zmq::message_t>& message,
    dsbproto::control::ErrorInfo::Code code,
    const std::string& details = std::string());


/**
\brief  Parses the first two bytes of `header` as an uint16_t.

This function does *not* check whether the returned number is a valid
message type.

\throws dsb::error::ProtocolViolationException if `header` is shorter than
        two bytes.
*/
uint16_t ParseMessageType(const zmq::message_t& header);


/**
\brief  Parses the first two bytes of the message as an uint16_t, and throws
        an exception if it is an ERROR message.

\throws RemoteErrorException if `message` is an ERROR message.
\throws std::invalid_argument if `message` is empty.
*/
uint16_t NonErrorMessageType(const std::deque<zmq::message_t>& message);


/**
\brief  Exception which signifies that the remote end sent a DENIED or ERROR
        message.

The `what()` function may be called to get the detailed error information
that was received.
*/
class RemoteErrorException : public std::runtime_error
{
public:
    // Constructor for DENIED messages.
    RemoteErrorException(const std::string& deniedReason);

    // Constructor for ERROR messages.
    RemoteErrorException(const dsbproto::control::ErrorInfo& errorInfo);
};


/**
\brief  Parses HELLO or DENIED messages.

If `message` is a HELLO message, this function will parse it and return the
protocol version.  Otherwise, if it is a DENIED message, a RemoteErrorException
will be thrown.  If the message is neither of these types, a
ProtocolViolationException will be thrown.

\throws std::logic_error if `message` is empty.
\throws RemoteErrorException if `message` is a DENIED message.
\throws dsb::error::ProtocolViolationException if `message` is not a HELLO
        or DENIED message.
*/
uint16_t ParseHelloMessage(const std::deque<zmq::message_t>& message);


}}      // namespace
#endif  // header guard
