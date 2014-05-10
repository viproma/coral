/**
\file
\brief  Functions for using ZMQ and Protobuf together.
*/
#ifndef DSB_COMM_PROTOBUF_HPP
#define DSB_COMM_PROTOBUF_HPP

#include "google/protobuf/message_lite.h"
#include "zmq.hpp"

namespace dsb { namespace comm
{


void SerializeToFrame(
    const google::protobuf::MessageLite& source,
    zmq::message_t* target);


void ParseFromFrame(
    const zmq::message_t& source,
    google::protobuf::MessageLite* target);


}}      // namespace
#endif  // header guard
