/**
\file
\brief Main header file for coral::protobuf.
\copyright
    Copyright 2013-2017, SINTEF Ocean and the Coral contributors.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifndef CORAL_PROTOBUF_HPP
#define CORAL_PROTOBUF_HPP

#include <stdexcept>
#include "google/protobuf/message_lite.h"
#include "zmq.hpp"


namespace coral
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
    explicit SerializationException(const std::string& msg);
};


}}      // namespace
#endif  // header guard
