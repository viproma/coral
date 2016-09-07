/**
\file
\brief  Socket types and fundamental communication patterns built on top
        of ZeroMQ sockets.
*/
#ifndef DSB_NET_SOCKET_HPP
#define DSB_NET_SOCKET_HPP

#include <chrono>
#include <memory>
#include <vector>

#include "zmq.hpp"

#include "dsb/config.h"
#include "dsb/net.hpp"


namespace dsb
{
namespace net
{


/**
\brief  A client socket for communication with a single server node.

This is similar to a ZeroMQ REQ socket, except that it is not limited to a
strict alternating send/receive sequence.
*/
class ReqSocket
{
public:
    /// Constructs a new, unconnected socket.
    ReqSocket();

    DSB_DEFINE_DEFAULT_MOVE(ReqSocket, m_socket)

    ~ReqSocket() DSB_NOEXCEPT;

    /**
    \brief  Connects to a server.

    This function may only be called if the socket is not already connected or
    bound.
    */
    void Connect(const dsb::net::Endpoint& server);

    /**
    \brief  Binds to an endpoint to accept an incoming direct connection from
            a server.

    This function may only be called if the socket is not already connected or
    bound.
    */
    void Bind(const dsb::net::Endpoint& localEndpoint);

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
    The socket is of type DEALER.
    */
    zmq::socket_t& Socket();
    const zmq::socket_t& Socket() const;

private:
    std::unique_ptr<zmq::socket_t> m_socket;
};


/**
\brief  A server socket for communication with one or more client nodes in a
        request-reply pattern.

This is similar to a ZeroMQ REP socket, except that it is not limited to a
strict alternating receive/send sequence:  The Receive() function may be
called again without an intervening Send(), which will cause the previous
request to be ignored.
*/
class RepSocket
{
public:
    /// Constructs a new, unconnected socket.
    RepSocket();

    DSB_DEFINE_DEFAULT_MOVE(RepSocket,
        m_socket, m_boundEndpoint, m_clientEnvelope)

    ~RepSocket() DSB_NOEXCEPT;

    /**
    \brief  Binds to a local endpoint and waits for incoming requests from
            clients.

    This function may only be called if the socket is not already connected or
    bound.
    */
    void Bind(const dsb::net::Endpoint& localEndpoint);

    /**
    \brief  Connects to a single client and waits for incoming requests from it.

    This function may only be called if the socket is not already connected or
    bound.
    */
    void Connect(const dsb::net::Endpoint& clientEndpoint);

    /**
    \brief  Disconnects and/or unbinds the socket.

    If the socket is not connected or bound, this function has no effect.
    */
    void Close();

    /**
    \brief  Returns the endpoint this socket has been bound to, or the endpoint
            of the proxy it is connected to along with the socket identity.
    */
    const dsb::net::Endpoint& BoundEndpoint() const;

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
    The socket is of type ROUTER.
    */
    zmq::socket_t& Socket();
    const zmq::socket_t& Socket() const;

private:
    std::unique_ptr<zmq::socket_t> m_socket;
    dsb::net::Endpoint m_boundEndpoint;
    std::vector<zmq::message_t> m_clientEnvelope;
};


/**
\brief Receives a message, given that one arrives before the timeout is reached.

Existing message content will be overwritten.

\returns `true` if a message was received, or `false` if the function timed out.
\throws zmq::error_t on failure to receive a message frame.
*/
bool Receive(
    RepSocket& socket,
    std::vector<zmq::message_t>& message,
    std::chrono::milliseconds timeout);


}} // namespace
#endif // header guard
