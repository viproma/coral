#include "dsb/protobuf.hpp"

#include "dsb/util/error.hpp"


namespace dsb { namespace protobuf {


void SerializeToFrame(
    const google::protobuf::MessageLite& source,
    zmq::message_t* target)
{
    DSB_INPUT_CHECK(target != nullptr);
    const auto size = source.ByteSize();
    target->rebuild(size);
    source.SerializeToArray(target->data(), size);
}


void ParseFromFrame(
    const zmq::message_t& source,
    google::protobuf::MessageLite* target)
{
    DSB_INPUT_CHECK(target != nullptr);
    target->ParseFromArray(source.data(), source.size());
}


}} // namespace
