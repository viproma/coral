#include "dsb/protocol/exe_data.hpp"

#include "dsb/error.hpp"
#include "dsb/protobuf.hpp"
#include "dsb/protocol/glue.hpp"
#include "dsb/util.hpp"

#include "exe_data.pb.h"


namespace ed = dsb::protocol::exe_data;


namespace {
    dsb::model::Variable ParseHeader(const zmq::message_t& msg)
    {
        if (msg.size() != ed::HEADER_SIZE) {
            throw dsb::error::ProtocolViolationException(
                "Invalid header frame");
        }
        return dsb::model::Variable(
            dsb::util::DecodeUint16(static_cast<const char*>(msg.data())),
            dsb::util::DecodeUint16(static_cast<const char*>(msg.data()) + 2));
    }

    void CreateRawHeader(
        const dsb::model::Variable& var,
        char buf[ed::HEADER_SIZE])
    {
        dsb::util::EncodeUint16(var.Slave(), buf);
        dsb::util::EncodeUint16(var.ID(), buf + 2);
    }

    zmq::message_t CreateHeader(const dsb::model::Variable& var)
    {
        auto msg = zmq::message_t(ed::HEADER_SIZE);
        CreateRawHeader(var, static_cast<char*>(msg.data()));
        return msg;
    }
}


ed::Message ed::ParseMessage(const std::vector<zmq::message_t>& rawMsg)
{
    if (rawMsg.size() != 2) {
        throw dsb::error::ProtocolViolationException(
            "Wrong number of frames");
    }
    Message m;
    m.variable = ParseHeader(rawMsg[0]);
    dsbproto::exe_data::TimestampedValue timestampedValue;
    dsb::protobuf::ParseFromFrame(rawMsg[1], timestampedValue);
    m.timestepID = timestampedValue.timestep_id();
    m.value = dsb::protocol::FromProto(timestampedValue.value());
    return m;
}


void ed::CreateMessage(
    const ed::Message& message,
    std::vector<zmq::message_t>& rawOut)
{
    rawOut.clear();
    rawOut.push_back(CreateHeader(message.variable));
    dsbproto::exe_data::TimestampedValue timestampedValue;
    dsb::protocol::ConvertToProto(message.value, *timestampedValue.mutable_value());
    timestampedValue.set_timestep_id(message.timestepID);
    rawOut.emplace_back();
    dsb::protobuf::SerializeToFrame(timestampedValue, rawOut[1]);
}


void ed::Subscribe(zmq::socket_t& socket, const dsb::model::Variable& variable)
{
    char header[HEADER_SIZE];
    CreateRawHeader(variable, header);
    socket.setsockopt(ZMQ_SUBSCRIBE, header, HEADER_SIZE);
}


void ed::Unsubscribe(zmq::socket_t& socket, const dsb::model::Variable& variable)
{
    char header[HEADER_SIZE];
    CreateRawHeader(variable, header);
    socket.setsockopt(ZMQ_UNSUBSCRIBE, header, HEADER_SIZE);
}
