#include "dsb/net/zmqx.hpp"

#include "dsb/config.h"
#include "dsb/error.hpp"


namespace
{
    bool PollSingleSocket(
        zmq::socket_t& socket,
        short events,
        std::chrono::milliseconds timeout)
    {
        assert(timeout >= std::chrono::milliseconds(0));
        auto pollItem = zmq::pollitem_t{static_cast<void*>(socket), 0, events, 0};
        return zmq::poll(&pollItem, 1, static_cast<long>(timeout.count())) == 1;
    }
}


bool dsb::net::zmqx::WaitForOutgoing(
    zmq::socket_t& socket,
    std::chrono::milliseconds timeout)
{
    DSB_INPUT_CHECK(timeout >= std::chrono::milliseconds(0));
    return PollSingleSocket(socket, ZMQ_POLLOUT, timeout);
}


bool dsb::net::zmqx::WaitForIncoming(
    zmq::socket_t& socket,
    std::chrono::milliseconds timeout)
{
    DSB_INPUT_CHECK(timeout >= std::chrono::milliseconds(0));
    return PollSingleSocket(socket, ZMQ_POLLIN, timeout);
}


namespace
{
    void SendFrames(
        zmq::socket_t& socket,
        std::vector<zmq::message_t>& message,
        dsb::net::zmqx::SendFlag flags)
    {
        assert (!message.empty());
        const auto last = --message.end();
        for (auto it = begin(message); it != last; ++it) {
            socket.send(*it, ZMQ_SNDMORE);
        }
        socket.send(
            *last,
            (flags & dsb::net::zmqx::SendFlag::more) != dsb::net::zmqx::SendFlag::none
                ? ZMQ_SNDMORE
                : 0);
        message.clear();
    }
}


void dsb::net::zmqx::Send(
    zmq::socket_t& socket,
    std::vector<zmq::message_t>& message,
    SendFlag flags)
{
    DSB_INPUT_CHECK(!message.empty());
    SendFrames(socket, message, flags);
    assert (message.empty());
}


void dsb::net::zmqx::Receive(
    zmq::socket_t& socket,
    std::vector<zmq::message_t>& message)
{
    message.clear();
    do {
        message.emplace_back();
        socket.recv(&message.back());
    } while (message.back().more());
}


std::string dsb::net::zmqx::ToString(const zmq::message_t& frame)
{
    return std::string(static_cast<const char*>(frame.data()), frame.size());
}


zmq::message_t dsb::net::zmqx::ToFrame(const std::string& s)
{
    auto msg = zmq::message_t(s.size());
    std::memcpy(msg.data(), s.data(), s.size());
    return msg;
}
