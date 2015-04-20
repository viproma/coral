#ifdef _WIN32
#   define NOMINMAX
#endif
#include "dsb/comm/reactor.hpp"

#include <algorithm>
#include <stdexcept>
#include "dsb/compat_helpers.hpp"
#include "dsb/util.hpp"


namespace dsb
{
namespace comm
{


Reactor::Reactor()
    : m_nextTimerID(0),
      m_needsRebuild(false),
      m_continuePolling(false)
{ }


void Reactor::AddSocket(zmq::socket_t& socket, SocketHandler handler)
{
    m_sockets.push_back(
        std::make_pair(&socket, std::make_unique<SocketHandler>(std::move(handler))));
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
    void PushTimer(std::vector<T>& v, T t)
    {
        v.push_back(std::move(t));
        std::push_heap(std::begin(v), std::end(v), EventTimeGreater<T>);
    }

    template<typename T>
    void PopTimer(std::vector<T>& v)
    {
        std::pop_heap(std::begin(v), std::end(v), EventTimeGreater<T>);
        v.pop_back();
    }

    template<typename T>
    void HeapifyTimers(std::vector<T>& v)
    {
        std::make_heap(std::begin(v), std::end(v), &EventTimeGreater<T>);
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
    const auto id = ++m_nextTimerID;
    PushTimer(m_timers, Timer(
        id,
        boost::chrono::system_clock::now() + interval,
        interval,
        count,
        std::make_unique<TimerHandler>(std::move(handler))));
    return id;
}


void Reactor::RemoveTimer(int id)
{
    const auto it = std::find_if(m_timers.begin(), m_timers.end(),
        [id](const Timer& t){ return t.id == id; });
    if (it == m_timers.end()) {
        throw std::invalid_argument("Invalid timer ID");
    }
    m_timers.erase(it);
    HeapifyTimers(m_timers);
}


void Reactor::Run()
{
    ResetTimers();
    m_continuePolling = true;
    do {
        if (m_needsRebuild) Rebuild();
        zmq::poll(m_pollItems.data(), m_pollItems.size(),
                  m_timers.empty() ? -1 : static_cast<long>(TimeToNextEvent().count()));
        for (size_t i = 0; i < m_pollItems.size(); ++i) {
            if ((m_pollItems[i].revents & ZMQ_POLLIN) && m_sockets[i].first != nullptr) {
                (*m_sockets[i].second)(*this, *m_sockets[i].first);
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


void Reactor::ResetTimers()
{
    const auto t0 = boost::chrono::system_clock::now();
    for (auto it = std::begin(m_timers); it != std::end(m_timers); ++it) {
        it->nextEventTime = t0 + it->interval;
    }
    HeapifyTimers(m_timers);
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
    assert (m_timers.front().nextEventTime <= boost::chrono::system_clock::now());
    assert (m_timers.front().remaining != 0);

    // The handler may delete the timer, thus also deleting some information
    // we need.  Therefore, we copy that info first.  We also need to *move*
    // the handler function object out here, so it doesn't inadvertently delete
    // itself.
    const auto id = m_timers.front().id;
    auto handler = std::move(m_timers.front().handler);

    // We use a scope guard, since the handler may throw.
    auto updateTimer = dsb::util::OnScopeExit([&] () {
        // The timer may already have been removed by the handler, in which case
        // we do nothing.
        if (m_timers.front().id == id) {
            auto t = std::move(m_timers.front());
            t.handler = std::move(handler);
            PopTimer(m_timers);
            if (t.remaining > 0) --t.remaining;
            if (t.remaining != 0) {
                t.nextEventTime += t.interval;
                PushTimer(m_timers, std::move(t));
            }
        }
    });
    (*handler)(*this, id);
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


Reactor::Timer::Timer(
    int id_,
    TimePoint nextEventTime_,
    boost::chrono::milliseconds interval_,
    int remaining_,
    std::unique_ptr<TimerHandler> handler_)
    : id(id_),
      nextEventTime(nextEventTime_),
      interval(interval_),
      remaining(remaining_),
      handler(std::move(handler_))
{
}


#if DSB_NO_AUTOGENERATED_MOVE

Reactor::Timer::Timer(Timer&& other) DSB_NOEXCEPT
    : id(other.id),
      nextEventTime(other.nextEventTime),
      interval(other.interval),
      remaining(other.remaining),
      handler(std::move(other.handler))
{
}

Reactor::Timer& Reactor::Timer::operator=(Timer&& other) DSB_NOEXCEPT
{
    id = other.id;
    nextEventTime = other.nextEventTime;
    interval = other.interval;
    remaining = other.remaining;
    handler = std::move(other.handler);
    return *this;
}

#endif // DSB_NO_AUTOGENERATED_MOVE


}} // namespace
