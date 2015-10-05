/**
\file
\brief  Functions and classes for point-to-point REQ-REP communication,
        optionally via a broker.
*/
#ifndef DSB_COMM_P2P_HPP
#define DSB_COMM_P2P_HPP

#include <cstdint>
#include <deque>
#include <memory>
#include <string>

#include "boost/chrono/duration.hpp"
#include "boost/thread.hpp"
#include "zmq.hpp"

#include "dsb/config.h"
#include "dsb/comm/proxy.hpp" // only for NEVER_TIMEOUT; we should probably move this


namespace dsb
{
namespace comm
{


/**
\brief  A function which runs a proxy for point-to-point communication.

This function is similar to ZMQ's proxy functions, except that it is
designed to route requests from clients to *particular, named servers* (and
of course to route the replies back to the correct clients again.

It must be given a socket of ROUTER type, to which both clients and servers
connect, as well as an optional control socket to which a message may be sent
to terminate the proxy.  The function blocks until the proxy is terminated.

Messages sent to this proxy should have the following format:
~~~
first frame     : empty
second frame    : recipient identity
third frame     : empty
remaining frames: message content
~~~
Note that if the client uses a REQ socket, the socket will automatically add
the first, empty, frame.  In that case, only the remaining frames need to be
sent explicitly.

If a peer with the given identity is connected to the proxy using a DEALER
socket, it will receive that message with the following format:
~~~
first frame     : empty
second frame    : sender identity
third frame     : empty
remaining frames: message content
~~~
If the recipient identity does not correspond to a connected peer, the message
will be dropped.

To terminate the proxy, send a message to the control socket, whose sole
contents must be the string "TERMINATE".  It is not allowed to send any other
message to the control socket, and doing so will result in unspecified
behaviour.

It is also possible to specify a timeout, so that if the proxy does not
receive any messages for a certain period of time, it will shut itself down.

\param [in] routerSocket
    A socket of ROUTER type, which will be used for routing messages between
    clients and servers.
\param [in] controlSocket
    A socket to which a message may be sent to terminate the proxy.
\param [in] timeout
    The timeout period, or #NEVER_TIMEOUT if the proxy should run indefinitely.

\throws std::invalid_argument if `routerSocket` is not of type ROUTER,
        or if `timeout` is nonpositive or greater than the maximum timeout
        supported by ZMQ's poll function.
\throws zmq::error_t if ZMQ reports an error.
*/
void P2PProxy(
    zmq::socket_t& routerSocket,
    zmq::socket_t* controlSocket = nullptr,
    boost::chrono::milliseconds timeout = NEVER_TIMEOUT);


/**
\brief  A class which starts and controls a P2P proxy in a background thread.

When an object of this class is created, it will spawn a background thread and
run dsb::comm::P2PProxy() in that thread.  The lifetime of the proxy and the
thread in which it runs are both tied to the lifetime of the BackgroundP2PProxy
object.  In other words, the proxy will be terminated when the object is
destroyed, if it hasn't been terminated manually with Terminate() or detached
with Detach() first, and if it hasn't self-terminated due to a timeout.

\see P2PProxy() for details about the P2P proxy.
\see SpawnTcpP2PProxy() for a convenient way of creating a background P2P proxy
    that communicates over TCP.
*/
class BackgroundP2PProxy
{
public:
    /**
    \brief  Starts a proxy which uses the given socket to route messages between
            peers.

    It is recommended that `routerSocket` be a socket created with the ZMQ
    context returned by dsb::comm:GlobalContext().  Otherwise, care must be
    taken to ensure that `routerSocket`'s context does not get destroyed while
    the proxy is still running.

    \param [in] routerSocket
        A socket of ROUTER type, which will be used for routing messages between
        clients and servers.
    \param [in] timeout
        The timeout period, or #NEVER_TIMEOUT if the proxy should run
        indefinitely.

    \throws std::invalid_argument if `routerSocket` is not of type ROUTER.
    \throws zmq::error_t if ZMQ reports an error.
    */
    BackgroundP2PProxy(
        zmq::socket_t&& routerSocket,
        boost::chrono::milliseconds timeout = NEVER_TIMEOUT);

    /**
    \brief  Starts a proxy and binds it to the given endpoint.

    \param [in] endpoint
        The endpoint on which the proxy should listen for incoming messages.
    \param [in] timeout
        The timeout period, or #NEVER_TIMEOUT if the proxy should run
        indefinitely.

    \throws std::invalid_argument if `endpoint` is empty.
    \throws zmq::error_t if ZMQ reports an error.
    */
    BackgroundP2PProxy(
        const std::string& endpoint,
        boost::chrono::milliseconds timeout = NEVER_TIMEOUT);

    /**
    \brief  Destructor which terminates the proxy if it hasn't been terminated
            or detached already.
    */
    ~BackgroundP2PProxy() DSB_NOEXCEPT;

    /// Move constructor
    BackgroundP2PProxy(BackgroundP2PProxy&&) DSB_NOEXCEPT;

    /**
    \brief  Move assignment
    
    The effect of this is equivalent to a destruction of the original object
    and a move construction of a new one in its place.
    */
    BackgroundP2PProxy& operator=(BackgroundP2PProxy&&) DSB_NOEXCEPT;

