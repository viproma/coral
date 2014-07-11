#ifndef DSB_BROKER_PROXY_HPP
#define DSB_BROKER_PROXY_HPP

#include <memory>
#include "boost/noncopyable.hpp"
#include "boost/thread.hpp"
#include "zmq.hpp"


namespace dsb
{
namespace broker
{


/**
\brief  An object that can be used to terminate a proxy spawned by SpawnProxy().

Objects of this class are not copyable, but they are movable.  This ensures that
there exists at most one Proxy object per spawned proxy.  It also means that
once the object reaches the end of its lifetime, there is no way to stop the
proxy manually, and it will run independently until the program terminates.
*/
class Proxy : boost::noncopyable
{
public:
    /**
    \brief  Move constructor.
    \post `other` no longer refers to a proxy, and the constructed object refers
        to the proxy previously handled by `other` (if any).
    */
    Proxy(Proxy&& other);

    ~Proxy();

    /**
    \brief  Move assignment operator.
    \post `other` no longer refers to a proxy, and the object assigned to refers
        to the proxy previously handled by `other` (if any).
    */
    Proxy& operator=(Proxy&& rhs);

    /**
    \brief  Stops the proxy.

    This stops the proxy immediately, without transferring any further messages
    between the sockets.  However, if the proxy is in the process of
    transferring a message when this function is called, the transfer will be
    completed.  (This also holds for multipart messages; the proxy will never
    stop before all frames are transferred.)

    Calling Stop() on a proxy which has already been stopped has no effect.
    */
    void Stop();

    /**
    \brief  Returns a reference to the `boost::thread` that manages the proxy
            thread.

    \warning
        This function is included mainly for debugging and testing purposes,
        and may be removed in the future.  Do not rely on its existence.
    */
    boost::thread& Thread__();

private:
    // Only SpawnProxy() may construct this class.
    friend Proxy SpawnProxy(
        std::shared_ptr<zmq::context_t> context,
        zmq::socket_t&& socket1,
        zmq::socket_t&& socket2);
    Proxy(zmq::socket_t controlSocket, boost::thread thread);

    bool m_active;
    zmq::socket_t m_controlSocket;
    boost::thread m_thread;
};


/**
\brief  Spawns a ZMQ proxy.

This function spawns a proxy that runs in a separate thread.  Given two ZMQ
sockets, the proxy transfers any incoming messages on either socket directly to
the other.  It will keep running until the program terminates, or until the
proxy itself is explicitly terminated with Proxy::Stop().

\param [in] context The ZMQ context that is used to create a communication
                    channel between the proxy thread and the parent thread, for
                    the Proxy::Stop() function. It is recommended that this be
                    the same context which is used to create `socket1` and
                    `socket2`, since it will be kept alive for at least as long
                    as the proxy runs (by the magic of `std::shared_ptr`).
\param [in] socket1 One of the sockets that should be linked by the proxy.
\param [in] socket2 One of the sockets that should be linked by the proxy.

\returns An object that can be used to stop the proxy.  This object can safely
    be discarded if this functionality is not required.  In that case, the
    proxy will keep running until program termination.

\throws zmq::error_t if ZMQ reports an error.

\pre `context` refers to a valid context, and `socket1` and `socket2` refer to
    valid, connected/bound sockets.
\post `socket1` and `socket2` no longer refer to sockets (as their contents have
    been moved to the proxy thread).
*/
Proxy SpawnProxy(
    std::shared_ptr<zmq::context_t> context,
    zmq::socket_t&& socket1,
    zmq::socket_t&& socket2);


/**
\brief  Spawns a ZMQ proxy. (Convenience function.)

This function creates two ZMQ sockets and binds them to the specified endpoints.
It then forwards to SpawnProxy() to spawn the actual proxy.

\param [in] context     The ZMQ context which is used to create the sockets.
                        This is also passed directly on to SpawnProxy().
\param [in] socketType1 The type of the first socket.
\param [in] endpoint1   The endpoint to which the first socket gets bound.
\param [in] socketType2 The type of the second socket.
\param [in] endpoint2   The endpoint to which the second socket gets bound.

\returns The return value of SpawnProxy()

\throws zmq::error_t if ZMQ reports an error.

\pre `context` refers to a valid context, `socketType1` and `socketType2` are
    valid ZMQ socket types, `endpoint1` and `endpoint2` are well-formed ZMQ
    endpoint specifications.
*/
Proxy SpawnProxy(
    std::shared_ptr<zmq::context_t> context,
    int socketType1, const std::string& endpoint1,
    int socketType2, const std::string& endpoint2);


}}      // namespace
#endif  // header guard
