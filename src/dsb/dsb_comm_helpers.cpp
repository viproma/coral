#include "dsb/comm/helpers.hpp"

#include <algorithm>
#include "dsb/util/error.hpp"


void dsb::comm::RecvMessage(
    zmq::socket_t& socket,
    std::deque<zmq::message_t>* message)
{
    DSB_INPUT_CHECK(message != nullptr);
    message->clear();
    do {
        message->emplace_back();
        socket.recv(&message->back());
    } while (message->back().more());
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
