/*
Copyright 2013-2017, SINTEF Ocean and the Coral contributors.
This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifdef _WIN32
#   define WIN32_LEAN_AND_MEAN
#endif
#include <coral/net.hpp>

#ifdef _WIN32
#   include <ws2tcpip.h>
#else
#   include <arpa/inet.h>
#endif

#include <cstdlib>
#include <stdexcept>
#include <utility>

#include <zmq.hpp>

#include <coral/error.hpp>
#include <coral/net/ip.hpp>
#include <coral/net/zmqx.hpp>


namespace coral
{
namespace net
{

// =============================================================================
// Endpoint
// =============================================================================

Endpoint::Endpoint() CORAL_NOEXCEPT { }


Endpoint::Endpoint(const std::string& url)
{
    const auto colonPos = url.find("://");
    if (colonPos == std::string::npos) {
        throw std::invalid_argument("Invalid URL: " + url);
    }
    m_transport = url.substr(0, colonPos);
    m_address = url.substr(colonPos + 3);
}


Endpoint::Endpoint(const std::string& transport, const std::string& address)
    : m_transport(transport)
    , m_address(address)
{
}


std::string Endpoint::Transport() const CORAL_NOEXCEPT
{
    return m_transport;
}


std::string Endpoint::Address() const CORAL_NOEXCEPT
{
    return m_address;
}


std::string Endpoint::URL() const CORAL_NOEXCEPT
{
    return m_transport + "://" + m_address;
}


// =============================================================================
// ip::Address
// =============================================================================

// NOTE:
// In this object, either the string member or the in_addr member may be set,
// but not both.  We use the emptiness of the string member as an indicator of
// which it is.

namespace
{
    void ParseAddressString(
        const std::string& address,
        std::string& name,
        in_addr& ipv4)
    {
        CORAL_INPUT_CHECK(!address.empty());
        name.clear();
        std::memset(&ipv4, 0, sizeof(ipv4));
        if (address == "*") {
            ipv4.s_addr = INADDR_ANY;
        } else if (inet_pton(AF_INET, address.c_str(), &ipv4) != 1) {
            name = address;
        }
    }
}

namespace ip
{

Address::Address() CORAL_NOEXCEPT
{
    ParseAddressString("*", m_strAddr, m_inAddr);
}


Address::Address(const std::string& address)
{
    ParseAddressString(address, m_strAddr, m_inAddr);
}


Address::Address(const char* address)
{
    ParseAddressString(address, m_strAddr, m_inAddr);
}


Address::Address(in_addr address) CORAL_NOEXCEPT
    : m_inAddr(address)
{
}


bool Address::IsAnyAddress() const CORAL_NOEXCEPT
{
    return m_strAddr.empty() && m_inAddr.s_addr == INADDR_ANY;
}


bool Address::IsName() const CORAL_NOEXCEPT
{
    return !m_strAddr.empty();
}


std::string Address::ToString() const CORAL_NOEXCEPT
{
    if (m_strAddr.empty()) {
        if (m_inAddr.s_addr == INADDR_ANY) {
            return "*";
        } else {
            return coral::net::ip::IPAddressToString(m_inAddr);
        }
    } else {
        return m_strAddr;
    }
}


in_addr Address::ToInAddr() const
{
    if (m_strAddr.empty()) {
        return m_inAddr;
    } else {
        throw std::logic_error("Not an IPv4 address");
    }
}


bool operator==(const Address& a1, const Address& a2)
{
    return a1.m_strAddr == a2.m_strAddr
        && a1.m_inAddr.s_addr == a2.m_inAddr.s_addr;
}


} // namespace ip


// =============================================================================
// ip::Port
// =============================================================================

namespace
{
    const std::int32_t ANY_PORT = -1;
    const std::int32_t MAX_PORT = 65535;

    bool IsInternetPortNumber(std::int32_t port) CORAL_NOEXCEPT
    {
        return port >= 0 && port <= MAX_PORT;
    }

    std::int32_t ParsePortString(const std::string& s)
    {
        if (s == "*") return ANY_PORT;
        const auto i = std::stoi(s);
        if (!IsInternetPortNumber(i)) {
            throw std::out_of_range("Port number out of range: " + s);
        }
        return i;
    }
}

namespace ip
{

Port::Port(std::uint16_t port) CORAL_NOEXCEPT
    : m_port{port}
{
}


Port::Port(const std::string& port)
    : m_port{ParsePortString(port)}
{
}


Port::Port(const char* port)
    : m_port{ParsePortString(port)}
{
}


bool Port::IsNumber() const CORAL_NOEXCEPT
{
    return IsInternetPortNumber(m_port);
}


bool Port::IsAnyPort() const CORAL_NOEXCEPT
{
    return m_port == ANY_PORT;
}


std::uint16_t Port::ToNumber() const
{
    CORAL_PRECONDITION_CHECK(IsNumber());
    return static_cast<std::uint16_t>(m_port);
}


std::string Port::ToString() const CORAL_NOEXCEPT
{
    if (m_port == ANY_PORT) {
        return "*";
    } else {
        return std::to_string(m_port);
    }
}


std::uint16_t Port::ToNetworkByteOrder() const
{
    return htons(ToNumber());
}


Port Port::FromNetworkByteOrder(std::uint16_t nPort) CORAL_NOEXCEPT
{
    return Port(ntohs(nPort));
}

} // namespace ip


// =============================================================================
// ip::Endpoint
// =============================================================================

namespace ip
{

Endpoint::Endpoint() CORAL_NOEXCEPT
{
}


Endpoint::Endpoint(const ip::Address& address, const ip::Port& port)
    CORAL_NOEXCEPT
    : m_address{address}
    , m_port{port}
{
}


Endpoint::Endpoint(const std::string& specification)
{
    const auto colonPos = specification.find(':');
    m_address = ip::Address{specification.substr(0, colonPos)};
    if (colonPos < specification.size() - 1) {
        m_port = ip::Port{specification.substr(colonPos+1)};
    }
}


Endpoint::Endpoint(const sockaddr_in& sin)
{
    CORAL_INPUT_CHECK(sin.sin_family == AF_INET);
    m_address = ip::Address{sin.sin_addr};
    m_port = ip::Port::FromNetworkByteOrder(sin.sin_port);
}


Endpoint::Endpoint(const sockaddr& sa)
{
    CORAL_INPUT_CHECK(sa.sa_family == AF_INET);
    const auto sin = reinterpret_cast<const sockaddr_in*>(&sa);
    m_address = ip::Address{sin->sin_addr};
    m_port = ip::Port::FromNetworkByteOrder(sin->sin_port);
}


const ip::Address& Endpoint::Address() const CORAL_NOEXCEPT
{
    return m_address;
}


void Endpoint::SetAddress(const ip::Address& value) CORAL_NOEXCEPT
{
    m_address = value;
}


const ip::Port& Endpoint::Port() const CORAL_NOEXCEPT
{
    return m_port;
}


void Endpoint::SetPort_(const ip::Port& value) CORAL_NOEXCEPT
{
    m_port = value;
}


std::string Endpoint::ToString() const CORAL_NOEXCEPT
{
    return m_address.ToString() + ':' + m_port.ToString();
}


coral::net::Endpoint Endpoint::ToEndpoint(const std::string& transport) const
{
    CORAL_INPUT_CHECK(!transport.empty());
    return coral::net::Endpoint{
        transport,
        m_address.ToString() + ':' + m_port.ToString()
    };
}


sockaddr_in Endpoint::ToSockaddrIn() const
{
    sockaddr_in result;
    std::memset(&result, 0, sizeof(result));
    result.sin_family = AF_INET;
    result.sin_port = m_port.ToNetworkByteOrder();
    result.sin_addr = m_address.ToInAddr();
    return result;
}

} // namespace ip


// =============================================================================
// SlaveLocator
// =============================================================================

SlaveLocator::SlaveLocator(
    const Endpoint& controlEndpoint,
    const Endpoint& dataPubEndpoint)
    CORAL_NOEXCEPT
    : m_controlEndpoint{controlEndpoint},
      m_dataPubEndpoint{dataPubEndpoint}
{
}


const Endpoint& SlaveLocator::ControlEndpoint() const CORAL_NOEXCEPT
{
    return m_controlEndpoint;
}


const Endpoint& SlaveLocator::DataPubEndpoint() const CORAL_NOEXCEPT
{
    return m_dataPubEndpoint;
}


}} // namespace
