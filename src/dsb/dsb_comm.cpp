#include "dsb/comm.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include "dsb/config.h"
#include "dsb/error.hpp"


namespace
{
    void SendFrames(
        zmq::socket_t& socket,
        std::deque<zmq::message_t>& message,
        bool moreComing)
    {
        assert (!message.empty());
        for (auto it = message.begin(); ; ) {
            auto m = it++;
            if (it == message.end()) {
                if (moreComing) socket.send(*m, ZMQ_SNDMORE);
                else            socket.send(*m);
                break;
            } else {
                socket.send(*m, ZMQ_SNDMORE);
            }
        }
        message.clear();
    }
}

void dsb::comm::Send(zmq::socket_t& socket, std::deque<zmq::message_t>& message)
{
    DSB_INPUT_CHECK(!message.empty());
    SendFrames(socket, message, false);
    assert (message.empty());
}


void dsb::comm::AddressedSend(
    zmq::socket_t& socket,
    std::deque<zmq::message_t>& envelope,
    std::deque<zmq::message_t>& body)
{
    DSB_INPUT_CHECK(!envelope.empty());
    DSB_INPUT_CHECK(!body.empty());
    SendFrames(socket, envelope, true);
    socket.send("", 0, ZMQ_SNDMORE);
    SendFrames(socket, body, false);
    assert (envelope.empty());
    assert (body.empty());
}


void dsb::comm::Receive(
    zmq::socket_t& socket,
    std::deque<zmq::message_t>& message)
{
    message.clear();
    do {
#if DSB_USE_MSVC_EMPLACE_WORKAROUND
        message.emplace_back(zmq::message_t());
#else
        message.emplace_back();
#endif
        socket.recv(&message.back());
    } while (message.back().more());
}


size_t dsb::comm::PopMessageEnvelope(
    std::deque<zmq::message_t>& message,
    std::deque<zmq::message_t>* envelope)
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


std::string dsb::comm::ToString(const zmq::message_t& frame)
{
    return std::string(static_cast<const char*>(frame.data()), frame.size());
}
