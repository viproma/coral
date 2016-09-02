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
    : m_connectedState{DISCONNECTED}
{
}


P2PReqSocket::~P2PReqSocket() DSB_NOEXCEPT
{
    if (m_socket) {
        // ---
        // Workaround for ZeroMQ issue 1264, https://github.com/zeromq/libzmq/issues/1264
        char temp;
        m_socket->recv(&temp, 1, ZMQ_DONTWAIT);
        // ---
    }
}


void P2PReqSocket::Connect(const P2PEndpoint& server)
{
    if (m_connectedState != DISCONNECTED) {
        throw std::logic_error("Socket already bound/connected");
    }
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
        // ---
        // Workaround for ZeroMQ issue 1264, https://github.com/zeromq/libzmq/issues/1264
        char temp;
        m_socket->recv(&temp, 1, ZMQ_DONTWAIT);
        // ---
        m_connectedState = DISCONNECTED;
        m_socket.reset();
        m_serverIdentity.rebuild();
    }
}


void P2PReqSocket::Send(std::vector<zmq::message_t>& msg)
{
    if (m_connectedState == DISCONNECTED) {
        throw std::logic_error("Socket not bound/connected");
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
    dsb::comm::Send(*m_socket, msg);
}


void P2PReqSocket::Receive(std::vector<zmq::message_t>& msg)
{
    if (m_connectedState == DISCONNECTED) {
        throw std::logic_error("Socket not bound/connected");
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
    dsb::comm::Receive(*m_socket, msg);
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
    : m_connectedState{DISCONNECTED}
{
}


P2PRepSocket::~P2PRepSocket() DSB_NOEXCEPT
{
    if (m_socket) {
        // ---
        // Workaround for ZeroMQ issue 1264, https://github.com/zeromq/libzmq/issues/1264
        char temp;
        m_socket->recv(&temp, 1, ZMQ_DONTWAIT);
        // ---
    }
}


void P2PRepSocket::Bind(const P2PEndpoint& bindpoint)
{
    EnforceDisconnected();

    auto connectedState = DISCONNECTED;
    auto socket = std::make_unique<zmq::socket_t>(GlobalContext(), ZMQ_ROUTER);
    P2PEndpoint boundEndpoint;
    if (bindpoint.IsBehindProxy()) {
        socket->setsockopt(
            ZMQ_IDENTITY,
            bindpoint.Identity().data(),
            bindpoint.Identity().size());
        socket->connect(bindpoint.Endpoint().c_str());
        boundEndpoint = bindpoint;
        connectedState = PROXY_BOUND;
    } else {
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

    auto socket = std::make_unique<zmq::socket_t>(GlobalContext(), ZMQ_ROUTER);
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
        // ---
        // Workaround for ZeroMQ issue 1264, https://github.com/zeromq/libzmq/issues/1264
        char temp;
        m_socket->recv(&temp, 1, ZMQ_DONTWAIT);
        // ---
        m_connectedState = DISCONNECTED;
        m_socket.reset();
        m_boundEndpoint = P2PEndpoint();
        m_clientEnvelope.clear();
    }
}


const P2PEndpoint& P2PRepSocket::BoundEndpoint() const
{
    return m_boundEndpoint;
}


namespace
{
    // Receives frames up to and including the next empty delimiter frame
    // and appends them to msg. If no frames follow the delimiter, an exception
    // is thrown.
    void RecvEnvelope(zmq::socket_t& socket, std::vector<zmq::message_t>& msg)
    {
        do {
            msg.emplace_back();
            socket.recv(&msg.back());
        } while (msg.back().size() > 0 && msg.back().more());
        if (!msg.back().more()) {
            throw std::runtime_error("Invalid incoming message (not enough frames)");
        }
    }
}


void P2PRepSocket::Receive(std::vector<zmq::message_t>& msg)
{
    EnforceConnected();
    DSB_PRECONDITION_CHECK(m_clientEnvelope.empty());

    std::vector<zmq::message_t> clientEnvelope;
    RecvEnvelope(*m_socket, clientEnvelope);
    if (m_connectedState == PROXY_BOUND) {
        // Also receive the "P2P envelope"
        RecvEnvelope(*m_socket, clientEnvelope);
    }
    dsb::comm::Receive(*m_socket, msg);
    m_clientEnvelope = std::move(clientEnvelope);
}


void P2PRepSocket::Send(std::vector<zmq::message_t>& msg)
{
    EnforceConnected();
    DSB_PRECONDITION_CHECK(!m_clientEnvelope.empty());
    DSB_INPUT_CHECK(!msg.empty());
    dsb::comm::Send(*m_socket, m_clientEnvelope, dsb::comm::SendFlag::more);
    dsb::comm::Send(*m_socket, msg);
    assert(m_clientEnvelope.empty());
}


void P2PRepSocket::Ignore()
{
    m_clientEnvelope.clear();
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
    assert(!m_socket);
    assert(m_clientEnvelope.empty());
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
