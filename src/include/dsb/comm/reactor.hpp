/**
\file
\brief Contains the dsb::comm::Reactor class and related functionality.
*/
#ifndef DSB_COMM_REACTOR_HPP
#define DSB_COMM_REACTOR_HPP

#include <chrono>
#include <functional>
#include <memory>
#include <vector>
#include <utility>

#include "zmq.hpp"
#include "dsb/config.h"


namespace dsb
{
namespace comm
{


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
    typedef std::chrono::system_clock::time_point TimePoint;
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

    If the given socket was never registered with AddSocket(), this function
    simply returns without doing anything.
    */
    void RemoveSocket(zmq::socket_t& socket) DSB_NOEXCEPT;

    /**
    \brief  Adds a timer.

    If the messaging loop is running, the first event will be triggered at
    `interval` after this function is called.  Otherwise, the first event will
    be triggered `interval` after Run() is called.

    \param [in] interval    The time between events.
    \param [in] count       The total number of events.  If negative, the timer
                            runs indefinitely.
    \param [in] handler     The event handler.

    \returns an index which may later be used to remove the timer.
    \throws std::invalid_argument if `count` is zero or `interval` is negative.
    */
    int AddTimer(
        std::chrono::milliseconds interval,
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
    void ResetTimers();
    std::chrono::milliseconds TimeToNextEvent() const;
    void PerformNextEvent();

    // Rebuilds the list of poll items.
    void Rebuild();

    typedef std::pair<zmq::socket_t*, std::unique_ptr<SocketHandler>> SocketHandlerPair;
    std::vector<SocketHandlerPair> m_sockets;
    std::vector<zmq::pollitem_t> m_pollItems;

    int m_nextTimerID;

    struct Timer
    {
        Timer(
            int id,
            TimePoint nextEventTime,
            std::chrono::milliseconds interval,
            int remaining,
            std::unique_ptr<TimerHandler> handler);

#if DSB_HAS_EXPLICIT_DEFAULTED_DELETED_FUNCS
        Timer(Timer&&) = default;
        Timer& operator=(Timer&&) = default;
#else
        Timer(Timer&&) DSB_NOEXCEPT;
        Timer& operator=(Timer&&) DSB_NOEXCEPT;
#endif

        int id;
        TimePoint nextEventTime;
        std::chrono::milliseconds interval;
        int remaining;
        std::unique_ptr<TimerHandler> handler;
    };
    std::vector<Timer> m_timers;

    bool m_needsRebuild;
    bool m_continuePolling;
};


}}      // namespace
#endif  // header guard
