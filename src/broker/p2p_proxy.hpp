#ifndef DSB_P2P_PROXY_HPP
#define DSB_P2P_PROXY_HPP

#include <cstdint>
#include <memory>
#include <string>
#include "zmq.hpp"


namespace dsb
{
namespace domain_broker
{


/**
\brief  Binds `socket` to an ephemeral TCP port on the given network interface
        and returns the port number.
*/
std::uint16_t BindToEphemeralPort(
    zmq::socket_t& socket,
    const std::string& networkInterface = "*");


/**
\brief  Given a string on the form "tcp://addr:port", returns the port number.

\throws std::invalid_argument if `endpoint` does not have the structure
    described above.
\throws std::bad_cast if the port number is not in a valid number format.
*/
std::uint16_t EndpointPort(const std::string& endpoint);


/**
\brief  Spawns a proxy for peer-to-peer TCP communication in a background thread.

Messages sent to this proxy should have the following format:
~~~
first frame     : recipient identity
second frame    : empty
remaining frames: message content
~~~
If a peer with the given identity is connected to the proxy, it will receive
that message with the following format:
~~~
first frame     : sender identity
second frame    : empty
remaining frames: message content
~~~
If the recipient identity does not correspond to a connected peer, the message
will be dropped.

\param [in] context             A context to use for in-process communication
                                with the background thread.
\param [in]  networkInterface   The network interface to which the proxy should
                                bind.
\param [out] ephemeralPort      The port number to which the proxy was bound.

\returns A PAIR socket which is connected to the proxy thread over the INPROC
    transport.  Any message sent on this socket will terminate the proxy.
*/
zmq::socket_t SpawnP2PProxy(
    std::shared_ptr<zmq::context_t> context,
    const std::string& networkInterface,
    std::uint16_t& ephemeralPort);


}}      // namespace
#endif  // header guard
