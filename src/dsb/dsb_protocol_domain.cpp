#include "dsb/protocol/domain.hpp"

#include "dsb/comm.hpp"
#include "dsb/error.hpp"
#include "dsb/protobuf.hpp"
#include "dsb/util.hpp"

namespace dp = dsb::protocol::domain;


namespace
{
    const size_t MAGIC_LENGTH = 4;
    const char* MAGIC = "DSDP"; // distributed simulation domain protocol
    const uint16_t MAX_PROTOCOL_VERSION = 0;
}


void dp::SubscribeToReports(zmq::socket_t& subSocket)
{
    subSocket.setsockopt(ZMQ_SUBSCRIBE, MAGIC, MAGIC_LENGTH);
}


zmq::message_t dp::CreateHeader(MessageType messageType, uint16_t protocolVersion)
{
    zmq::message_t msg(MAGIC_LENGTH + 4);
    const auto msgData = static_cast<char*>(msg.data());
    std::strncpy(msgData, MAGIC, MAGIC_LENGTH);
    dsb::util::EncodeUint16(protocolVersion, msgData + MAGIC_LENGTH);
    dsb::util::EncodeUint16(messageType, msgData + MAGIC_LENGTH + 2);
    return msg;
}


void dp::CreateAddressedMessage(
    std::deque<zmq::message_t>& message,
    const std::string& recipient,
    dp::MessageType messageType,
    uint16_t protocolVersion)
{
    message.clear();
    message.push_back(dsb::comm::ToFrame(recipient));
    message.push_back(zmq::message_t(0));
    message.push_back(CreateHeader(messageType, protocolVersion));
}


void dp::CreateAddressedMessage(
    std::deque<zmq::message_t>& message,
    const std::string& recipient,
    dp::MessageType messageType,
    uint16_t protocolVersion,
    const google::protobuf::MessageLite& body)
{
    CreateAddressedMessage(message, recipient, messageType, protocolVersion);
    message.push_back(zmq::message_t());
    dsb::protobuf::SerializeToFrame(body, message.back());
}


dp::Header dp::ParseHeader(const zmq::message_t& headerFrame)
{
    const size_t HEADER_SIZE = MAGIC_LENGTH + 2 + 2;
    const auto headerData = static_cast<const char*>(headerFrame.data());
    if (headerFrame.size() != HEADER_SIZE ||
        std::memcmp(headerData, MAGIC, MAGIC_LENGTH) != 0) {
        throw dsb::error::ProtocolViolationException("Invalid header frame");
    }

    Header h;
    h.protocol = dsb::util::DecodeUint16(headerData + MAGIC_LENGTH);
    if (h.protocol > MAX_PROTOCOL_VERSION) {
        throw dsb::error::ProtocolViolationException("Unsupported protocol");
    }
    const uint16_t t = dsb::util::DecodeUint16(headerData + MAGIC_LENGTH + 2);
    switch (t) {
        case MSG_SLAVEPROVIDER_HELLO:
        case MSG_UPDATE_AVAILABLE:
        case MSG_GET_SLAVE_LIST:
        case MSG_SLAVE_LIST:
        case MSG_INSTANTIATE_SLAVE:
        case MSG_INSTANTIATE_SLAVE_OK:
        case MSG_INSTANTIATE_SLAVE_FAILED:
            h.messageType = static_cast<MessageType>(t);
            break;
        default:
            throw dsb::error::ProtocolViolationException("Invalid request");
    }
    return h;
}
