#ifdef _WIN32
#   define NOMINMAX
#endif

#include "dsb/comm.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>

#include "dsb/config.h"
#include "dsb/error.hpp"
#include "dsb/util.hpp"


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


bool dsb::comm::Receive(
    zmq::socket_t& socket,
    std::deque<zmq::message_t>& message,
    boost::chrono::milliseconds timeout)
{
    zmq::pollitem_t pollItem = { socket, 0, ZMQ_POLLIN, 0 };
    if (zmq::poll(&pollItem, 1, static_cast<long>(timeout.count())) == 0) {
        return false;
    } else {
        assert (pollItem.revents == ZMQ_POLLIN);
        dsb::comm::Receive(socket, message);
        return true;
    }
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


void dsb::comm::CopyMessage(
    std::deque<zmq::message_t>& source,
    std::deque<zmq::message_t>& target)
{
    target.resize(source.size());
    for (size_t i = 0; i < source.size(); ++i) {
        target[i].copy(&source[i]);
    }
}


void dsb::comm::CopyMessage(
    const std::deque<zmq::message_t>& source,
    std::deque<zmq::message_t>& target)
{
    target.clear();
    for (auto it = source.cbegin(); it != source.cend(); ++it) {
        target.push_back(zmq::message_t(it->size()));
        std::memcpy(target.back().data(), it->data(), it->size());
    }
}


std::string dsb::comm::ToString(const zmq::message_t& frame)
{
    return std::string(static_cast<const char*>(frame.data()), frame.size());
}


zmq::message_t dsb::comm::ToFrame(const std::string& s)
{
    auto msg = zmq::message_t(s.size());
    std::memcpy(msg.data(), s.data(), s.size());
    return msg;
}


std::string dsb::comm::LastEndpoint(zmq::socket_t& socket)
{
    const size_t MAX_ENDPOINT_SIZE = 257; // including terminating zero
    char buffer[MAX_ENDPOINT_SIZE];
    size_t length = MAX_ENDPOINT_SIZE;
    socket.getsockopt(ZMQ_LAST_ENDPOINT, buffer, &length);
    assert (length > 0 && buffer[length-1] == '\0');
    return std::string(buffer, length-1);
}


namespace dsb { namespace comm {

Reactor::Reactor()
    : m_nextTimerID(0),
      m_needsRebuild(false),
      m_continuePolling(false)
{ }


void Reactor::AddSocket(zmq::socket_t& socket, SocketHandler handler)
{
    m_sockets.push_back(std::make_pair(&socket, handler));
    m_needsRebuild = true;
}


void Reactor::RemoveSocket(zmq::socket_t& socket)
{
    // Actual removal is deferred to the next rebuild.  At this stage, we just
    // replace the socket pointer with null.
    for (auto it = m_sockets.begin(); it != m_sockets.end(); ++it) {
        if (it->first == &socket) it->first = nullptr;
    }
    m_needsRebuild = true;
}


namespace
{
    // We use templates to work around the fact that Reactor::Timer is private.
    template<typename T>
    bool EventTimeGreater(const T& a, const T& b)
    {
        return a.nextEventTime > b.nextEventTime;
    }

    template<typename T>
    void PushTimer(std::vector<T>& v, const T& t)
    {
        v.push_back(t);
        std::push_heap(std::begin(v), std::end(v), EventTimeGreater<T>);
    }

    template<typename T>
    void PopTimer(std::vector<T>& v)
    {
        std::pop_heap(std::begin(v), std::end(v), EventTimeGreater<T>);
        v.pop_back();
    }
}


int Reactor::AddTimer(
    boost::chrono::milliseconds interval,
    int count,
    TimerHandler handler)
{
    if (interval < boost::chrono::milliseconds(0)) {
        throw std::invalid_argument("Negative interval");
    }
    if (count == 0) {
        throw std::invalid_argument("Invalid timer count");
    }
    Timer t = {
        ++m_nextTimerID,
        boost::chrono::system_clock::now() + interval,
        interval,
        count,
        handler
    };
    PushTimer(m_timers, t);
    return t.id;
}


void Reactor::RemoveTimer(int id)
{
    const auto it = std::find_if(m_timers.begin(), m_timers.end(),
        [id](const Timer& t){ return t.id == id; });
    if (it == m_timers.end()) {
        throw std::invalid_argument("Invalid timer ID");
    }
    m_timers.erase(it);
    std::make_heap(std::begin(m_timers), std::end(m_timers), &EventTimeGreater<Timer>);
}


void Reactor::Run()
{
    m_continuePolling = true;
    do {
        if (m_needsRebuild) Rebuild();
        zmq::poll(m_pollItems.data(), m_pollItems.size(),
                  m_timers.empty() ? -1 : static_cast<long>(TimeToNextEvent().count()));
        for (size_t i = 0; i < m_pollItems.size(); ++i) {
            if ((m_pollItems[i].revents & ZMQ_POLLIN) && m_sockets[i].first != nullptr) {
                m_sockets[i].second(*this, *m_sockets[i].first);
            }
        }
        while (!m_timers.empty()
               && boost::chrono::system_clock::now() >= m_timers.front().nextEventTime) {
            PerformNextEvent();
        }
    } while (m_continuePolling);
}


void Reactor::Stop()
{
    m_continuePolling = false;
}


boost::chrono::milliseconds Reactor::TimeToNextEvent() const
{
    return std::max(
        boost::chrono::duration_cast<boost::chrono::milliseconds>(
            m_timers.front().nextEventTime - boost::chrono::system_clock::now()),
        boost::chrono::milliseconds(0));
}


void Reactor::PerformNextEvent()
{
    auto t = m_timers.front();
    assert (t.nextEventTime <= boost::chrono::system_clock::now());
    assert (t.remaining != 0);

    // We need to update the timer queue even if the handler throws.
    auto updateTimer = dsb::util::OnScopeExit([&] () {
        // Timer may already have been removed by handler,
        // in which case we do nothing.
        if (m_timers.front().id == t.id) {
            PopTimer(m_timers);
            if (t.remaining > 0) --t.remaining;
            if (t.remaining != 0) {
                t.nextEventTime += t.interval;
                PushTimer(m_timers, t);
            }
        }
    });
    t.handler(*this, t.id);
}


void Reactor::Rebuild()
{
    // Remove null sockets from m_sockets
    auto newEnd = std::remove_if(m_sockets.begin(), m_sockets.end(),
        [](const SocketHandlerPair& a) { return a.first == nullptr; });
    m_sockets.erase(newEnd, m_sockets.end());

    // Rebuild m_pollItems
    m_pollItems.clear();
    for (auto it = m_sockets.begin(); it != m_sockets.end(); ++it) {
        zmq::pollitem_t pi = { *it->first, 0, ZMQ_POLLIN, 0 };
        m_pollItems.push_back(pi);
    }
    m_needsRebuild = false;
}


}} // namespace
