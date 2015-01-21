/**
\file
\brief Main header file for dsb::comm.
*/
#ifndef DSB_COMM_HPP
#define DSB_COMM_HPP

#include <deque>
#include <functional>
#include <string>
#include <vector>
#include <utility>

#include "boost/chrono/duration.hpp"
#include "boost/chrono/system_clocks.hpp"
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
\throws zmq::error_t on failure to send a message frame.
*/
void Send(zmq::socket_t& socket, std::deque<zmq::message_t>& message);


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
    std::deque<zmq::message_t>& envelope,
    std::deque<zmq::message_t>& body);


/**
\brief Receives a message.

Existing message content will be overwritten.

\throws zmq::error_t on failure to receive a message frame.
*/
void Receive(
    zmq::socket_t& socket,
    std::deque<zmq::message_t>& message);


/**
\brief Receives a message, assuming one arrives before the timeout is reached.

Existing message content will be overwritten.

\returns `true` if a message was received, or `false` if the function timed out.
\throws zmq::error_t on failure to receive a message frame.
*/
bool Receive(
    zmq::socket_t& socket,
    std::deque<zmq::message_t>& message,
    boost::chrono::milliseconds timeout);


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


/**
\brief  Makes a copy of a multipart message.

This function will resize `target` to the same size as `source` and then make
each element in `target` a copy of the corresponding element in `source` by
using `zmq::message_t::copy()`.  Any previous contents of `target` will
be replaced.

\throws zmq::error_t if `zmq::message_t::copy()` fails.
*/
void CopyMessage(
    std::deque<zmq::message_t>& source,
    std::deque<zmq::message_t>& target);


/**
\brief  Makes a copy of a `const` multipart message.

This function performs the same tasks as the non-`const` CopyMessage() function,
except that it performs a raw binary copy of the data in each frame rather than
using the `zmq::message_t::copy()` method.  (The latter can only be called on
non-`const` frames.)  This may have a negative impact on performance.
*/
void CopyMessage(
    const std::deque<zmq::message_t>& source,
    std::deque<zmq::message_t>& target);



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


/**
\brief Returns the value of the ZMQ_LAST_ENDPOINT socket property.
\throws zmq::error_t if ZMQ reports an error.
*/
std::string LastEndpoint(zmq::socket_t& socket);


/**
\brief  An implementation of the reactor pattern.

This class polls a number of sockets, and when a socket has incoming messages,
it dispatches to the registered handler function(s) for that socket.
If multiple sockets have incoming messages, or there are multiple handlers for
one socket, the functions are called in the order they were added.

It also supports timed events, where a handler function is called a certain
number of times (or indefinitely) with a fixed time interval.  Timers are only
active when the messaging loop is running, i.e. between Run() and Stop().
*/
class Reactor
{
public:
    typedef boost::chrono::system_clock::time_point TimePoint;
    typedef std::function<void(Reactor&, zmq::socket_t&)> SocketHandler;
    typedef std::function<void(Reactor&, int)> TimerHandler;

    Reactor();

    /// Adds a handler for the given socket.
    void AddSocket(zmq::socket_t& socket, SocketHandler handler);

    /**
    \brief  Removes all handlers for the given socket.

    If this function is called by a socket handler, no more handlers will be
    called for the removed socket, even if the last poll indicated that it has
    incoming messages.
    */
    void RemoveSocket(zmq::socket_t& socket);

    /**
    \brief  Adds a timer.

    \param [in] interval    The time between events.
    \param [in] count       The total number of events.  If negative, the timer
                            runs indefinitely.
    \param [in] handler     The event handler.

    \returns an index which may later be used to remove the timer.
    \throws std::invalid_argument if `count` is zero or `interval` is negative.
    */
    int AddTimer(
        boost::chrono::milliseconds interval,
        int count,
        TimerHandler handler);

    /**
    \brief  Removes a timer.
    \throws std::invalid_argument if `id` is not a valid timer ID.
    */
    void RemoveTimer(int id);

    /**
    \brief  Runs the messaging loop.

    This function does not return before Stop() is called (by one of the
    socket/timer handlers) or an error occurs.

    If a socket/timer handler throws an exception, the messaging loop will stop
    and the exception will propagate out of Run().

    \throws zmq::error_t if ZMQ reports an error.
    */
    void Run();

    /**
    \brief  Stops the messaging loop.

    This method may be called by a socket/timer handler, and will exit the
    messaging loop once all handlers for the current event(s) have been called.
    */
    void Stop();

private:
    boost::chrono::milliseconds TimeToNextEvent() const;
    void PerformNextEvent();

    // Rebuilds the list of poll items.
    void Rebuild();

    typedef std::pair<zmq::socket_t*, SocketHandler> SocketHandlerPair;
    std::vector<SocketHandlerPair> m_sockets;
    std::vector<zmq::pollitem_t> m_pollItems;

    int m_nextTimerID;
    struct Timer
    {
        int id;
        TimePoint nextEventTime;
        boost::chrono::milliseconds interval;
        int remaining;
        TimerHandler handler;
    };
    std::vector<Timer> m_timers;

    bool m_needsRebuild;
    bool m_continuePolling;
};


}}      // namespace
#endif  // header guard
