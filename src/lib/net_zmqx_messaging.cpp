/*
Copyright 2013-present, SINTEF Ocean.
This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifdef _WIN32
#   define NOMINMAX
#endif

#include <coral/net/zmqx.hpp>

#include <algorithm>
#include <coral/config.h>
#include <coral/error.hpp>


namespace
{
    bool PollSingleSocket(
        zmq::socket_t& socket,
        short events,
        std::chrono::milliseconds timeout)
    {
        const auto timeout_ms = std::max(static_cast<long>(timeout.count()), -1L);
        auto pollItem = zmq::pollitem_t{static_cast<void*>(socket), 0, events, 0};
        return zmq::poll(&pollItem, 1, timeout_ms) == 1;
    }
}


bool coral::net::zmqx::WaitForOutgoing(
    zmq::socket_t& socket,
    std::chrono::milliseconds timeout)
{
    return PollSingleSocket(socket, ZMQ_POLLOUT, timeout);
}


bool coral::net::zmqx::WaitForIncoming(
    zmq::socket_t& socket,
    std::chrono::milliseconds timeout)
{
    return PollSingleSocket(socket, ZMQ_POLLIN, timeout);
}


namespace
{
    void SendFrames(
        zmq::socket_t& socket,
        std::vector<zmq::message_t>& message,
        coral::net::zmqx::SendFlag flags)
    {
        assert (!message.empty());
        const auto last = --message.end();
        for (auto it = begin(message); it != last; ++it) {
            socket.send(*it, ZMQ_SNDMORE);
        }
        socket.send(
            *last,
            (flags & coral::net::zmqx::SendFlag::more) != coral::net::zmqx::SendFlag::none
                ? ZMQ_SNDMORE
                : 0);
        message.clear();
    }
}


void coral::net::zmqx::Send(
    zmq::socket_t& socket,
    std::vector<zmq::message_t>& message,
    SendFlag flags)
{
    CORAL_INPUT_CHECK(!message.empty());
    SendFrames(socket, message, flags);
    assert (message.empty());
}


void coral::net::zmqx::Receive(
    zmq::socket_t& socket,
    std::vector<zmq::message_t>& message)
{
    message.clear();
    do {
        message.emplace_back();
        socket.recv(&message.back());
    } while (message.back().more());
}


std::string coral::net::zmqx::ToString(const zmq::message_t& frame)
{
    return std::string(static_cast<const char*>(frame.data()), frame.size());
}


zmq::message_t coral::net::zmqx::ToFrame(const std::string& s)
{
    auto msg = zmq::message_t(s.size());
    std::memcpy(msg.data(), s.data(), s.size());
    return msg;
}
