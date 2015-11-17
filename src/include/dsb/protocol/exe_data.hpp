#ifndef DSB_PROTOCOL_EXE_DATA_HPP
#define DSB_PROTOCOL_EXE_DATA_HPP

#include <vector>
#include "zmq.hpp"
#include "dsb/model.hpp"


namespace dsb
{
namespace protocol
{

/**
\brief  Functions for constructing and parsing messages sent on the execution
        data channel, i.e. variable values.
*/
namespace exe_data
{
const size_t HEADER_SIZE = 4;

struct Message
{
    dsb::model::Variable variable;
    dsb::model::StepID timestepID;
    dsb::model::ScalarValue value;
};

Message ParseMessage(const std::vector<zmq::message_t>& rawMsg);

void CreateMessage(const Message& message, std::vector<zmq::message_t>& rawOut);

void Subscribe(zmq::socket_t& socket, const dsb::model::Variable& variable);

void Unsubscribe(zmq::socket_t& socket, const dsb::model::Variable& variable);

}}} // namespace
#endif // header guard
