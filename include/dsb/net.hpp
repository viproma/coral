/**
\file
\brief  Main module header for dsb::net
*/
#ifndef DSB_NET_HPP
#define DSB_NET_HPP

#ifdef _WIN32
#   include <winsock2.h>
#else
#   include <netinet/in.h>
#endif

#include <chrono>
#include <cstdint>
#include <string>

#include "dsb/config.h"


namespace dsb
{
/// Network addressing
namespace net
{


/// A protocol/transport independent endpoint address specification.
class Endpoint
{
public:
    /// Default constructor; leaves both transport and adress empty.
    Endpoint() DSB_NOEXCEPT;

    /// Constructor which takes an url on the form "transport://address".
    explicit Endpoint(const std::string& url);

    /// Constructor which takes a transport and an address.
    Endpoint(const std::string& transport, const std::string& address);

    /// Returns the transport.
    std::string Transport() const DSB_NOEXCEPT;

    /// Returns the address.
    std::string Address() const DSB_NOEXCEPT;

    /// Returns a URL on the form "transport://address".
    std::string URL() const DSB_NOEXCEPT;

private:
    std::string m_transport;
    std::string m_address;
};


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
class InetAddress
{
public:
    /// Default constructor which sets the address to "*".
    InetAddress() DSB_NOEXCEPT;

    /**
    \brief  Constructor which takes an address in string form.

    The validity of the address is not checked, and no host name resolution
    or interface-IP lookup is performed.

    \throws std::invalid_argument   If `address` is empty.
    */
    /* implicit */ InetAddress(const std::string& address);

    // C-style version of the above.
    /* implicit */ InetAddress(const char* address);

    /// Constructor which takes an IP address as an in_addr.
    /* implicit */ InetAddress(in_addr address) DSB_NOEXCEPT;

    /// Returns whether this address is the special "any address" value.
    bool IsAnyAddress() const DSB_NOEXCEPT;

    /// Returns a string representation of the address.
    std::string ToString() const DSB_NOEXCEPT;

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
    std::string m_strAddr;
    in_addr m_inAddr;
};


/**
\brief  An object which represents an internet port number.

This object may contain a port number in the range 0 through 65535, or
it may, depending on the context in which it is used, contain the special
value "*", which means "any port" or "OS-assigned (ephemeral) port".
*/
class InetPort
{
public:
    /// Constructor which takes a port number.
    /* implicit */ InetPort(std::uint16_t port = 0u) DSB_NOEXCEPT;

    /**
    \brief  Constructor which takes a port number in string form, or the
            special value "*".

    \throws std::invalid_argument
        If the string does not contain a number.
    \throws std::out_of_range
        If the number is out of the valid port range.
    */
    /* implicit */ InetPort(const std::string& port);

    // C-style version of the above.
    /* implicit */ InetPort(const char* port);

    /// Returns whether this is a normal port number in the range 0-65535.
    bool IsNumber() const DSB_NOEXCEPT;

    /// Returns whether the object was initialised with the special value "*".
    bool IsAnyPort() const DSB_NOEXCEPT;

    /**
    \brief  Returns the port number.
    \pre IsNumber() must return `true`.
    */
    std::uint16_t ToNumber() const;

    /// Returns a string representation of the port number.
    std::string ToString() const DSB_NOEXCEPT;

    /**
    \brief  Returns the port number in network byte order.
    \pre IsNumber() must return `true`.
    */
    std::uint16_t ToNetworkByteOrder() const;

    /// Constructs an InetPort from a port number in network byte order.
    static InetPort FromNetworkByteOrder(std::uint16_t nPort) DSB_NOEXCEPT;

private:
    std::int32_t m_port;
};


/**
\brief  An object which identifies an endpoint for Internet communication
        as a combination of an address and a port number.
*/
class InetEndpoint
{
public:
    /// Constructs an InetEndpoint with address "*" and port zero.
    InetEndpoint() DSB_NOEXCEPT;

    /// Constructs an InetEndpoint from an InetAddress and an InetPort.
    InetEndpoint(
        const InetAddress& address,
        const InetPort& port)
        DSB_NOEXCEPT;

    /**
    \brief  Constructs an InetEndpoint from a string on the form "address:port",
            where the ":port" part is optional and defaults to port zero.

    \throws std::out_of_range
        If the number is out of the valid port range.
    \throws std::invalid_argument
        If the specification is otherwise invalid.
    */
    explicit InetEndpoint(const std::string& specification);

    /// Constructs an InetEndpoint from a `sockaddr_in` object.
    explicit InetEndpoint(const sockaddr_in& sin);

    /**
    \brief  Constructs an InetEndpoint from a `sockaddr` object.

    \throws std::invalid_argument
        If the address family of `sa` is not `AF_INET`.
    */
    explicit InetEndpoint(const sockaddr& sa);

    /// Returns the address.
    const InetAddress& Address() const DSB_NOEXCEPT;

    /// Sets the address.
    void SetAddress(const InetAddress& value) DSB_NOEXCEPT;

    /// Returns the port.
    const InetPort& Port() const DSB_NOEXCEPT;

    /**
    \brief  Sets the port.
    \note
        The underscore in the name of this function is due to a name collision
        with a macro in the Windows headers.
    */
    void SetPort_(const InetPort& value) DSB_NOEXCEPT;

    /// Returns a string on the form "address:port".
    std::string ToString() const DSB_NOEXCEPT;

    /**
    \brief  Returns an Endpoint object which refers to the same endpoint.

    The transport must be specified.  Currently, DSB only supports the "tcp"
    transport.

    \throws std::invalid_argument   If `transport` is empty.
    */
    Endpoint ToEndpoint(const std::string& transport) const;

    /**
    \brief  Returns the endpoint address as a `sockaddr_in` object.

    \throws std::logic_error
        If the address is not an IPv4 address or the port is not a normal port
        number.
    */
    sockaddr_in ToSockaddrIn() const;

private:
    InetAddress m_address;
    InetPort m_port;
};


/// Class which represents the network location(s) of a slave.
class SlaveLocator
{
public:
    explicit SlaveLocator(
        const Endpoint& controlEndpoint = Endpoint{},
        const Endpoint& dataPubEndpoint = Endpoint{}) DSB_NOEXCEPT;

    const Endpoint& ControlEndpoint() const DSB_NOEXCEPT;
    const Endpoint& DataPubEndpoint() const DSB_NOEXCEPT;

private:
    Endpoint m_controlEndpoint;
    Endpoint m_dataPubEndpoint;
};


}}      // namespace
#endif  // header guard
