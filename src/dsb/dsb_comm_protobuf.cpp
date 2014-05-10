#include "dsb/comm/protobuf.hpp"

#include "dsb/util/error.hpp"


void dsb::comm::SerializeToFrame(
    const google::protobuf::MessageLite& source,
    zmq::message_t* target)
{
    DSB_INPUT_CHECK(target != nullptr);
    const auto size = source.ByteSize();
    target->rebuild(size);
    source.SerializeToArray(target->data(), size);
}


void dsb::comm::ParseFromFrame(
    const zmq::message_t& source,
    google::protobuf::MessageLite* target)
{
    DSB_INPUT_CHECK(target != nullptr);
    target->ParseFromArray(source.data(), source.size());
}
