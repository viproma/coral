#define NOMINMAX
#include "dsb/comm/p2p.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include "boost/lexical_cast.hpp"

#include "dsb/comm/messaging.hpp"
#include "dsb/comm/util.hpp"
#include "dsb/config.h"
#include "dsb/error.hpp"
#include "dsb/util.hpp"


namespace dsb
{
namespace comm
{


// =============================================================================
// P2PProxy
// =============================================================================

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

    void SwapEnvelopes(std::vector<zmq::message_t>& msg)
    {
        // TODO: This is currently somewhat limited in that it can only deal
        // with single-identity envelopes.
        if (msg.size() >= 4 && msg[1].size() == 0 && msg[3].size() == 0) {
            std::swap(msg[0], msg[2]);
        } else assert (!"Received a message with the wrong envelope format");
    }

    void SwapEnvelopesAndTransfer(zmq::socket_t& src, zmq::socket_t& tgt)
    {
        std::vector<zmq::message_t> msg;
        dsb::comm::Receive(src, msg);
        SwapEnvelopes(msg);
        dsb::comm::Send(tgt, msg);
    }
}


void P2PProxy(
    zmq::socket_t& routerSocket,
    zmq::socket_t* controlSocket,
    std::chrono::milliseconds timeout)
{
    EnforceRouter(routerSocket);
    DSB_INPUT_CHECK(timeout == NEVER_TIMEOUT
        || (timeout.count() > 0 && timeout.count() < std::numeric_limits<long>::max()));
    const auto timeoutL = static_cast<long>(timeout.count());
    zmq::pollitem_t pollItems[2] = {
        { static_cast<void*>(routerSocket),   0, ZMQ_POLLIN, 0 },
        { static_cast<void*>(*controlSocket), 0, ZMQ_POLLIN, 0 }
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
            std::chrono::milliseconds timeout)
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
        std::chrono::milliseconds m_timeout;
    };
}


BackgroundP2PProxy::BackgroundP2PProxy(
    zmq::socket_t&& routerSocket,
    std::chrono::milliseconds timeout)
    : m_controlSocket(GlobalContext(), ZMQ_PAIR)
{
    Init(std::move(routerSocket), timeout);
}


// TODO: Write unittest for this.
BackgroundP2PProxy::BackgroundP2PProxy(
    const std::string& endpoint,
    std::chrono::milliseconds timeout)
    : m_controlSocket(GlobalContext(), ZMQ_PAIR)
{
    auto routerSocket = zmq::socket_t(GlobalContext(), ZMQ_ROUTER);
    routerSocket.bind(endpoint.c_str());
    Init(std::move(routerSocket), timeout);
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


std::thread& BackgroundP2PProxy::Thread__()
{
    return m_thread;
}


void BackgroundP2PProxy::Init(
    zmq::socket_t&& routerSocket,
    std::chrono::milliseconds timeout)
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
    m_thread = std::thread(ProxyFunctor(
        std::move(routerSocket),
        std::move(controlSocketRemote),
        timeout));
}


