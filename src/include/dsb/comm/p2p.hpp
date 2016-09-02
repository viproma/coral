/**
\file
\brief  Functions and classes for point-to-point REQ-REP communication,
        optionally via a broker.
*/
#ifndef DSB_COMM_P2P_HPP
#define DSB_COMM_P2P_HPP

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "zmq.hpp"

#include "dsb/config.h"


namespace dsb
{
namespace comm
{


/**
\brief  An endpoint description which may refer to an endpoint which is
        accessed via a P2P proxy.
*/
class P2PEndpoint
{
public:
    P2PEndpoint();

    P2PEndpoint(const std::string& url);

    P2PEndpoint(const std::string& endpoint, const std::string& identity);

    const std::string& Endpoint() const;

    bool IsBehindProxy() const;

    const std::string& Identity() const;

    std::string URL() const;

private:
    std::string m_endpoint;
    std::string m_identity;
};


/**
\brief  A client socket for communication with a single server node in a
        request-reply pattern, either directly or via a P2P proxy.

The purpose of this class is to communicate with one other node, the "server",
in a request-reply pattern.  The server may be connected directly, socket to
socket, or it may be connected via a P2P proxy.  After connection, the presence
of the proxy (or lack thereof) does not affect client code.

\see P2PRepSocket, which is the corresponding server socket.
\see SpawnP2PProxy, which sets up a P2P proxy.
*/
class P2PReqSocket
{
public:
    /// Constructs a new, unconnected socket.
    P2PReqSocket();

    DSB_DEFINE_DEFAULT_MOVE(P2PReqSocket,
        m_connectedState, m_socket, m_serverIdentity)

    ~P2PReqSocket() DSB_NOEXCEPT;

    /**
    \brief  Connects to a server.

    This function may only be called if the socket is not already connected or
    bound.
    */
    void Connect(const P2PEndpoint& server);

    /**
    \brief  Binds to an endpoint to accept an incoming direct connection from
            a server.

    This function may only be called if the socket is not already connected or
    bound.
    */
    void Bind(const std::string& localEndpoint);

    /**
    \brief  Disconnects and/or unbinds the socket.

    If the socket is not connected or bound, this function has no effect.
    */
    void Close();

    /**
    \brief  Sends a request.

    This function may only be called if the socket is connected or bound.
    */
    void Send(std::vector<zmq::message_t>& msg);

    /**
    \brief  Receives a reply.

    This function may only be called if the socket is connected or bound.
    */
    void Receive(std::vector<zmq::message_t>& msg);

    /**
    \brief  The underlying ZMQ socket.

    This reference is only valid after the socket has been connected/bound.
    The socket is of type REQ.
    */
    zmq::socket_t& Socket();
    const zmq::socket_t& Socket() const;

private:
    enum State
    {
        DISCONNECTED,
        CONNECTED,
        PROXY_CONNECTED,
        BOUND,
    };
    State m_connectedState;
    std::unique_ptr<zmq::socket_t> m_socket;
    zmq::message_t m_serverIdentity;
};


/**
\brief  A server socket for communication with client nodes in a
        request-reply pattern, either directly or via a P2P proxy.

The purpose of this class is to communicate with one or more other nodes, the
"clients", in a request-reply pattern.  The clients may be connected directly,
socket to socket, or they may be connected via a P2P proxy.  (These two methods
cannot be used at once by different clients.)  After connection, the presence
of the proxy (or lack thereof) does not affect code that uses this class.

\see P2PReqSocket, which is the corresponding client socket.
\see SpawnP2PProxy, which sets up a P2P proxy.
*/
class P2PRepSocket
{
public:
    /// Constructs a new, unconnected socket.
    P2PRepSocket();

    DSB_DEFINE_DEFAULT_MOVE(P2PRepSocket,
        m_connectedState, m_socket, m_boundEndpoint, m_clientEnvelope)

    ~P2PRepSocket() DSB_NOEXCEPT;

    /**
    \brief  Binds to a local endpoint or connects to a proxy and waits for
            incoming requests from clients.

    This function may only be called if the socket is not already connected or
    bound.
    */
    void Bind(const P2PEndpoint& bindpoint);

    /**
    \brief  Connects to a single client and waits for incoming requests from it.

    This function may only be called if the socket is not already connected or
    bound.
    */
    void Connect(const std::string& clientEndpoint);

    /**
    \brief  Disconnects and/or unbinds the socket.

    If the socket is not connected or bound, this function has no effect.
    */
    void Close();

    /**
    \brief  Returns the endpoint this socket has been bound to, or the endpoint
            of the proxy it is connected to along with the socket identity.
    */
    const P2PEndpoint& BoundEndpoint() const;

    /**
    \brief  Receives a request.

    The sender's identity will be stored and used when a reply is sent with
    Send().

    This function may only be called if the socket is connected or bound, and
    may not be called again before a reply has been sent with Send() or the
    request has been ignored with Ignore().
    */
    void Receive(std::vector<zmq::message_t>& msg);

    /**
    \brief  Sends a reply.

    This function may only be called if the socket is connected or bound, and
    then only after a request has been received with Receive() and it has not
    been ignored with Ignore().
    */
    void Send(std::vector<zmq::message_t>& msg);

    /**
    \brief  Ignores the last received request.

    This enables calling Receive() to receive a new request without first
    sending a reply to the last one.  After calling this function, it is an
    error to attempt to call Send() before a new request has been received.

    If the socket is not connected, or no request has been received, this
    function has no effect.
    */
    void Ignore();

    /**
    \brief  The underlying ZMQ socket.

    This reference is only valid after the socket has been connected/bound.
    If ProxyBind() has been called, the socket is of type DEALER, otherwise it
    has type REP.
    */
    zmq::socket_t& Socket();
    const zmq::socket_t& Socket() const;

private:
    void EnforceConnected() const;
    void EnforceDisconnected() const;
    enum State
    {
        DISCONNECTED,
        BOUND,
        PROXY_BOUND,
        CONNECTED
    };
    State m_connectedState;
    std::unique_ptr<zmq::socket_t> m_socket;
    P2PEndpoint m_boundEndpoint;
    std::vector<zmq::message_t> m_clientEnvelope;
};


/**
\brief Receives a message, given that one arrives before the timeout is reached.

Existing message content will be overwritten.

\returns `true` if a message was received, or `false` if the function timed out.
\throws zmq::error_t on failure to receive a message frame.
*/
bool Receive(
    P2PRepSocket& socket,
    std::vector<zmq::message_t>& message,
    std::chrono::milliseconds timeout);


}} // namespace
#endif // header guard
