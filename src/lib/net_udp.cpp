#include "coral/net/udp.hpp"

#ifdef _WIN32
#   include <ws2tcpip.h>
#else
#   include <arpa/inet.h>
#   include <sys/socket.h>
#endif

#include <algorithm> // find_if
#include <cstring>
#include <utility>
#include <vector>

#include "coral/net/ip.hpp"
#include "coral/log.hpp"
#include "coral/util.hpp"


namespace
{
#ifdef _WIN32
    const SOCKET INVALID_NATIVE_SOCKET = INVALID_SOCKET;
#   define CLOSE_NATIVE_SOCKET closesocket
#else
    const int INVALID_NATIVE_SOCKET = -1;
#   define CLOSE_NATIVE_SOCKET close
#endif
}


namespace coral
{
namespace net
{
namespace udp
{


class BroadcastSocket::Private
{
public:
    Private(const ip::Address& networkInterface, ip::Port port, int flags)
        : m_socket{INVALID_NATIVE_SOCKET}
        , m_port{port}
    {
        // Parse the `networkInterface` parameter and obtain the listen and
        // broadcast addresses.
        in_addr listenAddress;
        if (networkInterface.IsAnyAddress()) {
            listenAddress = networkInterface.ToInAddr();
            for (const auto& iface : ip::GetNetworkInterfaces()) {
                m_broadcastAddrs.push_back(iface.broadcastAddress);
                CORAL_LOG_TRACE(
                    boost::format("BroadcastSocket: Adding broadcast address %s.")
                        % ip::IPAddressToString(iface.broadcastAddress));
            }
        } else {
            const auto ifaces = ip::GetNetworkInterfaces();
            auto iface = decltype(ifaces)::const_iterator{};
            if (networkInterface.IsName()) {
                const auto interfaceName = networkInterface.ToString();
                iface = std::find_if(
                    begin(ifaces), end(ifaces),
                    [&](const ip::NetworkInterfaceInfo& nii) {
                        return nii.name == interfaceName;
                    });
            } else {
                const auto interfaceAddr = networkInterface.ToInAddr();
                iface = std::find_if(
                    begin(ifaces), end(ifaces),
                    [&](const ip::NetworkInterfaceInfo& nii) {
                        return nii.address.s_addr == interfaceAddr.s_addr;
                    });
            }
            if (iface == end(ifaces)) {
                throw std::runtime_error(
                    "Unknown or invalid network interface: "
                    + networkInterface.ToString());
            }
            listenAddress = iface->address;
            m_broadcastAddrs.push_back(iface->broadcastAddress);
            CORAL_LOG_TRACE(
                boost::format("BroadcastSocket: Adding broadcast address %s.")
                    % ip::IPAddressToString(iface->broadcastAddress));
        }

        bool constructionComplete = false;
#ifdef _WIN32
        // Prepare Windows Sockets
        WSADATA wsaData;
        if (auto wsaError = WSAStartup(MAKEWORD(2, 2), &wsaData)) {
            throw std::runtime_error("Failed to initialise Windows networking");
        }
        auto wsaCleanup = coral::util::OnScopeExit([&]() {
            if (!constructionComplete) WSACleanup();
        });
#endif

        // Create socket
        m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (m_socket == INVALID_NATIVE_SOCKET) {
            throw std::runtime_error("Failed to create UDP socket");
        }
        auto closeSocket = coral::util::OnScopeExit([&]() {
            if (!constructionComplete) CLOSE_NATIVE_SOCKET(m_socket);
        });

        // Enable broadcasting
#ifdef _WIN32
        const BOOL onVal = TRUE;
        const auto on = reinterpret_cast<const char*>(&onVal);
#else
        const int onVal = 1;
        const void* on = &onVal;
#endif
        if (0 != setsockopt(m_socket, SOL_SOCKET, SO_BROADCAST, on, sizeof(onVal))) {
            throw std::runtime_error("Failed to activate broadcast mode on UDP socket");
        }

        if (!(flags & onlySend)) {
            //  Enable multiple listening sockets on same port
            if (0 != setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, on, sizeof(onVal))) {
                throw std::runtime_error("Failed to activate address reuse on UDP socket");
            }

            // Bind socket, to listen for incoming broadcasts
            sockaddr_in address;
            std::memset(&address, 0, sizeof(address));
            address.sin_family = AF_INET;
            address.sin_addr = listenAddress;
            address.sin_port = port.ToNetworkByteOrder();
            if (0 != bind(m_socket, reinterpret_cast<sockaddr*>(&address), sizeof(address))) {
                throw std::runtime_error("Failed to bind UDP socket to local port");
            }
            CORAL_LOG_TRACE(boost::format("BroadcastSocket: Bound to %s:%d")
                % ip::IPAddressToString(listenAddress)
                % port.ToNumber());
        }
        constructionComplete = true;
    }