    /**
    \brief  Terminates the proxy
    
    This function sends a TERMINATE message to the proxy's control socket and
    waits for the background thread to end before returning.  If the proxy has
    already terminated, this function returns immediately and has no effect.
    */
    void Terminate();

    /**
    \brief  Detaches this object from the proxy and the background thread.

    After this function is called, the lifetime of the proxy is no longer tied
    to the lifetime of this object.  If the proxy is still running at this
    point, it will keep running until it self-terminates due to a timeout or
    the program ends.
    */
    void Detach();

    // Only for testing/debugging purposes; may be removed without notice.
    boost::thread& Thread__();

private:
    void Init(zmq::socket_t&& routerSocket, boost::chrono::milliseconds timeout);
    zmq::socket_t m_controlSocket;
    boost::thread m_thread;
};


/**
\brief  Spawns a background P2P proxy bound to an ephemeral TCP port.

This is a convenience function which creates a BackgroundP2PProxy which is
already bound to a TCP endpoint using an ephemeral port number.

\param [in] networkInterface
    The network interface to which the proxy should bind.  That is, the proxy
    will be bound to the endpoint `tcp://networkInterface:*`).
\param [in] timeout
    The timeout period, passed directly on to the BackgroundP2PProxy
    constructor.
\param [out] ephemeralPort
    The port number to which the proxy was bound.

\returns A BackgroundP2PProxy object which corresponds to the new proxy.

\throws zmq::error_t if ZMQ reports an error.
*/
BackgroundP2PProxy SpawnTcpP2PProxy(
    const std::string& networkInterface,
    boost::chrono::milliseconds timeout,
    std::uint16_t& ephemeralPort);

/// An overload of SpawnTcpP2PProxy where `timeout = NEVER_TIMEOUT`.
BackgroundP2PProxy SpawnTcpP2PProxy(
    const std::string& networkInterface,
    std::uint16_t& ephemeralPort);


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


/// Flags for P2PReqSocket::Send()
enum P2PSendFlags
{
    /// Allows a message to be sent out of the strict request-reply order.
    SEND_OUT_OF_ORDER = 1
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

    /// Move constructor
#if DSB_HAS_EXPLICIT_DEFAULTED_DELETED_FUNCS
    P2PReqSocket(P2PReqSocket&&) = default;
#else
    P2PReqSocket(P2PReqSocket&&) DSB_NOEXCEPT;
#endif

    /// Move assignment
#if DSB_HAS_EXPLICIT_DEFAULTED_DELETED_FUNCS
    P2PReqSocket& operator=(P2PReqSocket&&) = default;
#else
    P2PReqSocket& operator=(P2PReqSocket&&) DSB_NOEXCEPT;
#endif

#if DSB_HAS_EXPLICIT_DEFAULTED_DELETED_FUNCS
    ~P2PReqSocket() = default;
#endif

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

    This function may only be called if the socket is connected or bound, and
    may normally not be called again before a reply has been received with
    Receive().

    The exception is if the following Send() is called with `flags` set to
    `SEND_OUT_OF_ORDER`, which disables this check.  Use this at your own risk.
    */
    void Send(std::deque<zmq::message_t>& msg, int flags = 0);

    /**
    \brief  Receives a reply.

    This function may only be called if the socket is connected or bound, and
    then only after a request has been sent with Send()().
    */
    void Receive(std::deque<zmq::message_t>& msg);

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
    bool m_awaitingRep;
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

    /// Move constructor
#if DSB_HAS_EXPLICIT_DEFAULTED_DELETED_FUNCS
    P2PRepSocket(P2PRepSocket&&) = default;
#else
    P2PRepSocket(P2PRepSocket&&) DSB_NOEXCEPT;
#endif

    /// Move assignment
#if DSB_HAS_EXPLICIT_DEFAULTED_DELETED_FUNCS
    P2PRepSocket& operator=(P2PRepSocket&&) = default;
#else
    P2PRepSocket& operator=(P2PRepSocket&&) DSB_NOEXCEPT;
#endif

#if DSB_HAS_EXPLICIT_DEFAULTED_DELETED_FUNCS
    ~P2PRepSocket() = default;
#endif

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
    may not be called again before a reply has been sent with Send().
    */
    void Receive(std::deque<zmq::message_t>& msg);

    /**
    \brief  Sends a reply.

    This function may only be called if the socket is connected or bound, and
    then only after a request has been received with Receive().
    */
    void Send(std::deque<zmq::message_t>& msg);

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
    bool m_processingReq;
    std::unique_ptr<zmq::socket_t> m_socket;
    P2PEndpoint m_boundEndpoint;
    zmq::message_t m_clientIdentity;
};


/**
\brief Receives a message, given that one arrives before the timeout is reached.

Existing message content will be overwritten.

\returns `true` if a message was received, or `false` if the function timed out.
\throws zmq::error_t on failure to receive a message frame.
*/
bool Receive(
    P2PRepSocket& socket,
    std::deque<zmq::message_t>& message,
    boost::chrono::milliseconds timeout);


}} // namespace
#endif // header guard
