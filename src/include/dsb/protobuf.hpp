/**
\file
\brief Main header file for dsb::protobuf.
*/
#ifndef DSB_PROTOBUF_HPP
#define DSB_PROTOBUF_HPP

#include <stdexcept>
#include "google/protobuf/message_lite.h"
#include "zmq.hpp"


namespace dsb
{

/// Functions for using Protobuf with ZMQ.
namespace protobuf
{


/**
\brief  Serializes a Protobuf message into a ZMQ message.

Any existing contents of `target` will be replaced.

\throws SerializationException on failure.
*/
void SerializeToFrame(
    const google::protobuf::MessageLite& source,
    zmq::message_t& target);


/**
\brief  Deserializes a Protobuf message from a ZMQ message.
\throws SerializationException on failure.
*/
void ParseFromFrame(
    const zmq::message_t& source,
    google::protobuf::MessageLite& target);


/// Exception that signals failure to serialize or deserialize a message.
class SerializationException : public std::runtime_error
{
public:
    SerializationException(const std::string& msg);
};


}}      // namespace
#endif  // header guard
