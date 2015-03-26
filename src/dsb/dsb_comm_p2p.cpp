#define NOMINMAX
#include "dsb/comm/p2p.hpp"

#include <algorithm>
#include <deque>
#include <cstring>
#include <limits>
#include <utility>

#include "boost/lexical_cast.hpp"
#include "boost/thread.hpp"
#include "dsb/comm/messaging.hpp"
#include "dsb/comm/util.hpp"
#include "dsb/config.h"
#include "dsb/error.hpp"
#include "dsb/util.hpp"


namespace dsb
{
namespace comm
{


namespace
{
    void EnforceRouter(zmq::socket_t& socket)
    {
        int socketType = -1;
        size_t socketTypeLen = sizeof(socketType);
        socket.getsockopt(ZMQ_TYPE, &socketType, &socketTypeLen);
        assert(socketTypeLen == sizeof(socketType));
        if (socketType != ZMQ_ROUTER) {
            throw std::invalid_argument("Not a ROUTER socket");
        }
    }

    void SwapEnvelopes(std::deque<zmq::message_t>& msg)
    {
        // TODO: This is currently somewhat limited in that it can only deal
        // with single-identity envelopes.
        if (msg.size() >= 4 && msg[1].size() == 0 && msg[3].size() == 0) {
            std::swap(msg[0], msg[2]);
        } else assert (!"Received a message with the wrong envelope format");
    }

    void SwapEnvelopesAndTransfer(zmq::socket_t& src, zmq::socket_t& tgt)
    {
        std::deque<zmq::message_t> msg;
        dsb::comm::Receive(src, msg);
        SwapEnvelopes(msg);
        dsb::comm::Send(tgt, msg);
    }
}


void P2PProxy(
    zmq::socket_t& routerSocket,
    zmq::socket_t* controlSocket,
    boost::chrono::milliseconds timeout)
{
    EnforceRouter(routerSocket);
    DSB_INPUT_CHECK(timeout == NEVER_TIMEOUT
        || (timeout.count() > 0 && timeout.count() < std::numeric_limits<long>::max()));
    const auto timeoutL = static_cast<long>(timeout.count());
    zmq::pollitem_t pollItems[2] = {
        { routerSocket,   0, ZMQ_POLLIN, 0 },
        { *controlSocket, 0, ZMQ_POLLIN, 0 }
    };
    const size_t nSockets = controlSocket ? 2 : 1;
    zmq::message_t controlMsg;
    try {
        for (;;) {
            const auto nEvents = zmq::poll(pollItems, nSockets, timeoutL);
            if (nEvents == 0) {
                return; // timed out
            }
            if (pollItems[1].revents & ZMQ_POLLIN) {
                controlSocket->recv(&controlMsg);
                if (controlMsg.size() == 9
                        && std::memcmp(controlMsg.data(), "TERMINATE", 9) == 0) {
                    return;
                } else {
                    assert(!"P2PProxy received invalid control message");
                }
            }
            if (pollItems[0].revents & ZMQ_POLLIN) {
                SwapEnvelopesAndTransfer(routerSocket, routerSocket);
            }
        }
    } catch (...) {
        assert(!"Exception thrown in background P2P proxy thread");
    }
}


namespace
{
    class ProxyFunctor
    {
    public:
        ProxyFunctor(
            zmq::socket_t&& routerSocket,
            zmq::socket_t&& controlSocket,
            boost::chrono::milliseconds timeout)
            : m_routerSocket(std::move(routerSocket)),
              m_controlSocket(std::move(controlSocket)),
              m_timeout(timeout)
        {
        }

        ProxyFunctor(ProxyFunctor&& other) DSB_NOEXCEPT
            : m_routerSocket(std::move(other.m_routerSocket)),
              m_controlSocket(std::move(other.m_controlSocket)),
              m_timeout(other.m_timeout)
        {
        }

        void operator()()
        {
            dsb::comm::P2PProxy(m_routerSocket, &m_controlSocket, m_timeout);
        }

    private:
        zmq::socket_t m_routerSocket;
        zmq::socket_t m_controlSocket;
        boost::chrono::milliseconds m_timeout;
    };
}


BackgroundP2PProxy::BackgroundP2PProxy(
    zmq::socket_t&& routerSocket,
    boost::chrono::milliseconds timeout)
    : m_controlSocket(GlobalContext(), ZMQ_PAIR)
{
    EnforceRouter(routerSocket);
    DSB_INPUT_CHECK(timeout == NEVER_TIMEOUT
        || (timeout.count() > 0
            && timeout.count() < std::numeric_limits<long>::max()));

    auto controlSocketRemote = zmq::socket_t(GlobalContext(), ZMQ_PAIR);
    const int linger = 0;
    m_controlSocket.setsockopt(ZMQ_LINGER, &linger, sizeof(linger));
    controlSocketRemote.setsockopt(ZMQ_LINGER, &linger, sizeof(linger));

    const auto controlEndpoint = "inproc://" + dsb::util::RandomUUID();
    m_controlSocket.bind(controlEndpoint.c_str());
    controlSocketRemote.connect(controlEndpoint.c_str());
    m_thread = boost::thread(ProxyFunctor(
        std::move(routerSocket),
        std::move(controlSocketRemote),
        timeout));
}


BackgroundP2PProxy::~BackgroundP2PProxy() DSB_NOEXCEPT
{
    try {
        Terminate();
    } catch (...) {
        assert(!"Exception thrown during BackgroundP2PProxy destruction");
    }
}


BackgroundP2PProxy::BackgroundP2PProxy(BackgroundP2PProxy&& other) DSB_NOEXCEPT
    : m_controlSocket(std::move(other.m_controlSocket)),
      m_thread(std::move(other.m_thread))
{
}


BackgroundP2PProxy& BackgroundP2PProxy::operator=(BackgroundP2PProxy&& other)
    DSB_NOEXCEPT
{
    try {
        Terminate();
    } catch (...) {
        assert(!"Exception thrown during BackgroundP2PProxy move assignment");
    }
    m_controlSocket = std::move(other.m_controlSocket);
    m_thread        = std::move(other.m_thread);
    return *this;
}


void BackgroundP2PProxy::Terminate()
{
    if (m_thread.joinable()) {
        m_controlSocket.send("TERMINATE", 9);
        m_thread.join();
        m_controlSocket.close();
    }
}


void BackgroundP2PProxy::Detach()
{
    if (m_thread.joinable()) {
        m_thread.detach();
        m_controlSocket.close();
    }
}


boost::thread& BackgroundP2PProxy::Thread__()
{
    return m_thread;
}


BackgroundP2PProxy SpawnTcpP2PProxy(
    const std::string& networkInterface,
    boost::chrono::milliseconds timeout,
    std::uint16_t& ephemeralPort)
{
    auto routerSocket = zmq::socket_t(GlobalContext(), ZMQ_ROUTER);
    const auto ephemeralPort_ = BindToEphemeralPort(routerSocket, networkInterface);
    auto proxy = BackgroundP2PProxy(std::move(routerSocket), timeout);
    // ---- Critical line; no exceptions thrown below ----
    ephemeralPort = ephemeralPort_;
    return std::move(proxy);
}


BackgroundP2PProxy SpawnTcpP2PProxy(
    const std::string& networkInterface,
    std::uint16_t& ephemeralPort)
{
    return SpawnTcpP2PProxy(networkInterface, NEVER_TIMEOUT, ephemeralPort);
}


}} // namespace
