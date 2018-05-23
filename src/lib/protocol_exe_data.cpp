/*
Copyright 2013-2017, SINTEF Ocean and the Coral contributors.
This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#include <coral/protocol/exe_data.hpp>

#include <coral/error.hpp>
#include <coral/protobuf.hpp>
#include <coral/protocol/glue.hpp>
#include <coral/util.hpp>

#ifdef _MSC_VER
#   pragma warning(push, 0)
#endif
#include <exe_data.pb.h>
#ifdef _MSC_VER
#   pragma warning(pop)
#endif


namespace ed = coral::protocol::exe_data;


namespace {
    coral::model::Variable ParseHeader(const zmq::message_t& msg)
    {
        if (msg.size() != ed::HEADER_SIZE) {
            throw coral::error::ProtocolViolationException(
                "Invalid header frame");
        }
        return coral::model::Variable(
            coral::util::DecodeUint16(static_cast<const char*>(msg.data())),
            coral::util::DecodeUint32(static_cast<const char*>(msg.data()) + 2));
    }

    void CreateRawHeader(
        const coral::model::Variable& var,
        char buf[ed::HEADER_SIZE])
    {
        coral::util::EncodeUint16(var.Slave(), buf);
        coral::util::EncodeUint32(var.ID(), buf + 2);
    }

    zmq::message_t CreateHeader(const coral::model::Variable& var)
    {
        auto msg = zmq::message_t(ed::HEADER_SIZE);
        CreateRawHeader(var, static_cast<char*>(msg.data()));
        return msg;
    }
}


ed::Message ed::ParseMessage(const std::vector<zmq::message_t>& rawMsg)
{
    if (rawMsg.size() != 2) {
        throw coral::error::ProtocolViolationException(
            "Wrong number of frames");
    }
    Message m;
    m.variable = ParseHeader(rawMsg[0]);
    coralproto::exe_data::TimestampedValue timestampedValue;
    coral::protobuf::ParseFromFrame(rawMsg[1], timestampedValue);
    m.timestepID = timestampedValue.timestep_id();
    m.value = coral::protocol::FromProto(timestampedValue.value());
    return m;
}


void ed::CreateMessage(
    const ed::Message& message,
    std::vector<zmq::message_t>& rawOut)
{
    rawOut.clear();
    rawOut.push_back(CreateHeader(message.variable));
    coralproto::exe_data::TimestampedValue timestampedValue;
    coral::protocol::ConvertToProto(message.value, *timestampedValue.mutable_value());
    timestampedValue.set_timestep_id(message.timestepID);
    rawOut.emplace_back();
    coral::protobuf::SerializeToFrame(timestampedValue, rawOut[1]);
}


void ed::Subscribe(zmq::socket_t& socket, const coral::model::Variable& variable)
{
    char header[HEADER_SIZE];
    CreateRawHeader(variable, header);
    socket.setsockopt(ZMQ_SUBSCRIBE, header, HEADER_SIZE);
}


void ed::Unsubscribe(zmq::socket_t& socket, const coral::model::Variable& variable)
{
    char header[HEADER_SIZE];
    CreateRawHeader(variable, header);
    socket.setsockopt(ZMQ_UNSUBSCRIBE, header, HEADER_SIZE);
}
