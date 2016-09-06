/**
\file
\brief  Helper functions for creating, sending, receiving and interpreting
        ZMQ messages.
*/
#ifndef DSB_COMM_MESSAGING_HPP
#define DSB_COMM_MESSAGING_HPP

#include <chrono>
#include <cstring>
#include <string>
#include <vector>

#include "zmq.hpp"
#include "dsb/config.h"


namespace dsb
{
namespace comm
{


/**
\brief  Waits up to `timeout` milliseconds to see if a message may be enqueued
        on `socket`.

\returns whether a message may be immediately enqueued on `socket`.
\throws std::invalid_argument if `timeout` is negative.
\throws zmq::error_t on communications error.
*/
bool WaitForOutgoing(zmq::socket_t& socket, std::chrono::milliseconds timeout);


/**
\brief  Waits up to `timeout` milliseconds for incoming messages on `socket`.

\returns whether there are incoming messages on `socket`.
\throws std::invalid_argument if `timeout` is negative.
\throws zmq::error_t on communications error.
*/
bool WaitForIncoming(zmq::socket_t& socket, std::chrono::milliseconds timeout);


/// Flags for the Send() function
enum class SendFlag : int
{
    /// No flags are set.
    none = 0,

    /**
    \brief  The frames being sent are part of a multiframe message, and more
            frames are coming.
    */
    more = 1
};
DSB_DEFINE_BITWISE_ENUM_OPERATORS(SendFlag)


/**
\brief Sends a message.

The message content will be cleared on return.

\throws std::invalid_argument if `message` is empty.
\throws zmq::error_t on failure to send a message frame.
*/
void Send(
    zmq::socket_t& socket,
    std::vector<zmq::message_t>& message,
    SendFlag flags = SendFlag::none);


/**
\brief  Sends an addressed message.

This function sends a multipart message consisting of the frames in `envelope`
followed by an empty delimiter frame and the frames in `body`.

Both `envelope` and `body` will be cleared on return.

\throws std::invalid_argument if either of `envelope` or `body` are empty.
\throws zmq::error_t on failure to send a message frame.
*/
void AddressedSend(
    zmq::socket_t& socket,
    std::vector<zmq::message_t>& envelope,
    std::vector<zmq::message_t>& body);


/**
\brief Receives a message.

Existing message content will be overwritten.

\throws zmq::error_t on failure to receive a message frame.
*/
void Receive(
    zmq::socket_t& socket,
    std::vector<zmq::message_t>& message);


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
    std::vector<zmq::message_t>& message,
    std::vector<zmq::message_t>* envelope = nullptr);


/**
\brief  Makes a copy of a multipart message.

This function will resize `target` to the same size as `source` and then make
each element in `target` a copy of the corresponding element in `source` by
using `zmq::message_t::copy()`.  Any previous contents of `target` will
be replaced.

\throws zmq::error_t if `zmq::message_t::copy()` fails.
*/
void CopyMessage(
    std::vector<zmq::message_t>& source,
    std::vector<zmq::message_t>& target);


/**
\brief  Makes a copy of a `const` multipart message.

This function performs the same tasks as the non-`const` CopyMessage() function,
except that it performs a raw binary copy of the data in each frame rather than
using the `zmq::message_t::copy()` method.  (The latter can only be called on
non-`const` frames.)  This may have a negative impact on performance.
*/
void CopyMessage(
    const std::vector<zmq::message_t>& source,
    std::vector<zmq::message_t>& target);



/// Returns the content of a message frame as a `std::string`.
std::string ToString(const zmq::message_t& frame);


/// Returns a message frame whose contents are equal to `s`.
zmq::message_t ToFrame(const std::string& s);


/**
\brief  Returns a message frame which contains the raw binary representation
        of the given value.

To avoid issues with different endianness, word size, etc., such messages should
only be sent between threads in one process, and not between processes or
across the network.  The function must only be used with POD types.
*/
template<typename T>
zmq::message_t EncodeRawDataFrame(const T& value)
{
    auto frame = zmq::message_t(sizeof(value));
    std::memcpy(frame.data(), &value, sizeof(value));
    return frame;
}


/**
\brief  Returns a value of type `T` created by making a raw binary copy of the
        contents of the given frame.

To avoid issues with different endianness, word size, etc., such messages should
only be sent between threads in one process, and not between processes or
across the network.  The function must only be used with POD types.

\pre `frame.size() == sizeof(T)`
*/
template<typename T>
T DecodeRawDataFrame(const zmq::message_t& frame)
{
    T value;
    assert (frame.size() == sizeof(value));
    std::memcpy(&value, frame.data(), sizeof(value));
    return value;
}


}} // namespace
#endif // header guard
