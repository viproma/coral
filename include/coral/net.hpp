/**
\file
\brief  Main module header for coral::net
\copyright
    Copyright 2013-2017, SINTEF Ocean and the Coral contributors.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifndef CORAL_NET_HPP
#define CORAL_NET_HPP

#ifdef _WIN32
#   include <winsock2.h>
#else
#   include <netinet/in.h>
#endif

#include <chrono>
#include <cstdint>
#include <string>

#include "coral/config.h"


namespace coral
{
/// Networking, communication and general-purpose protocols
namespace net
{


/// A protocol/transport independent endpoint address specification.
class Endpoint
{
public:
    /// Default constructor; leaves both transport and adress empty.
    Endpoint() CORAL_NOEXCEPT;

    /// Constructor which takes an url on the form "transport://address".
    explicit Endpoint(const std::string& url);

    /// Constructor which takes a transport and an address.
    Endpoint(const std::string& transport, const std::string& address);

    /// Returns the transport.
    std::string Transport() const CORAL_NOEXCEPT;

    /// Returns the address.
    std::string Address() const CORAL_NOEXCEPT;

    /// Returns a URL on the form "transport://address".
    std::string URL() const CORAL_NOEXCEPT;

private:
    std::string m_transport;
    std::string m_address;
};


/**
\brief  Functions and classes used for communication over the
        Internet Protocol.
*/
namespace ip
{

    /**
    \brief  An object which identifies an internet host or network interface as
            either an IPv4 address or a textual name.

    If the address is specified as a string, it may either be an IPv4 address in
    dotted-decimal format, or, depending on the context in which the address is
    used, a host name or an (OS-defined) local network interface name.

    The special name "*" may be used in certain contexts to refer to *all*
    available network interfaces, and corresponds to the POSIX/WinSock constant
    INADDR_ANY and the IPv4 address 0.0.0.0.
    */
    class Address
    {
    public:
        /// Default constructor which sets the address to "*".
        Address() CORAL_NOEXCEPT;

        /**
        \brief  Constructor which takes an address in string form.

        The validity of the address is not checked, and no host name resolution
        or interface-IP lookup is performed.

        \throws std::invalid_argument   If `address` is empty.
        */
        /* implicit */ Address(const std::string& address);

        // C-style version of the above.
        /* implicit */ Address(const char* address);

        /// Constructor which takes an IP address as an in_addr.
        /* implicit */ Address(in_addr address) CORAL_NOEXCEPT;

        /// Returns whether this address is the special "any address" value.
        bool IsAnyAddress() const CORAL_NOEXCEPT;

        /// Returns whether this address is a name (i.e., host or interface name)
        bool IsName() const CORAL_NOEXCEPT;

        /// Returns a string representation of the address.
        std::string ToString() const CORAL_NOEXCEPT;

        /**
        \brief  Returns the address as an in_addr object.

        If the address was specified as "*", this returns an `in_addr` whose
        `s_addr` member is equal to `INADDR_ANY`.  Otherwise, this function
        requires that the address was specified as an IPv4 address in the first
        place.  No host name resolution or interface lookup is performed.

        \throws std::logic_error    If the address could not be converted.
        */
        in_addr ToInAddr() const;

    private:
        friend bool operator==(const Address&, const Address&);
        std::string m_strAddr;
        in_addr m_inAddr;
    };

    /**
    \brief  Equality operator for `coral::net::ip::Address`.

    This returns `true` if and only if the addresses themselves match.
    No host name resolution or interface lookup is performed.
    */
    bool operator==(const Address& a1, const Address& a2);

    /**
    \brief  Inequality operator for `coral::net::ip::Address`.

    This is defined as the negation of
    `operator==(const Address&, const Address&)`.
    */
    inline bool operator!=(const Address& a1, const Address& a2)
    {
        return !operator==(a1, a2);
    }


    /**
    \brief  An object which represents an internet port number.

    This object may contain a port number in the range 0 through 65535, or
    it may, depending on the context in which it is used, contain the special
    value "*", which means "any port" or "OS-assigned (ephemeral) port".
    */
    class Port
    {
    public:
        /// Constructor which takes a port number.
        /* implicit */ Port(std::uint16_t port = 0u) CORAL_NOEXCEPT;