BackgroundP2PProxy SpawnTcpP2PProxy(
    const std::string& networkInterface,
    std::chrono::milliseconds timeout,
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


// =============================================================================
// P2PEndpoint
// =============================================================================

P2PEndpoint::P2PEndpoint()
    : m_endpoint(),
      m_identity()
{
}


P2PEndpoint::P2PEndpoint(const std::string& url)
    : m_endpoint(url.substr(0, url.find_last_of('$'))),
      m_identity(url.size() > m_endpoint.size() ? url.substr(m_endpoint.size() + 1) : std::string())
{
    DSB_INPUT_CHECK(!url.empty());
}


P2PEndpoint::P2PEndpoint(
    const std::string& endpoint,
    const std::string& identity)
    : m_endpoint(endpoint),
      m_identity(identity)
{
    DSB_INPUT_CHECK(!endpoint.empty());
}


const std::string& P2PEndpoint::Endpoint() const { return m_endpoint; }


bool P2PEndpoint::IsBehindProxy() const { return !m_identity.empty(); }


const std::string& P2PEndpoint::Identity() const { return m_identity; }


std::string P2PEndpoint::URL() const
{
    auto retval = Endpoint();
    if (IsBehindProxy()) {
        retval += '$';
        retval += Identity();
    }
    return retval;
}



// =============================================================================
// P2PReqSocket
// =============================================================================

namespace
{
    static const int P2PREQ_DEFAULT_LINGER_MSEC = 0;

    bool HasMoreFrames(zmq::socket_t& socket)
    {
        int val = -1;
        size_t len = sizeof(val);
        socket.getsockopt(ZMQ_RCVMORE, &val, &len);
        return !!val;
    }

    void DiscardMessage(zmq::socket_t& socket)
    {
        while (HasMoreFrames(socket)) {
            socket.recv(nullptr, 0);
        }
    }

    void ConsumeDelimiterFrame(zmq::socket_t& s)
    {
        zmq::message_t df;
        s.recv(&df);
        if (!df.more()) {
            throw std::runtime_error("Invalid incoming message (not enough frames)");
        } else if (df.size() > 0) {
            DiscardMessage(s);
            throw std::runtime_error("Invalid incoming message (bad header)");
        }
    }
}


P2PReqSocket::P2PReqSocket()
    : m_connectedState(DISCONNECTED),
      m_awaitingRep(false),
      m_socket(),
      m_serverIdentity()
{
}


#if !DSB_HAS_EXPLICIT_DEFAULTED_DELETED_FUNCS
    P2PReqSocket::P2PReqSocket(P2PReqSocket&& other) DSB_NOEXCEPT
        : m_connectedState(other.m_connectedState),
          m_awaitingRep(other.m_awaitingRep),
          m_socket(std::move(other.m_socket)),
          m_serverIdentity(std::move(other.m_serverIdentity))
    { }
    
    P2PReqSocket& P2PReqSocket::operator=(P2PReqSocket&& other) DSB_NOEXCEPT
    {
        m_connectedState = dsb::util::MoveAndReplace(other.m_connectedState, DISCONNECTED);
        m_awaitingRep = dsb::util::MoveAndReplace(other.m_awaitingRep, false);
        m_socket = std::move(other.m_socket);
        m_serverIdentity = std::move(other.m_serverIdentity);
        return *this;
    }
#endif


void P2PReqSocket::Connect(const P2PEndpoint& server)
{
    if (m_connectedState != DISCONNECTED) {
        throw std::logic_error("Socket already bound/connected");
    }
    assert(!m_awaitingRep);
    assert(!m_socket);
    assert(m_serverIdentity.size() == 0);

    auto connectedState = DISCONNECTED;
    auto socket = std::make_unique<zmq::socket_t>(GlobalContext(), ZMQ_DEALER);
    zmq::message_t serverIdentity;
    if (server.IsBehindProxy()) {
        socket->connect(server.Endpoint().c_str());
        serverIdentity = zmq::message_t(server.Identity().size());
        std::memcpy(
            serverIdentity.data(),
            server.Identity().data(),
            server.Identity().size());
        connectedState = PROXY_CONNECTED;
    } else {
        socket->connect(server.Endpoint().c_str());
        connectedState = CONNECTED;
    }
    socket->setsockopt(
        ZMQ_LINGER,
        &P2PREQ_DEFAULT_LINGER_MSEC,
        sizeof(P2PREQ_DEFAULT_LINGER_MSEC));
    // ---- No exceptions below this line ----
    m_connectedState = connectedState;
    m_socket = std::move(socket);
    m_serverIdentity = std::move(serverIdentity);
}


void P2PReqSocket::Bind(const std::string& localEndpoint)
{
    if (m_connectedState != DISCONNECTED) {
        throw std::logic_error("Socket already bound/connected");
    }
    assert(!m_awaitingRep);
    assert(!m_socket);
    assert(m_serverIdentity.size() == 0);

    auto socket = std::make_unique<zmq::socket_t>(GlobalContext(), ZMQ_DEALER);
    socket->bind(localEndpoint.c_str());
    socket->setsockopt(
        ZMQ_LINGER,
        &P2PREQ_DEFAULT_LINGER_MSEC,
        sizeof(P2PREQ_DEFAULT_LINGER_MSEC));
    // ---- No exceptions below this line ----
    m_connectedState = BOUND;
    m_socket = std::move(socket);
}


void P2PReqSocket::Close()
{
    if (m_connectedState != DISCONNECTED) {
        m_connectedState = DISCONNECTED;
        m_awaitingRep = false;
        m_socket.reset();
        m_serverIdentity.rebuild();
    }
}


void P2PReqSocket::Send(std::vector<zmq::message_t>& msg, int flags)
{
    if (m_connectedState == DISCONNECTED) {
        throw std::logic_error("Socket not bound/connected");
    }
    if (m_awaitingRep && !(flags & SEND_OUT_OF_ORDER)) {
        throw std::logic_error("Send() called out of order");
    }
    if (msg.empty()) {
        throw std::invalid_argument("Message is empty");
    }
    m_socket->send("", 0, ZMQ_SNDMORE);
    if (m_connectedState == PROXY_CONNECTED) {
        zmq::message_t serverIdentity;
        serverIdentity.copy(&m_serverIdentity);
        m_socket->send(serverIdentity, ZMQ_SNDMORE);
        m_socket->send("", 0, ZMQ_SNDMORE);
    }
    const auto nextToLast = --msg.end();
    for (auto it = msg.begin(); it != nextToLast; ++it) {
        m_socket->send(*it, ZMQ_SNDMORE);
    }
    m_socket->send(msg.back());
    msg.clear();
    m_awaitingRep = true;
}


void P2PReqSocket::Receive(std::vector<zmq::message_t>& msg)
{
    if (m_connectedState == DISCONNECTED) {
        throw std::logic_error("Socket not bound/connected");
    }
    if (!m_awaitingRep) {
        throw std::logic_error("Receive() called out of order");
    }
    ConsumeDelimiterFrame(*m_socket);
    if (m_connectedState == PROXY_CONNECTED) {
        zmq::message_t serverIdentity;
        m_socket->recv(&serverIdentity);
        if (!serverIdentity.more()) {
            throw std::runtime_error("Invalid incoming message (not enough frames)");
        }
        ConsumeDelimiterFrame(*m_socket);
        if (serverIdentity.size() != m_serverIdentity.size()
            || std::memcmp(serverIdentity.data(), m_serverIdentity.data(), serverIdentity.size()))
        {
            throw std::runtime_error("Invalid incoming message (wrong server identity)");
        }
    }
    msg.clear();
    do {
        msg.push_back(zmq::message_t());
        m_socket->recv(&msg.back());
    } while (msg.back().more());
    m_awaitingRep = false;
}


zmq::socket_t& P2PReqSocket::Socket()
{
    return *m_socket;
}


const zmq::socket_t& P2PReqSocket::Socket() const
{
    return *m_socket;
}


// =============================================================================
// P2PRepSocket
// =============================================================================

namespace
{
    static const int P2PREP_DEFAULT_LINGER_MSEC = P2PREQ_DEFAULT_LINGER_MSEC;
}


P2PRepSocket::P2PRepSocket()
    : m_connectedState(DISCONNECTED),
      m_processingReq(false),
      m_socket(),
      m_boundEndpoint(),
      m_clientIdentity()
{
}


#if !DSB_HAS_EXPLICIT_DEFAULTED_DELETED_FUNCS
    P2PRepSocket::P2PRepSocket(P2PRepSocket&& other) DSB_NOEXCEPT
        : m_connectedState(other.m_connectedState),
          m_processingReq(other.m_processingReq),
          m_socket(std::move(other.m_socket)),
          m_boundEndpoint(dsb::util::MoveAndReplace(other.m_boundEndpoint)),
          m_clientIdentity(std::move(other.m_clientIdentity))
    { }
    
    P2PRepSocket& P2PRepSocket::operator=(P2PRepSocket&& other) DSB_NOEXCEPT
    {
        m_connectedState = dsb::util::MoveAndReplace(other.m_connectedState, DISCONNECTED);
        m_processingReq = dsb::util::MoveAndReplace(other.m_processingReq, false);
        m_socket = std::move(other.m_socket);
        m_boundEndpoint = dsb::util::MoveAndReplace(other.m_boundEndpoint);
        m_clientIdentity = std::move(other.m_clientIdentity);
        return *this;
    }
#endif


void P2PRepSocket::Bind(const P2PEndpoint& bindpoint)
{
    EnforceDisconnected();

    auto connectedState = DISCONNECTED;
    std::unique_ptr<zmq::socket_t> socket;
    P2PEndpoint boundEndpoint;
    if (bindpoint.IsBehindProxy()) {
        socket = std::make_unique<zmq::socket_t>(GlobalContext(), ZMQ_DEALER);
        socket->setsockopt(
            ZMQ_IDENTITY,
            bindpoint.Identity().data(),
            bindpoint.Identity().size());
        socket->connect(bindpoint.Endpoint().c_str());
        boundEndpoint = bindpoint;
        connectedState = PROXY_BOUND;
    } else {
        socket = std::make_unique<zmq::socket_t>(GlobalContext(), ZMQ_REP);
        socket->bind(bindpoint.Endpoint().c_str());
        boundEndpoint = P2PEndpoint(LastEndpoint(*socket));
        connectedState = BOUND;
    }
    socket->setsockopt(
        ZMQ_LINGER,
        &P2PREP_DEFAULT_LINGER_MSEC,
        sizeof(P2PREP_DEFAULT_LINGER_MSEC));
    // ---- No exceptions below this line ----
    m_connectedState = connectedState;
    m_socket = std::move(socket);
    m_boundEndpoint = std::move(boundEndpoint);
}


void P2PRepSocket::Connect(const std::string& clientEndpoint)
{
    EnforceDisconnected();

    auto socket = std::make_unique<zmq::socket_t>(GlobalContext(), ZMQ_REP);
    socket->setsockopt(
        ZMQ_LINGER,
        &P2PREP_DEFAULT_LINGER_MSEC,
        sizeof(P2PREP_DEFAULT_LINGER_MSEC));
    socket->connect(clientEndpoint.c_str());
    // ---- No exceptions below this line ----
    m_connectedState = CONNECTED;
    m_socket = std::move(socket);
}


void P2PRepSocket::Close()
{
    if (m_connectedState != DISCONNECTED) {
        m_connectedState = DISCONNECTED;
        m_processingReq = false;
        m_socket.reset();
        m_boundEndpoint = P2PEndpoint();
        m_clientIdentity = zmq::message_t();
    }
}


const P2PEndpoint& P2PRepSocket::BoundEndpoint() const
{
    return m_boundEndpoint;
}


void P2PRepSocket::Receive(std::vector<zmq::message_t>& msg)
{
    EnforceConnected();
    if (m_processingReq) {
        throw std::logic_error("Receive() called out of order");
    }
    if (m_connectedState == PROXY_BOUND) {
        ConsumeDelimiterFrame(*m_socket);
        zmq::message_t clientIdentity;
        m_socket->recv(&clientIdentity);
        if (!clientIdentity.more()) {
            throw std::runtime_error("Invalid incoming message (not enough frames)");
        }
        ConsumeDelimiterFrame(*m_socket);
        m_clientIdentity = std::move(clientIdentity);
    }
    msg.clear();
    do {
        msg.push_back(zmq::message_t());
        m_socket->recv(&msg.back());
    } while (msg.back().more());
    m_processingReq = true;
}


void P2PRepSocket::Send(std::vector<zmq::message_t>& msg)
{
    EnforceConnected();
    if (!m_processingReq) {
        throw std::logic_error("Send() called out of order");
    }
    if (msg.empty()) {
        throw std::invalid_argument("Message is empty");
    }
    if (m_connectedState == PROXY_BOUND) {
        m_socket->send("", 0, ZMQ_SNDMORE);
        m_socket->send(m_clientIdentity, ZMQ_SNDMORE);
        m_socket->send("", 0, ZMQ_SNDMORE);
    }
    const auto nextToLast = --msg.end();
    for (auto it = msg.begin(); it != nextToLast; ++it) {
        m_socket->send(*it, ZMQ_SNDMORE);
    }
    m_socket->send(msg.back());
    msg.clear();
    m_processingReq = false;
}


void P2PRepSocket::EnforceConnected() const
{
    if (m_connectedState == DISCONNECTED) {
        throw std::logic_error("Socket not bound/connected");
    }
    assert(m_socket);
}


void P2PRepSocket::EnforceDisconnected() const
{
    if (m_connectedState != DISCONNECTED) {
        throw std::logic_error("Socket already bound/connected");
    }
    assert(!m_processingReq);
    assert(!m_socket);
    assert(m_clientIdentity.size() == 0);
}


zmq::socket_t& P2PRepSocket::Socket()
{
    return *m_socket;
}


const zmq::socket_t& P2PRepSocket::Socket() const
{
    return *m_socket;
}


bool Receive(
    P2PRepSocket& socket,
    std::vector<zmq::message_t>& message,
    std::chrono::milliseconds timeout)
{
    zmq::pollitem_t pollItem = { static_cast<void*>(socket.Socket()), 0, ZMQ_POLLIN, 0 };
    if (zmq::poll(&pollItem, 1, static_cast<long>(timeout.count())) == 0) {
        return false;
    } else {
        assert (pollItem.revents == ZMQ_POLLIN);
        socket.Receive(message);
        return true;
    }
}

}} // namespace
