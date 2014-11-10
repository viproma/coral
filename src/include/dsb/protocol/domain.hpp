#ifndef DSB_PROTOCOL_DOMAIN_HPP
#define DSB_PROTOCOL_DOMAIN_HPP

#include <deque>
#include <string>
#include "zmq.hpp"


namespace dsb
{
namespace protocol
{
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
};


zmq::message_t CreateHeader(MessageType messageType, uint16_t protocolVersion);


void CreateAddressedMessage(
    std::deque<zmq::message_t>& message,
    const std::string& recipient,
    MessageType messageType,
    uint16_t protocolVersion);


struct Header
{
    uint16_t protocol;
    MessageType messageType;
};


Header ParseHeader(const zmq::message_t& headerFrame);


}}}     // namespace
#endif  // header guard