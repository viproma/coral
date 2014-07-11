#include "dsb/protobuf.hpp"


void dsb::protobuf::SerializeToFrame(
    const google::protobuf::MessageLite& source,
    zmq::message_t& target)
{
    const auto size = source.ByteSize();
    target.rebuild(size);
    if (!source.SerializeToArray(target.data(), size)) {
        throw SerializationException(SerializationException::SERIALIZE);
    }
}


void dsb::protobuf::ParseFromFrame(
    const zmq::message_t& source,
    google::protobuf::MessageLite& target)
{
    if (!target.ParseFromArray(source.data(), source.size())) {
        throw SerializationException(SerializationException::PARSE);
    }
}
