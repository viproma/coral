#include "dsb/comm/helpers.hpp"

#include <algorithm>
#include <cstring>
#include "dsb/util/error.hpp"


void dsb::comm::Send(zmq::socket_t& socket, std::deque<zmq::message_t>& message)
{
    DSB_INPUT_CHECK(!message.empty());
    for (auto it = message.begin(); ; ) {
        auto m = it++;
        if (it == message.end()) {
            socket.send(*m);
            break;
        } else {
            socket.send(*m, ZMQ_SNDMORE);
        }
    }
    message.clear();
}


void dsb::comm::AddressedSend(
    zmq::socket_t& socket,
    const std::string& recipient,
    std::deque<zmq::message_t>& message)
{
    DSB_INPUT_CHECK(!recipient.empty());
    DSB_INPUT_CHECK(!message.empty());

    zmq::message_t env(recipient.size());
    std::memcpy(env.data(), recipient.data(), recipient.size());
    socket.send(env, ZMQ_SNDMORE);
    socket.send(env, ZMQ_SNDMORE); // env is empty after the previous send
    Send(socket, message);
}


void dsb::comm::Receive(
    zmq::socket_t& socket,
    std::deque<zmq::message_t>& message)
{
    message.clear();
    do {
        message.emplace_back();
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
