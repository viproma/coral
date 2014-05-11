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
\brief  Exception which signifies that the remote end sent an ERROR message.

The `what()` function may be called to get the detailed error information
that was received.
*/
class RemoteErrorException : public std::runtime_error
{
public:
    RemoteErrorException(const dsbproto::control::ErrorInfo& errorInfo);
};


/**
\brief  Fills `message` with a body-less HELLO message that requests the
        given protocol version.

Any pre-existing contents of `message` will be replaced.
*/
void CreateHelloMessage(
    uint16_t protocolVersion,
    std::deque<zmq::message_t>& message);


/**
\brief  Fills `message` with a HELLO message that requests the
        given protocol version.

Any pre-existing contents of `message` will be replaced.
*/
void CreateHelloMessage(
    uint16_t protocolVersion,
    const google::protobuf::MessageLite& body,
    std::deque<zmq::message_t>& message);


/**
\brief  Parses the protocol version field in a HELLO message header.

\throws dsb::error::ProtocolViolationException if `header` is not a HELLO
        message header.
*/
uint16_t ParseProtocolVersion(const zmq::message_t& header);


/**
\brief  Fills `message` with a body-less message of the given type.

Any pre-existing contents of `message` will be replaced.
*/
void CreateMessage(
    dsbproto::control::MessageType type,
    std::deque<zmq::message_t>& message);


/**
\brief  Fills `message` with a body-less message of the given type.

Any pre-existing contents of `message` will be replaced.
*/
void CreateMessage(
    dsbproto::control::MessageType type,
    const google::protobuf::MessageLite& body,
    std::deque<zmq::message_t>& message);


}}      // namespace
#endif  // header guard
