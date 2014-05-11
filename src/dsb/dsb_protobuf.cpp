#include "dsb/protobuf.hpp"


void dsb::protobuf::SerializeToFrame(
    const google::protobuf::MessageLite& source,
    zmq::message_t& target)
{
    const auto size = source.ByteSize();
    target.rebuild(size);
    source.SerializeToArray(target.data(), size);
}


void dsb::protobuf::ParseFromFrame(
    const zmq::message_t& source,
    google::protobuf::MessageLite& target)
{
    target.ParseFromArray(source.data(), source.size());
}
