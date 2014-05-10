#ifndef DSB_COMM_HELPERS_HPP
#define DSB_COMM_HELPERS_HPP

#include <deque>
#include "zmq.hpp"


namespace dsb { namespace comm
{


void RecvMessage(
    zmq::socket_t& socket,
    std::deque<zmq::message_t>* message);

size_t PopMessageEnvelope(
    std::deque<zmq::message_t>& message,
    std::deque<zmq::message_t>* envelope = nullptr);


}}      // namespace
#endif  // header guard
