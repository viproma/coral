/**
\file
\brief  Functions for using ZMQ and Protobuf together.
*/
#ifndef DSB_PROTOBUF_HPP
#define DSB_PROTOBUF_HPP

#include "google/protobuf/message_lite.h"
#include "zmq.hpp"


namespace dsb { namespace protobuf
{


/**
\brief  Serializes a Protobuf message into a ZMQ message.
\throws std::invalid_argument if `target` is null.
*/
void SerializeToFrame(
    const google::protobuf::MessageLite& source,
    zmq::message_t* target);


/**
\brief  Deserializes a Protobuf message from a ZMQ message.
\throws std::invalid_argument if `target` is null.
*/
void ParseFromFrame(
    const zmq::message_t& source,
    google::protobuf::MessageLite* target);


}}      // namespace
#endif  // header guard