    ~Private() CORAL_NOEXCEPT
    {
        CLOSE_NATIVE_SOCKET(m_socket);
#ifdef _WIN32
        WSACleanup();
#endif
    }


    Private(const Private&) = delete;
    Private& operator=(const Private&) = delete;
    Private(Private&&) = delete;
    Private& operator=(Private&&) = delete;


    void Send(const char* buffer, std::size_t bufferSize)
    {
        sockaddr_in address;
        std::memset(&address, 0, sizeof(address));
        address.sin_family = AF_INET;
        address.sin_port = m_port.ToNetworkByteOrder();
        for (auto addr : m_broadcastAddrs) {
            address.sin_addr = addr;
            auto bytesSent = sendto(
                m_socket,
                buffer,
#ifdef _WIN32
                (int)
#endif
                bufferSize,
                0, // flags
                reinterpret_cast<const sockaddr*>(&address),
#ifdef _WIN32
                (int)
#endif
                sizeof(address));
            if (bytesSent < 0) {
                throw std::runtime_error("Failed to broadcast UDP message");
            } else if (static_cast<std::size_t>(bytesSent) < bufferSize) {
                coral::log::Log(
                    coral::log::warning,
                    boost::format("Failed to broadcast entire UDP message. %d of %d bytes sent.")
                        % bytesSent % bufferSize);
            }
        }
    }


    std::size_t Receive(char* buffer, std::size_t bufferSize, in_addr* sender)
    {
        sockaddr_in senderAddress;
        std::memset(&senderAddress, 0, sizeof(senderAddress));
#ifdef _WIN32
        int senderAddressSize = static_cast<int>(sizeof(senderAddress));
#else
        socklen_t senderAddressSize = sizeof(senderAddress);
#endif
        const auto msgSize = recvfrom(
            m_socket,
            buffer,
#ifdef _WIN32
            (int)
#endif
            bufferSize,
            0, // flags
            reinterpret_cast<sockaddr*>(&senderAddress),
            &senderAddressSize);
        if (msgSize < 0) {
            throw std::runtime_error(
                "An error occurred while attempting to receive UDP message");
        }
        if (sender != nullptr) {
            *sender = senderAddress.sin_addr;
        }
        return static_cast<std::size_t>(msgSize);
    }


    NativeSocket NativeHandle() const CORAL_NOEXCEPT
    {
        return m_socket;
    }


private:
    NativeSocket m_socket;
    ip::Port m_port;
    std::vector<in_addr> m_broadcastAddrs;
};


BroadcastSocket::BroadcastSocket(
    const ip::Address& networkInterface,
    ip::Port port,
    int flags)
    : m_private{std::make_unique<Private>(networkInterface, port, flags)}
{
}


BroadcastSocket::~BroadcastSocket() CORAL_NOEXCEPT
{
}


BroadcastSocket::BroadcastSocket(BroadcastSocket&& other)
    CORAL_NOEXCEPT
    : m_private(std::move(other.m_private))
{
}


BroadcastSocket& BroadcastSocket::operator=(BroadcastSocket&& other)
    CORAL_NOEXCEPT
{
    m_private = std::move(other.m_private);
    return *this;
}


void BroadcastSocket::Send(const char* buffer, std::size_t bufferSize)
{
    m_private->Send(buffer, bufferSize);
}


std::size_t BroadcastSocket::Receive(
    char* buffer,
    std::size_t bufferSize,
    in_addr* sender)
{
    return m_private->Receive(buffer, bufferSize, sender);
}


BroadcastSocket::NativeSocket BroadcastSocket::NativeHandle()
    const CORAL_NOEXCEPT
{
    return m_private->NativeHandle();
}


}}} // namespace
