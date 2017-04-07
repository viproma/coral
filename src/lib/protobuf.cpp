/*
Copyright 2013-2017, SINTEF Ocean and the Coral contributors.
This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#include <coral/protobuf.hpp>

#include <boost/numeric/conversion/cast.hpp>


void coral::protobuf::SerializeToFrame(
    const google::protobuf::MessageLite& source,
    zmq::message_t& target)
{
    const auto size = source.ByteSize();
    assert (size >= 0);
    target.rebuild(size);
    if (!source.SerializeToArray(target.data(), size)) {
        throw SerializationException("Failed to serialize message");
    }
}


void coral::protobuf::ParseFromFrame(
    const zmq::message_t& source,
    google::protobuf::MessageLite& target)
{
    if (!target.ParseFromArray(
            source.data(),
            boost::numeric_cast<int>(source.size()))) {
        throw SerializationException("Failed to parse message");
    }
}


coral::protobuf::SerializationException::SerializationException(const std::string& msg)
    : std::runtime_error(msg)
{
}
