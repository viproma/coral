/**
\file
\brief Main header file for dsb::protocol::domain.
*/
#ifndef DSB_PROTOCOL_DOMAIN_HPP
#define DSB_PROTOCOL_DOMAIN_HPP

#include <string>
#include <vector>
#include "zmq.hpp"
#include "google/protobuf/message_lite.h"


namespace dsb
{
namespace protocol
{
/**
\brief  Functions for constructing and parsing messages sent between domain
        participants.
*/
namespace domain
{


const uint16_t MAX_PROTOCOL_VERSION = 0;


void SubscribeToReports(zmq::socket_t& subSocket);


enum MessageType
{
    MSG_SLAVEPROVIDER_HELLO,
    MSG_UPDATE_AVAILABLE,
    MSG_GET_SLAVE_LIST,
    MSG_SLAVE_LIST,
    MSG_INSTANTIATE_SLAVE,
    MSG_INSTANTIATE_SLAVE_OK,
    MSG_INSTANTIATE_SLAVE_FAILED
};


/**
\brief  Creates a message header for the given message type, using the given
        protocol version.
*/
zmq::message_t CreateHeader(MessageType messageType, uint16_t protocolVersion);


/**
\brief  Creates a body-less addressed message.

This function fills `message` with three frames: an envelope frame with the
contents of `recipient`, an empty delimiter frame, and a header frame for the
given message type and protocol version.  Any previous contents of `message`
will be replaced.
*/
void CreateAddressedMessage(
    std::vector<zmq::message_t>& message,
    const std::string& recipient,
    MessageType messageType,
    uint16_t protocolVersion);


/**
\brief  Creates an addressed message.
*/
void CreateAddressedMessage(
    std::vector<zmq::message_t>& message,
    const std::string& recipient,
    MessageType messageType,
    uint16_t protocolVersion,
    const google::protobuf::MessageLite& body);


/// The information in a message header.
struct Header
{
    /// Protocol version.
    uint16_t protocol;

    /// Message type.
    MessageType messageType;
};


/**
\brief  Parses a header frame and returns its contents as a Header object.

\throws dsb::error::ProtocolViolationException if the frame format is invalid,
    if the protocol version is not supported, or if the message type is
    unknown.
*/
Header ParseHeader(const zmq::message_t& headerFrame);


}}}     // namespace
#endif  // header guard
