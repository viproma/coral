/**
\file
\brief Contains the coral::net::Reactor class and related functionality.
\copyright
    Copyright 2013-2018, SINTEF Ocean and the Coral contributors.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifndef CORAL_NET_REACTOR_HPP
#define CORAL_NET_REACTOR_HPP

#include <chrono>
#include <functional>
#include <memory>
#include <vector>
#include <utility>

#include <zmq.hpp>
#include <coral/config.h>


namespace coral
{
namespace net
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
#ifdef _WIN32
    typedef SOCKET NativeSocket;
#else
    typedef int NativeSocket;
#endif

    typedef std::chrono::system_clock::time_point TimePoint;
    typedef std::function<void(Reactor&, zmq::socket_t&)> SocketHandler;
    typedef std::function<void(Reactor&, NativeSocket)> NativeSocketHandler;
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
    void RemoveSocket(zmq::socket_t& socket) noexcept;

    /// Adds a handler for the given native socket.
    void AddNativeSocket(NativeSocket socket, NativeSocketHandler handler);

    /**
    \brief  Removes all handlers for the given native socket.

    If this function is called by a socket handler, no more handlers will be
    called for the removed socket, even if the last poll indicated that it has
    incoming messages.

    If the given socket was never registered with AddNativeSocket(), this
    function simply returns without doing anything.
    */
    void RemoveNativeSocket(NativeSocket socket) noexcept;

    /// A number which will never be returned by AddTimer().
    static const int invalidTimerID;

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
    \brief  Resets the time to the next event for a timer.

    This function sets the elapsed time for the *current* iteration of a timer
    to zero.  It does not change the number of remaining events.

    \throws std::invalid_argument if `id` is not a valid timer ID.
    */
    void RestartTimerInterval(int id);

    /**
    \brief  Runs the messaging loop.

    This function returns when `Stop()` is called (by one of the socket/timer
    handlers) or when there are no sockets or timers left to wait on.

    If a socket/timer handler throws an exception, the messaging loop will stop
    and the exception will propagate out of `Run()`.

    \throws zmq::error_t if ZMQ reports an error.
    */
    void Run();

    /**
    \brief  Stops the messaging loop.

    This method may be called by a socket/timer handler, and will stop the
    messaging loop once that handler returns, causing `Run()` to return to
    its caller.
    */
    void Stop();

private:
    struct Timer
    {
        Timer(
            int id,
            TimePoint nextEventTime,
            std::chrono::milliseconds interval,
            int remaining,
            std::unique_ptr<TimerHandler> handler);

        CORAL_DEFINE_DEFAULT_MOVE(Timer, id, nextEventTime, interval, remaining, handler)

        int id;
        TimePoint nextEventTime;
        std::chrono::milliseconds interval;
        int remaining;
        std::unique_ptr<TimerHandler> handler;
    };

    void RestartTimerIntervals(
        std::vector<Timer>::iterator begin,
        std::vector<Timer>::iterator end);
    std::chrono::milliseconds TimeToNextEvent() const;
    void PerformNextEvent();

    // Rebuilds the list of poll items.
    void Rebuild();

    typedef std::pair<zmq::socket_t*, std::unique_ptr<SocketHandler>> SocketHandlerPair;
    typedef std::pair<NativeSocket, std::unique_ptr<NativeSocketHandler>> NativeSocketHandlerPair;
    std::vector<SocketHandlerPair> m_sockets;
    std::vector<NativeSocketHandlerPair> m_nativeSockets;
    std::vector<zmq::pollitem_t> m_pollItems;

    int m_nextTimerID;
    std::vector<Timer> m_timers;

    bool m_needsRebuild;
    bool m_running;
};


}}      // namespace
#endif  // header guard