        /**
        \brief  Constructor which takes a port number in string form, or the
                special value "*".

        \throws std::invalid_argument
            If the string does not contain a number.
        \throws std::out_of_range
            If the number is out of the valid port range.
        */
        /* implicit */ Port(const std::string& port);

        // C-style version of the above.
        /* implicit */ Port(const char* port);

        /// Returns whether this is a normal port number in the range 0-65535.
        bool IsNumber() const CORAL_NOEXCEPT;

        /// Returns whether the object was initialised with the special value "*".
        bool IsAnyPort() const CORAL_NOEXCEPT;

        /**
        \brief  Returns the port number.
        \pre IsNumber() must return `true`.
        */
        std::uint16_t ToNumber() const;

        /// Returns a string representation of the port number.
        std::string ToString() const CORAL_NOEXCEPT;

        /**
        \brief  Returns the port number in network byte order.
        \pre IsNumber() must return `true`.
        */
        std::uint16_t ToNetworkByteOrder() const;

        /// Constructs a Port from a port number in network byte order.
        static Port FromNetworkByteOrder(std::uint16_t nPort) CORAL_NOEXCEPT;

    private:
        std::int32_t m_port;
    };


    /**
    \brief  An object which identifies an endpoint for Internet communication
            as a combination of an address and a port number.
    */
    class Endpoint
    {
    public:
        /// Constructs an Endpoint with address "*" and port zero.
        Endpoint() CORAL_NOEXCEPT;

        /// Constructs an Endpoint from an Address and a Port.
        Endpoint(
            const ip::Address& address,
            const ip::Port& port)
            CORAL_NOEXCEPT;

        /**
        \brief  Constructs an Endpoint from a string on the form "address:port",
                where the ":port" part is optional and defaults to port zero.

        \throws std::out_of_range
            If the number is out of the valid port range.
        \throws std::invalid_argument
            If the specification is otherwise invalid.
        */
        explicit Endpoint(const std::string& specification);

        /// Constructs an Endpoint from a `sockaddr_in` object.
        explicit Endpoint(const sockaddr_in& sin);

        /**
        \brief  Constructs an Endpoint from a `sockaddr` object.

        \throws std::invalid_argument
            If the address family of `sa` is not `AF_INET`.
        */
        explicit Endpoint(const sockaddr& sa);

        /// Returns the address.
        const ip::Address& Address() const CORAL_NOEXCEPT;

        /// Sets the address.
        void SetAddress(const ip::Address& value) CORAL_NOEXCEPT;

        /// Returns the port.
        const ip::Port& Port() const CORAL_NOEXCEPT;

        /**
        \brief  Sets the port.
        \note
            The underscore in the name of this function is due to a name
            collision with a macro in the Windows system headers.
        */
        void SetPort_(const ip::Port& value) CORAL_NOEXCEPT;

        /// Returns a string on the form "address:port".
        std::string ToString() const CORAL_NOEXCEPT;

        /**
        \brief  Returns a coral::net::Endpoint object which refers to the
                same endpoint.

        The transport must be specified.  Currently, the only supported
        transport is "tcp".

        \throws std::invalid_argument   If `transport` is empty.
        */
        coral::net::Endpoint ToEndpoint(const std::string& transport) const;

        /**
        \brief  Returns the endpoint address as a `sockaddr_in` object.

        \throws std::logic_error
            If the address is not an IPv4 address or the port is not a normal
            port number.
        */
        sockaddr_in ToSockaddrIn() const;

    private:
        ip::Address m_address;
        ip::Port m_port;
    };

} // namespace ip


/// Class which represents the network location(s) of a slave.
class SlaveLocator
{
public:
    explicit SlaveLocator(
        const Endpoint& controlEndpoint = Endpoint{},
        const Endpoint& dataPubEndpoint = Endpoint{}) CORAL_NOEXCEPT;

    const Endpoint& ControlEndpoint() const CORAL_NOEXCEPT;
    const Endpoint& DataPubEndpoint() const CORAL_NOEXCEPT;

private:
    Endpoint m_controlEndpoint;
    Endpoint m_dataPubEndpoint;
};


}}      // namespace
#endif  // header guard
