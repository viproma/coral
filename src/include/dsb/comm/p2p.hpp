/**
\file
\brief  Functions and classes for point-to-point REQ-REP communication,
        optionally via a broker.
*/
#ifndef DSB_COMM_P2P_HPP
#define DSB_COMM_P2P_HPP

#include <cstdint>
#include <memory>
#include <string>
#include "boost/thread.hpp"
#include "zmq.hpp"
#include "dsb/config.h"


namespace dsb
{
namespace comm
{


/**
\brief  A function which runs a proxy for point-to-point communication.

This function is similar to the ZMQ `proxy()` function, except that it is
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

\param [in] routerSocket
    A socket of ROUTER type, which will be used for routing messages between
    clients and servers.
\param [in] controlSocket
    A socket to which a message may be sent to terminate the proxy.

\throws std::invalid_argument if `routerSocket` is not of type ROUTER.
\throws zmq::error_t if ZMQ reports an error.
*/
void P2PProxy(zmq::socket_t& routerSocket, zmq::socket_t* controlSocket = nullptr);


/**
\brief  A class which starts and controls a P2P proxy in a background thread.

When an object of this class is created, it will spawn a background thread and
run dsb::comm::P2PProxy() in that thread.  The lifetime of the proxy and the
thread in which it runs are both tied to the lifetime of the BackgroundP2PProxy
object.  In other words, the proxy will be terminated when the object is
destroyed, if it hasn't been terminated manually with Terminate() or detached
with Detach() first.

\see P2PProxy() for details about the P2P proxy.
*/
class BackgroundP2PProxy
{
public:
    /**
    \brief  Starts a proxy which uses the given socket to route messages between
            peers.

    \param [in] context
        The context to use for the "control" communication between the
        foreground and background threads.
    \param [in] routerSocket
        A socket of ROUTER type, which will be used for routing messages between
        clients and servers.

    \throws std::invalid_argument if `routerSocket` is not of type ROUTER.
    \throws zmq::error_t if ZMQ reports an error.
    */
    BackgroundP2PProxy(
        std::shared_ptr<zmq::context_t> context,
        zmq::socket_t&& routerSocket);

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
    waits for the background thread to end.

    \pre The proxy has not been terminated or detached already.
    */
    void Terminate();

    /**
    \brief  Detaches this object from the proxy and the background thread.

    After calling this function, the lifetime of the proxy is no longer tied
    to the lifetime of this object, and the proxy will be running until the
    program terminates.  The Terminate() function may no longer be used to
    stop the proxy.

    \pre The proxy has not been terminated or detached already.
    */
    void Detach();

private:
    std::shared_ptr<zmq::context_t> m_context;
    zmq::socket_t m_controlSocket;
    boost::thread m_thread;
};


/**
\brief  Spawns a background P2P proxy bound to an ephemeral TCP port.

This is a convenience function which creates a BackgroundP2PProxy which is
already bound to a TCP endpoint using an ephemeral port number.

\param [in] context    
    A ZMQ context which will be used for the proxy router socket as well as
    being passed on to the BackgroundP2PProxy constructor.
\param [in] networkInterface
    The network interface to which the proxy should bind.  That is, the proxy
    will be bound to the endpoint `tcp://networkInterface:*`).
\param [out] ephemeralPort
    The port number to which the proxy was bound.

\returns A BackgroundP2PProxy object which corresponds to the new proxy.

\throws zmq::error_t if ZMQ reports an error.
*/
BackgroundP2PProxy SpawnTcpP2PProxy(
    std::shared_ptr<zmq::context_t> context,
    const std::string& networkInterface,
    std::uint16_t& ephemeralPort);


}} // namespace
#endif // header guard
