/*
Copyright 2013-present, SINTEF Ocean.
This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#include <coral/protocol/domain.hpp>

#include <cstring>

#include <coral/error.hpp>
#include <coral/net/zmqx.hpp>
#include <coral/protobuf.hpp>
#include <coral/util.hpp>

namespace dp = coral::protocol::domain;


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
    std::memcpy(msgData, MAGIC, MAGIC_LENGTH);
    coral::util::EncodeUint16(protocolVersion, msgData + MAGIC_LENGTH);
    coral::util::EncodeUint16(messageType, msgData + MAGIC_LENGTH + 2);
    return msg;
}


void dp::CreateAddressedMessage(
    std::vector<zmq::message_t>& message,
    const std::string& recipient,
    dp::MessageType messageType,
    uint16_t protocolVersion)
{
    message.clear();
    message.push_back(coral::net::zmqx::ToFrame(recipient));
    message.push_back(zmq::message_t(0));
    message.push_back(CreateHeader(messageType, protocolVersion));
}


void dp::CreateAddressedMessage(
    std::vector<zmq::message_t>& message,
    const std::string& recipient,
    dp::MessageType messageType,
    uint16_t protocolVersion,
    const google::protobuf::MessageLite& body)
{
    CreateAddressedMessage(message, recipient, messageType, protocolVersion);
    message.push_back(zmq::message_t());
    coral::protobuf::SerializeToFrame(body, message.back());
}


dp::Header dp::ParseHeader(const zmq::message_t& headerFrame)
{
    const size_t HEADER_SIZE = MAGIC_LENGTH + 2 + 2;
    const auto headerData = static_cast<const char*>(headerFrame.data());
    if (headerFrame.size() != HEADER_SIZE ||
        std::memcmp(headerData, MAGIC, MAGIC_LENGTH) != 0) {
        throw coral::error::ProtocolViolationException("Invalid header frame");
    }

    Header h;
    h.protocol = coral::util::DecodeUint16(headerData + MAGIC_LENGTH);
    if (h.protocol > MAX_PROTOCOL_VERSION) {
        throw coral::error::ProtocolViolationException("Unsupported protocol");
    }
    const uint16_t t = coral::util::DecodeUint16(headerData + MAGIC_LENGTH + 2);
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
            throw coral::error::ProtocolViolationException("Invalid request");
    }
    return h;
}
