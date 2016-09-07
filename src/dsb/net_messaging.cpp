#include "dsb/net/messaging.hpp"

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


bool dsb::net::WaitForOutgoing(
    zmq::socket_t& socket,
    std::chrono::milliseconds timeout)
{
    DSB_INPUT_CHECK(timeout >= std::chrono::milliseconds(0));
    return PollSingleSocket(socket, ZMQ_POLLOUT, timeout);
}


bool dsb::net::WaitForIncoming(
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
        dsb::net::SendFlag flags)
    {
        assert (!message.empty());
        const auto last = --message.end();
        for (auto it = begin(message); it != last; ++it) {
            socket.send(*it, ZMQ_SNDMORE);
        }
        socket.send(
            *last,
            (flags & dsb::net::SendFlag::more) != dsb::net::SendFlag::none
                ? ZMQ_SNDMORE
                : 0);
        message.clear();
    }
}


void dsb::net::Send(
    zmq::socket_t& socket,
    std::vector<zmq::message_t>& message,
    SendFlag flags)
{
    DSB_INPUT_CHECK(!message.empty());
    SendFrames(socket, message, flags);
    assert (message.empty());
}


void dsb::net::AddressedSend(
    zmq::socket_t& socket,
    std::vector<zmq::message_t>& envelope,
    std::vector<zmq::message_t>& body)
{
    DSB_INPUT_CHECK(!envelope.empty());
    DSB_INPUT_CHECK(!body.empty());
    SendFrames(socket, envelope, SendFlag::more);
    socket.send("", 0, ZMQ_SNDMORE);
    SendFrames(socket, body, SendFlag::none);
    assert (envelope.empty());
    assert (body.empty());
}


void dsb::net::Receive(
    zmq::socket_t& socket,
    std::vector<zmq::message_t>& message)
{
    message.clear();
    do {
        message.emplace_back();
        socket.recv(&message.back());
    } while (message.back().more());
}


size_t dsb::net::PopMessageEnvelope(
    std::vector<zmq::message_t>& message,
    std::vector<zmq::message_t>* envelope)
{
    auto delim = std::find_if(message.begin(), message.end(),
                              [](const zmq::message_t& m) { return m.size() == 0; });
    if (delim == message.end()) {
        if (envelope) envelope->clear();
        return 0;
    }
    const auto envSize = delim - message.begin();
    if (envelope) {
        envelope->resize(envSize);
        std::move(message.begin(), delim, envelope->begin());
    }
    message.erase(message.begin(), ++delim);
    return envSize + 1;
}


void dsb::net::CopyMessage(
    std::vector<zmq::message_t>& source,
    std::vector<zmq::message_t>& target)
{
    target.resize(source.size());
    for (size_t i = 0; i < source.size(); ++i) {
        target[i].copy(&source[i]);
    }
}


void dsb::net::CopyMessage(
    const std::vector<zmq::message_t>& source,
    std::vector<zmq::message_t>& target)
{
    target.clear();
    for (auto it = source.cbegin(); it != source.cend(); ++it) {
        target.push_back(zmq::message_t(it->size()));
        std::memcpy(target.back().data(), it->data(), it->size());
    }
}


std::string dsb::net::ToString(const zmq::message_t& frame)
{
    return std::string(static_cast<const char*>(frame.data()), frame.size());
}


zmq::message_t dsb::net::ToFrame(const std::string& s)
{
    auto msg = zmq::message_t(s.size());
    std::memcpy(msg.data(), s.data(), s.size());
    return msg;
}
