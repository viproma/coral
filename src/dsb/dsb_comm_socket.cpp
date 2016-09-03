#define NOMINMAX
#include "dsb/comm/socket.hpp"

#include <cassert>
#include <stdexcept>
#include <utility>

#include "dsb/config.h"
#include "dsb/comm/messaging.hpp"
#include "dsb/comm/util.hpp"
#include "dsb/error.hpp"


namespace dsb
{
namespace comm
{


// =============================================================================
// ReqSocket
// =============================================================================

namespace
{
    static const int REQSOCKET_DEFAULT_LINGER_MSEC = 0;

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


ReqSocket::ReqSocket()
{
}


ReqSocket::~ReqSocket() DSB_NOEXCEPT
{
    if (m_socket) {
        // ---
        // Workaround for ZeroMQ issue 1264, https://github.com/zeromq/libzmq/issues/1264
        char temp;
        m_socket->recv(&temp, 1, ZMQ_DONTWAIT);
        // ---
    }
}


void ReqSocket::Connect(const dsb::net::Endpoint& serverEndpoint)
{
    if (m_socket) {
        throw std::logic_error("Socket already bound/connected");
    }
    auto socket = std::make_unique<zmq::socket_t>(GlobalContext(), ZMQ_DEALER);
    socket->connect(serverEndpoint.URL());
    socket->setsockopt(ZMQ_LINGER, REQSOCKET_DEFAULT_LINGER_MSEC);
    // ---- No exceptions below this line ----
    m_socket = std::move(socket);
}


void ReqSocket::Bind(const dsb::net::Endpoint& localEndpoint)
{
    if (m_socket) {
        throw std::logic_error("Socket already bound/connected");
    }
    auto socket = std::make_unique<zmq::socket_t>(GlobalContext(), ZMQ_DEALER);
    socket->bind(localEndpoint.URL());
    socket->setsockopt(ZMQ_LINGER, REQSOCKET_DEFAULT_LINGER_MSEC);
    // ---- No exceptions below this line ----
    m_socket = std::move(socket);
}


void ReqSocket::Close()
{
    if (m_socket) {
        // ---
        // Workaround for ZeroMQ issue 1264, https://github.com/zeromq/libzmq/issues/1264
        char temp;
        m_socket->recv(&temp, 1, ZMQ_DONTWAIT);
        // ---
        m_socket.reset();
    }
}


void ReqSocket::Send(std::vector<zmq::message_t>& msg)
{
    if (!m_socket) {
        throw std::logic_error("Socket not bound/connected");
    }
    DSB_INPUT_CHECK(!msg.empty());
    m_socket->send("", 0, ZMQ_SNDMORE);
    dsb::comm::Send(*m_socket, msg);
}


void ReqSocket::Receive(std::vector<zmq::message_t>& msg)
{
    if (!m_socket) {
        throw std::logic_error("Socket not bound/connected");
    }
    ConsumeDelimiterFrame(*m_socket);
    dsb::comm::Receive(*m_socket, msg);
}


zmq::socket_t& ReqSocket::Socket()
{
    return *m_socket;
}


const zmq::socket_t& ReqSocket::Socket() const
{
    return *m_socket;
}


// =============================================================================
// RepSocket
// =============================================================================

namespace
{
    static const int REPSOCKET_DEFAULT_LINGER_MSEC = REQSOCKET_DEFAULT_LINGER_MSEC;
}


RepSocket::RepSocket()
{
}


RepSocket::~RepSocket() DSB_NOEXCEPT
{
    if (m_socket) {
        // ---
        // Workaround for ZeroMQ issue 1264, https://github.com/zeromq/libzmq/issues/1264
        char temp;
        m_socket->recv(&temp, 1, ZMQ_DONTWAIT);
        // ---
    }
}


void RepSocket::Bind(const dsb::net::Endpoint& localEndpoint)
{
    if (m_socket) {
        throw std::logic_error("Socket already bound/connected");
    }
    auto socket = std::make_unique<zmq::socket_t>(GlobalContext(), ZMQ_ROUTER);
    socket->bind(localEndpoint.URL());
    auto boundEndpoint = dsb::net::Endpoint{LastEndpoint(*socket)};
    socket->setsockopt(ZMQ_LINGER, REPSOCKET_DEFAULT_LINGER_MSEC);
    // ---- No exceptions below this line ----
    m_socket = std::move(socket);
    m_boundEndpoint = std::move(boundEndpoint);
}


void RepSocket::Connect(const dsb::net::Endpoint& clientEndpoint)
{
    if (m_socket) {
        throw std::logic_error("Socket already bound/connected");
    }
    auto socket = std::make_unique<zmq::socket_t>(GlobalContext(), ZMQ_ROUTER);
    socket->setsockopt(ZMQ_LINGER, REPSOCKET_DEFAULT_LINGER_MSEC);
    socket->connect(clientEndpoint.URL());
    // ---- No exceptions below this line ----
    m_socket = std::move(socket);
    m_boundEndpoint = dsb::net::Endpoint{};
}


void RepSocket::Close()
{
    if (m_socket) {
        // ---
        // Workaround for ZeroMQ issue 1264, https://github.com/zeromq/libzmq/issues/1264
        char temp;
        m_socket->recv(&temp, 1, ZMQ_DONTWAIT);
        // ---
        m_socket.reset();
        m_boundEndpoint = dsb::net::Endpoint{};
        m_clientEnvelope.clear();
    }
}


const dsb::net::Endpoint& RepSocket::BoundEndpoint() const
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


void RepSocket::Receive(std::vector<zmq::message_t>& msg)
{
    if (!m_socket) {
        throw std::logic_error("Socket not bound/connected");
    }
    std::vector<zmq::message_t> clientEnvelope;
    RecvEnvelope(*m_socket, clientEnvelope);
    dsb::comm::Receive(*m_socket, msg);
    m_clientEnvelope = std::move(clientEnvelope);
}


void RepSocket::Send(std::vector<zmq::message_t>& msg)
{
    if (!m_socket) {
        throw std::logic_error("Socket not bound/connected");
    }
    DSB_PRECONDITION_CHECK(!m_clientEnvelope.empty());
    DSB_INPUT_CHECK(!msg.empty());
    dsb::comm::Send(*m_socket, m_clientEnvelope, dsb::comm::SendFlag::more);
    dsb::comm::Send(*m_socket, msg);
    assert(m_clientEnvelope.empty());
}


zmq::socket_t& RepSocket::Socket()
{
    return *m_socket;
}


const zmq::socket_t& RepSocket::Socket() const
{
    return *m_socket;
}


bool Receive(
    RepSocket& socket,
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
