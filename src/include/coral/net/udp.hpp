/**
\file
\brief  Module header for coral::net::udp
\copyright
    Copyright 2013-2017, SINTEF Ocean and the Coral contributors.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifndef CORAL_NET_UDP_HPP
#define CORAL_NET_UDP_HPP

#ifdef _WIN32
#   include <winsock2.h>
#else
#   include <netinet/in.h>
#endif

#include <cstdint>
#include <memory>
#include <string>

#include <coral/config.h>
#include <coral/net.hpp>


namespace coral
{
namespace net
{

/// Facilities for communication over the User Datagram Protocol (UDP)
namespace udp
{


/// A class for sending and receiving UDP broadcast messages.
class BroadcastSocket
{
public:
    /// The native socket handle type (`SOCKET` on Windows, `int` on *NIX).
#ifdef _WIN32
    typedef SOCKET NativeSocket;
#else
    typedef int NativeSocket;
#endif

    /// Flags that control the operation of this class.
    enum Flags
    {
        /**
        \brief  Only send, don't receive (i.e., don't bind the socket).

        If this flag is set, Receive() won't work and shouldn't be called.
        */
        onlySend = 1
    };

    /**
    \brief  Constructor.

    \param [in] networkInterface
        The name or IP address of the network interface to broadcast and listen
        on.  The special value "*" means "all interfaces".
    \param [in] port
        The port to broadcast and listen on.
    \param [in] flags
        A bitwise OR of one or more Flags values, or zero to use defaults.

    \throws std::runtime_error on failure.
    */
    BroadcastSocket(
        const ip::Address& networkInterface,
        ip::Port port,
        int flags = 0);

    /// Destructor
    ~BroadcastSocket() CORAL_NOEXCEPT;

    BroadcastSocket(const BroadcastSocket&) = delete;
    BroadcastSocket& operator=(const BroadcastSocket&) = delete;

    /// Move constructor
    BroadcastSocket(BroadcastSocket&&) CORAL_NOEXCEPT;
    /// Move assignment operator
    BroadcastSocket& operator=(BroadcastSocket&&) CORAL_NOEXCEPT;

    /**
    \brief  Broadcasts a message.

    \param [in] buffer
        A buffer of size at least `msgSize`, which contains the message data.
    \param [in] msgSize
        The size of the message.

    \throws std::runtime_error on failure.
    */
    void Send(const char* buffer, std::size_t msgSize);

    /**
    \brief  Receives a message.

    \param [in] buffer
        A pointer to an array of size at least `bufferSize` bytes, to which the
        message data will be written.
    \param [in] bufferSize
        The size of the supplied buffer.
    \param [in] sender
        If nonnull, the object pointed to by this parameter will be filled
        with the IP address of the message sender.

    \returns
        The number of bytes received.  This may be more than `bufferSize`,
        meaning that the message has been truncated.

    \throws std::runtime_error on failure.
    */
    std::size_t Receive(
        char* buffer,
        std::size_t bufferSize,
        ip::Address* sender);

    /// The native socket handle.
    NativeSocket NativeHandle() const CORAL_NOEXCEPT;

private:
    class Private;
    std::unique_ptr<Private> m_private;
};


}}} // namespace
#endif // header guard
