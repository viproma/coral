#ifdef _WIN32
#   define WIN32_LEAN_AND_MEAN
#endif
#include "dsb/net.hpp"

#ifdef _WIN32
#   include <ws2tcpip.h>
#else
#   include <arpa/inet.h>
#endif

#include <cstdlib>
#include <stdexcept>
#include <utility>
#include "zmq.hpp"

#include "dsb/net/ip.hpp"
#include "dsb/net/messaging.hpp"
#include "dsb/net/util.hpp"
#include "dsb/error.hpp"


namespace dsb
{
namespace net
{

// =============================================================================
// InetEndpoint
// =============================================================================

Endpoint::Endpoint() DSB_NOEXCEPT { }


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


std::string Endpoint::Transport() const DSB_NOEXCEPT
{
    return m_transport;
}


std::string Endpoint::Address() const DSB_NOEXCEPT
{
    return m_address;
}


std::string Endpoint::URL() const DSB_NOEXCEPT
{
    return m_transport + "://" + m_address;
}


// =============================================================================
// InetAddress
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
        DSB_INPUT_CHECK(!address.empty());
        name.clear();
        std::memset(&ipv4, 0, sizeof(ipv4));
        if (address == "*") {
            ipv4.s_addr = INADDR_ANY;
        } else if (inet_pton(AF_INET, address.c_str(), &ipv4) != 1) {
            name = address;
        }
    }
}

InetAddress::InetAddress() DSB_NOEXCEPT
{
    ParseAddressString("*", m_strAddr, m_inAddr);
}


InetAddress::InetAddress(const std::string& address)
{
    ParseAddressString(address, m_strAddr, m_inAddr);
}


InetAddress::InetAddress(const char* address)
{
    ParseAddressString(address, m_strAddr, m_inAddr);
}


InetAddress::InetAddress(in_addr address) DSB_NOEXCEPT
    : m_inAddr(address)
{
}


bool InetAddress::IsAnyAddress() const DSB_NOEXCEPT
{
    return m_strAddr.empty() && m_inAddr.s_addr == INADDR_ANY;
}


std::string InetAddress::ToString() const DSB_NOEXCEPT
{
    if (m_strAddr.empty()) {
        if (m_inAddr.s_addr == INADDR_ANY) {
            return "*";
        } else {
            return dsb::net::IPAddressToString(m_inAddr);
        }
    } else {
        return m_strAddr;
    }
}


in_addr InetAddress::ToInAddr() const
{
    if (m_strAddr.empty()) {
        return m_inAddr;
    } else {
        throw std::logic_error("Not an IPv4 address");
    }
}


// =============================================================================
// InetPort
// =============================================================================

namespace
{
    const std::int32_t ANY_PORT = -1;
    const std::int32_t MAX_PORT = 65535;

    bool IsInternetPortNumber(std::int32_t port) DSB_NOEXCEPT
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


InetPort::InetPort(std::uint16_t port) DSB_NOEXCEPT
    : m_port{port}
{
}


InetPort::InetPort(const std::string& port)
    : m_port{ParsePortString(port)}
{
}


InetPort::InetPort(const char* port)
    : m_port{ParsePortString(port)}
{
}


bool InetPort::IsNumber() const DSB_NOEXCEPT
{
    return IsInternetPortNumber(m_port);
}


bool InetPort::IsAnyPort() const DSB_NOEXCEPT
{
    return m_port == ANY_PORT;
}


std::uint16_t InetPort::ToNumber() const
{
    DSB_PRECONDITION_CHECK(IsNumber());
    return static_cast<std::uint16_t>(m_port);
}


std::string InetPort::ToString() const DSB_NOEXCEPT
{
    if (m_port == ANY_PORT) {
        return "*";
    } else {
        return std::to_string(m_port);
    }
}


std::uint16_t InetPort::ToNetworkByteOrder() const
{
    return htons(ToNumber());
}


InetPort InetPort::FromNetworkByteOrder(std::uint16_t nPort) DSB_NOEXCEPT
{
    return InetPort(ntohs(nPort));
}


// =============================================================================
// InetEndpoint
// =============================================================================


InetEndpoint::InetEndpoint() DSB_NOEXCEPT
{
}


InetEndpoint::InetEndpoint(const InetAddress& address, const InetPort& port)
    DSB_NOEXCEPT
    : m_address{address}
    , m_port{port}
{
}


InetEndpoint::InetEndpoint(const std::string& specification)
{
    const auto colonPos = specification.find(':');
    m_address = InetAddress{specification.substr(0, colonPos)};
    if (colonPos < specification.size() - 1) {
        m_port = InetPort{specification.substr(colonPos+1)};
    }
}


InetEndpoint::InetEndpoint(const sockaddr_in& sin)
{
    DSB_INPUT_CHECK(sin.sin_family == AF_INET);
    m_address = InetAddress{sin.sin_addr};
    m_port = InetPort::FromNetworkByteOrder(sin.sin_port);
}


InetEndpoint::InetEndpoint(const sockaddr& sa)
{
    DSB_INPUT_CHECK(sa.sa_family == AF_INET);
    const auto sin = reinterpret_cast<const sockaddr_in*>(&sa);
    m_address = InetAddress{sin->sin_addr};
    m_port = InetPort::FromNetworkByteOrder(sin->sin_port);
}


const InetAddress& InetEndpoint::Address() const DSB_NOEXCEPT
{
    return m_address;
}


void InetEndpoint::SetAddress(const InetAddress& value) DSB_NOEXCEPT
{
    m_address = value;
}


const InetPort& InetEndpoint::Port() const DSB_NOEXCEPT
{
    return m_port;
}


void InetEndpoint::SetPort_(const InetPort& value) DSB_NOEXCEPT
{
    m_port = value;
}


std::string InetEndpoint::ToString() const DSB_NOEXCEPT
{
    return m_address.ToString() + ':' + m_port.ToString();
}


Endpoint InetEndpoint::ToEndpoint(const std::string& transport) const
{
    DSB_INPUT_CHECK(!transport.empty());
    return Endpoint{transport, m_address.ToString() + ':' + m_port.ToString()};
}


sockaddr_in InetEndpoint::ToSockaddrIn() const
{
    sockaddr_in result;
    std::memset(&result, 0, sizeof(result));
    result.sin_family = AF_INET;
    result.sin_port = m_port.ToNetworkByteOrder();
    result.sin_addr = m_address.ToInAddr();
    return result;
}


// =============================================================================
// SlaveLocator
// =============================================================================

SlaveLocator::SlaveLocator(
    const Endpoint& controlEndpoint,
    const Endpoint& dataPubEndpoint)
    DSB_NOEXCEPT
    : m_controlEndpoint{controlEndpoint},
      m_dataPubEndpoint{dataPubEndpoint}
{
}


const Endpoint& SlaveLocator::ControlEndpoint() const DSB_NOEXCEPT
{
    return m_controlEndpoint;
}


const Endpoint& SlaveLocator::DataPubEndpoint() const DSB_NOEXCEPT
{
    return m_dataPubEndpoint;
}


}} // namespace
