/**
\file
\brief  Module header for coral::net::zmqx
\copyright
    Copyright 2013-2017, SINTEF Ocean and the Coral contributors.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifndef CORAL_NET_ZMQX_HPP
#define CORAL_NET_ZMQX_HPP

#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <zmq.hpp>

#include <coral/config.h>
#include <coral/net.hpp>


namespace coral
{
namespace net
{

/**
\brief  Functions and classes that extend or wrap the ZeroMQ API.

The namespace name `zmqx` stands for "ZeroMQ eXtensions", and was mainly chosen
because `zmq` caused all kinds of annoying name conflicts with the standard
ZeroMQ C++ API.
*/
namespace zmqx
{


/**
\brief  Returns a reference to a global ZMQ context.

This function is thread safe as long as it is not used in a static
initialisation setting.
*/
zmq::context_t& GlobalContext();


/**
\brief  Binds `socket` to an ephemeral TCP port on the given network interface
        and returns the port number.
*/
std::uint16_t BindToEphemeralPort(
    zmq::socket_t& socket,
    const std::string& networkInterface = "*");


/**
\brief Returns the value of the ZMQ_LAST_ENDPOINT socket property.
\throws zmq::error_t if ZMQ reports an error.
*/
std::string LastEndpoint(zmq::socket_t& socket);


/**
\brief  Given a string on the form "tcp://addr:port", returns the port number.

\throws std::invalid_argument if `endpoint` does not have the structure
    described above.
\throws std::bad_cast if the port number is not in a valid number format.
*/
std::uint16_t EndpointPort(const std::string& endpoint);


/**
\brief  Waits up to `timeout` milliseconds to see if a message may be enqueued
        on `socket`.

If `timeout` is negative, the function will wait indefinitely.

\returns whether a message may be immediately enqueued on `socket`.
\throws zmq::error_t on communications error.
*/
bool WaitForOutgoing(zmq::socket_t& socket, std::chrono::milliseconds timeout);


/**
\brief  Waits up to `timeout` milliseconds for incoming messages on `socket`.

If `timeout` is negative, the function will wait indefinitely.

\returns whether there are incoming messages on `socket`.
\throws zmq::error_t on communications error.
*/
bool WaitForIncoming(zmq::socket_t& socket, std::chrono::milliseconds timeout);


/// Flags for the Send() function
enum class SendFlag : int
{
    /// No flags are set.
    none = 0,

    /**
    \brief  The frames being sent are part of a multiframe message, and more
            frames are coming.
    */
    more = 1
};
CORAL_DEFINE_BITWISE_ENUM_OPERATORS(SendFlag)


/**
\brief Sends a message.

The message content will be cleared on return.

\throws std::invalid_argument if `message` is empty.
\throws zmq::error_t on failure to send a message frame.
*/
void Send(
    zmq::socket_t& socket,
    std::vector<zmq::message_t>& message,
    SendFlag flags = SendFlag::none);


/**
\brief Receives a message.

Existing message content will be overwritten.

\throws zmq::error_t on failure to receive a message frame.
*/
void Receive(
    zmq::socket_t& socket,
    std::vector<zmq::message_t>& message);


/// Returns the content of a message frame as a `std::string`.
std::string ToString(const zmq::message_t& frame);


/// Returns a message frame whose contents are equal to `s`.
zmq::message_t ToFrame(const std::string& s);


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

    CORAL_DEFINE_DEFAULT_MOVE(ReqSocket, m_socket)

    ~ReqSocket() noexcept;

    /**
    \brief  Connects to a server.

    This function may only be called if the socket is not already connected or
    bound.
    */
    void Connect(const coral::net::Endpoint& server);

    /**
    \brief  Binds to an endpoint to accept an incoming direct connection from
            a server.

    This function may only be called if the socket is not already connected or
    bound.
    */
    void Bind(const coral::net::Endpoint& localEndpoint);

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

    CORAL_DEFINE_DEFAULT_MOVE(RepSocket,
        m_socket, m_boundEndpoint, m_clientEnvelope)

    ~RepSocket() noexcept;

    /**
    \brief  Binds to a local endpoint and waits for incoming requests from
            clients.

    This function may only be called if the socket is not already connected or
    bound.
    */
    void Bind(const coral::net::Endpoint& localEndpoint);

    /**
    \brief  Connects to a single client and waits for incoming requests from it.

    This function may only be called if the socket is not already connected or
    bound.
    */
    void Connect(const coral::net::Endpoint& clientEndpoint);

    /**
    \brief  Disconnects and/or unbinds the socket.

    If the socket is not connected or bound, this function has no effect.
    */
    void Close();

    /**
    \brief  Returns the endpoint this socket has been bound to, or the endpoint
            of the proxy it is connected to along with the socket identity.
    */
    const coral::net::Endpoint& BoundEndpoint() const;

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
    coral::net::Endpoint m_boundEndpoint;
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


}}} // namespace
#endif // header guard
