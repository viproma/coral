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


/// Serializes a Protobuf message into a ZMQ message.
void SerializeToFrame(
    const google::protobuf::MessageLite& source,
    zmq::message_t& target);


/// Deserializes a Protobuf message from a ZMQ message.
void ParseFromFrame(
    const zmq::message_t& source,
    google::protobuf::MessageLite& target);


/// Exception that signals failure to serialize or deserialize a message.
class SerializationException : public std::runtime_error
{
public:
    enum Action { SERIALIZE, PARSE };

    SerializationException(Action action)
        : std::runtime_error(action == SERIALIZE ? "Failed to serialize message"
                                                 : "Failed to parse message")
    { }
};


}}      // namespace
#endif  // header guard
