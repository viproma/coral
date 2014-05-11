/**
\file
\brief Main header file for dsb::comm.
*/
#ifndef DSB_COMM_HPP
#define DSB_COMM_HPP

#include <deque>
#include <string>
#include "zmq.hpp"


namespace dsb
{

/// Helper functions for communication over ZMQ sockets.
namespace comm
{


/**
\brief Sends a message.

The message content will be cleared on return.

\throws std::invalid_argument if `message` is empty.
*/
void Send(zmq::socket_t& socket, std::deque<zmq::message_t>& message);


/**
\brief  Sends an addressed message.

This function sends `message`, prepended with an envelope consisting of two
frames: a frame with the contents of `recipient` and an empty delimiter frame.

The message content will be cleared on return.

\throws std::invalid_argument if `recipient` or `message` is empty.
*/
void AddressedSend(
    zmq::socket_t& socket,
    const std::string& recipient,
    std::deque<zmq::message_t>& message);


/**
\brief Receives a message.

Existing message content will be overwritten.
*/
void Receive(
    zmq::socket_t& socket,
    std::deque<zmq::message_t>& message);


/**
\brief  Removes or moves the envelope from a message.

The envelope of a message are the message frames starting at the beginning
of the message and ending at the first empty frame, called a "delimiter".

If `message` is empty, or has no envelope (i.e., no delimiter), this
function returns without doing anything.  Otherwise, if `envelope` is not
null, all message frames up to, but not including, the delimiter are moved
to `envelope`. Any existing contents of `envelope` will be overwritten.
Finally, all frames up to and including the delimiter are removed from
`message`.

\returns The number of frames removed from `message`, including the delimiter.
*/
size_t PopMessageEnvelope(
    std::deque<zmq::message_t>& message,
    std::deque<zmq::message_t>* envelope = nullptr);


/// Returns the content of a message frame as a `std::string`.
std::string ToString(const zmq::message_t& frame);


}}      // namespace
#endif  // header guard
