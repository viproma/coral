/**
\file
\brief  Module header for coral::protocol::exe_data
\copyright
    Copyright 2013-present, SINTEF Ocean.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifndef CORAL_PROTOCOL_EXE_DATA_HPP
#define CORAL_PROTOCOL_EXE_DATA_HPP

#include <vector>
#include <zmq.hpp>
#include <coral/model.hpp>


namespace coral
{
namespace protocol
{

/**
\brief  Functions for constructing and parsing messages sent on the execution
        data channel, i.e. variable values.
*/
namespace exe_data
{
const size_t HEADER_SIZE = 6;

struct Message
{
    coral::model::Variable variable;
    coral::model::StepID timestepID;
    coral::model::ScalarValue value;
};

Message ParseMessage(const std::vector<zmq::message_t>& rawMsg);

void CreateMessage(const Message& message, std::vector<zmq::message_t>& rawOut);

void Subscribe(zmq::socket_t& socket, const coral::model::Variable& variable);

void Unsubscribe(zmq::socket_t& socket, const coral::model::Variable& variable);

}}} // namespace
#endif // header guard
