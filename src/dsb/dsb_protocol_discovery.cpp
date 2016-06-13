#include "dsb/protocol/discovery.hpp"

#ifdef _WIN32
#   include <winsock2.h>
#else
#   include <arpa/inet.h>
#   include <sys/socket.h>
#endif

#include <algorithm>
#include <cassert>
#include <cstring>
#include <vector>
#include <utility>

#include "boost/numeric/conversion/cast.hpp"

#include "dsb/comm/messaging.hpp"
#include "dsb/comm/util.hpp"
#include "dsb/error.hpp"
#include "dsb/log.hpp"
#include "dsb/util.hpp"

// Even out some of the differences between Windows and BSD sockets
#ifdef _WIN32
#   define DSB_CLOSE_SOCKET closesocket
    namespace
    {
        typedef SOCKET NativeSocket;
        typedef int socklen_t;
    }
#else
#   define DSB_CLOSE_SOCKET close
    namespace
    {
        const int INVALID_SOCKET = -1;
        const int SOCKET_ERROR = -1;
        typedef int NativeSocket;
    }
#endif


namespace dsb
{
namespace protocol
{

// =============================================================================
// ServiceBeacon
// =============================================================================

namespace
{
    void BeaconThread(
        std::chrono::milliseconds period,
        const std::vector<char>& message,
        NativeSocket udpSocket,
        std::shared_ptr<zmq::socket_t> inprocSocket)
    {
        // Release resources that were acquired in the ServiceBeacon
        // constructor.
#ifdef _WIN32
        auto wsaCleanup = dsb::util::OnScopeExit([]() { WSACleanup(); });
#endif
        auto closeSocket = dsb::util::OnScopeExit([udpSocket]() {
            DSB_CLOSE_SOCKET(udpSocket);
        });

        // Messaging loop
        auto pollItem =
            zmq::pollitem_t{static_cast<void*>(*inprocSocket), 0, ZMQ_POLLIN, 0};
        auto nextBeacon = std::chrono::steady_clock::now();
        for (;;) {
            const auto timeout =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    nextBeacon - std::chrono::steady_clock::now());
            zmq::poll(&pollItem, 1, boost::numeric_cast<long>(timeout.count()));
            if (pollItem.revents & ZMQ_POLLIN) {
                zmq::message_t msg;
                inprocSocket->recv(&msg);
                assert(!msg.more());
                if (dsb::comm::ToString(msg) == "STOP") break;
            }
            if (std::chrono::steady_clock::now() >= nextBeacon) {
                const auto sendStatus = send(
                    udpSocket, message.data(), message.size(), 0);
                if (sendStatus == SOCKET_ERROR) {
                    dsb::log::Log(dsb::log::error,
                        "ServiceBeacon: Error sending UDP message. "
                        "Beacon thread terminating.");
                    return;
                }
                nextBeacon = std::chrono::steady_clock::now() + period;
            }
        }
    }

    // The format of a beacon message is as follows:
    //
    //      magic string:       4 bytes
    //      protocol version:   8-bit unsigned integer
    //      domain ID:          64-bit unsigned integer, network byte order
    //      service type size:  8-bit unsigned integer
    //      service name size:  8-bit unsigned integer
    //      payload size:       16-bit unsigned integer, network byte order
    //      service type:       variable-length ASCII string
    //      service name:       variable-length ASCII string
    //      payload:            variable-length byte array
    //
    const char* const protocolMagic = "\0DSD"; // DSB Service Discovery
    const std::size_t protocolMagicSize = 4;
    const std::size_t minMessageSize =
        protocolMagicSize
        + 1  // version
        + 8  // domain ID
        + 1  // serviceType size
        + 1  // serviceIdentifier size
        + 2; // payload size
}

ServiceBeacon::ServiceBeacon(
    std::uint64_t domainID,
    const std::string& serviceType,
    const std::string& serviceIdentifier,
    const char* payload,
    std::uint16_t payloadSize,
    std::chrono::milliseconds period,
    std::uint16_t port)
    : m_socket(dsb::comm::GlobalContext(), ZMQ_PAIR)
{
    DSB_INPUT_CHECK(serviceType.size() < 256u);
    DSB_INPUT_CHECK(serviceIdentifier.size() < 256u);
    DSB_INPUT_CHECK(payloadSize == 0 || payload != nullptr);
    DSB_INPUT_CHECK(period > std::chrono::milliseconds(0));

    // Create the thread-to-thread channel
    const auto endpoint = "inproc://" + dsb::util::RandomUUID();
    m_socket.bind(endpoint);
    auto otherSocket = std::make_shared<zmq::socket_t>(
        dsb::comm::GlobalContext(), ZMQ_PAIR);
    otherSocket->connect(endpoint);

    // Set up the UDP port
#ifdef _WIN32
    WSADATA wsaData;
    if (auto wsaError = WSAStartup(MAKEWORD(2, 2), &wsaData)) {
        throw std::runtime_error("Failed to initialise Windows networking");
    }
    auto wsaCleanup = dsb::util::OnScopeExit([this]() {
        if (!m_thread.joinable()) WSACleanup();
    });
#endif

    auto udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udpSocket == INVALID_SOCKET) {
        throw std::runtime_error("Failed to create UDP socket");
    }
    auto closeSocket = dsb::util::OnScopeExit([udpSocket, this]() {
        if (!m_thread.joinable()) DSB_CLOSE_SOCKET(udpSocket);
    });

#ifdef _WIN32
    const BOOL onVal = TRUE;
    const auto on = reinterpret_cast<const char*>(&onVal);
#else
    const int onVal = 1;
    const void* on = &onVal;
#endif
    if (setsockopt(udpSocket, SOL_SOCKET, SO_BROADCAST, on, sizeof(onVal))
            == SOCKET_ERROR) {
        throw std::runtime_error("Failed to activate broadcast mode on UDP socket");
    }

    sockaddr_in address;
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    address.sin_port = htons(port);
    if (connect(udpSocket, reinterpret_cast<const sockaddr*>(&address), sizeof(address))
            == SOCKET_ERROR) {
        throw std::runtime_error(
            "Failed to set destination address or port for UDP socket");
    }

    // Create the message to broadcast
    const auto messageSize =
        minMessageSize
        + serviceType.size()
        + serviceIdentifier.size()
        + payloadSize;
    if (messageSize > 1500) {
        // Source of the "1500 bytes" recommendation:
        // http://zguide.zeromq.org/page:all#Cooperative-Discovery-Using-UDP-Broadcasts
        DSB_LOG_DEBUG("ServiceBeacon packet size exceeds 1500 bytes");
    }
    auto message = std::vector<char>(messageSize);
    std::memcpy(&message[0], protocolMagic, protocolMagicSize);
    message[protocolMagicSize] = 0;
    dsb::util::EncodeUint64(domainID, &message[protocolMagicSize+1]);
    message[protocolMagicSize + 9] = serviceType.size();
    message[protocolMagicSize + 10] = serviceIdentifier.size();
    dsb::util::EncodeUint16(payloadSize, &message[protocolMagicSize+11]);
    auto putIter = message.begin() + minMessageSize;
    putIter = std::copy(serviceType.begin(), serviceType.end(), putIter);
    putIter = std::copy(serviceIdentifier.begin(), serviceIdentifier.end(), putIter);
    putIter = std::copy(payload, payload + payloadSize, putIter);
    assert(putIter == message.end());

    // Creating the thread must be the last thing we do, because we've
    // used the thread object as an indicator of whether the constructor
    // succeeded.
    m_thread = std::thread(&BeaconThread,
        period,
        std::move(message),
        udpSocket,
        std::move(otherSocket));
}


ServiceBeacon::~ServiceBeacon() DSB_NOEXCEPT
{
    if (m_thread.joinable()) Stop();
}


void ServiceBeacon::Stop()
{
    m_socket.send("STOP", 4);
    m_thread.join();
    m_socket.close();
}


// =============================================================================
// ServiceListener
// =============================================================================

class ServiceListener::Impl
{
public:
    Impl(
        dsb::comm::Reactor& reactor,
        std::uint64_t domainID,
        std::uint16_t port,
        NotificationHandler onNotification);
    ~Impl() DSB_NOEXCEPT;
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;

private:
    void IncomingBeacon();

    dsb::comm::Reactor& m_reactor;
    std::uint64_t m_domainID;
    NotificationHandler m_onNotification;
    NativeSocket m_udpSocket;
};


ServiceListener::Impl::Impl(
    dsb::comm::Reactor& reactor,
    std::uint64_t domainID,
    std::uint16_t port,
    NotificationHandler onNotification)
    : m_reactor(reactor),
      m_domainID(domainID),
      m_onNotification(onNotification)
{
    DSB_INPUT_CHECK(onNotification != nullptr);

    bool constructionComplete = false;
#ifdef _WIN32
    WSADATA wsaData;
    if (auto wsaError = WSAStartup(MAKEWORD(2, 2), &wsaData)) {
        throw std::runtime_error("Failed to initialise Windows networking");
    }
    auto wsaCleanup = dsb::util::OnScopeExit([&]() {
        if (!constructionComplete) WSACleanup();
    });
#endif

    m_udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_udpSocket == INVALID_SOCKET) {
        throw std::runtime_error("Failed to create UDP socket");
    }
    auto closeSocket = dsb::util::OnScopeExit([&]() {
        if (!constructionComplete) DSB_CLOSE_SOCKET(m_udpSocket);
    });

    sockaddr_in address;
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);
    if (bind(m_udpSocket, reinterpret_cast<sockaddr*>(&address), sizeof(address))
            == SOCKET_ERROR) {
        throw std::runtime_error("Failed to bind UDP socket to local port");
    }

    reactor.AddNativeSocket(
        m_udpSocket,
        [this] (dsb::comm::Reactor&, dsb::comm::Reactor::NativeSocket) {
            IncomingBeacon();
        });
    constructionComplete = true;
}


ServiceListener::Impl::~Impl() DSB_NOEXCEPT
{
    assert(m_udpSocket != INVALID_SOCKET);
    m_reactor.RemoveNativeSocket(m_udpSocket);
    DSB_CLOSE_SOCKET(m_udpSocket);
#ifdef _WIN32
    WSACleanup();
#endif
}


void ServiceListener::Impl::IncomingBeacon()
{
    sockaddr_in peerAddress;
    std::memset(&peerAddress, 0, sizeof(peerAddress));
    socklen_t peerAddressSize = sizeof(peerAddress);
    char buffer[65535];
    auto msgSize = recvfrom(
        m_udpSocket,
        buffer,
        sizeof(buffer),
        0,
        reinterpret_cast<sockaddr*>(&peerAddress),
        &peerAddressSize);
    if (msgSize == SOCKET_ERROR) {
        throw std::runtime_error("Error receiving data from UDP socket");
    }
    if (static_cast<std::size_t>(msgSize) < minMessageSize) {
        DSB_LOG_TRACE("ServiceListener: Ignoring invalid message (too small)");
        return;
    }
    if (0 != std::memcmp(buffer, protocolMagic, protocolMagicSize)) {
        DSB_LOG_TRACE("ServiceListener: Ignoring invalid message (bad format)");
        return;
    }
    if (buffer[protocolMagicSize] != 0) {
        DSB_LOG_TRACE(
            boost::format("ServiceListener: Ignoring message of version %d")
            % static_cast<int>(buffer[protocolMagicSize]));
        return;
    }
    const auto domainID = dsb::util::DecodeUint64(buffer+protocolMagicSize+1);
    if (domainID != m_domainID) {
        DSB_LOG_TRACE(
            boost::format("ServiceListener: Ignoring message from domain %d")
            % domainID);
        return;
    }
    const auto serviceTypeSize =
        static_cast<unsigned char>(buffer[protocolMagicSize+9]);
    const auto serviceIdentifierSize =
        static_cast<unsigned char>(buffer[protocolMagicSize+10]);
    const auto  payloadSize =
        dsb::util::DecodeUint16(buffer+protocolMagicSize+11);
    if (static_cast<std::size_t>(msgSize) !=
            minMessageSize + serviceTypeSize + serviceIdentifierSize + payloadSize) {
        DSB_LOG_TRACE("ServiceListener: Ignoring invalid message (wrong size)");
        return;
    }
    m_onNotification(
        inet_ntoa(peerAddress.sin_addr),
        std::string(buffer + minMessageSize, serviceTypeSize),
        std::string(buffer + minMessageSize+serviceTypeSize, serviceIdentifierSize),
        payloadSize ? buffer + minMessageSize+serviceTypeSize+serviceIdentifierSize
                    : nullptr,
        payloadSize);
}


ServiceListener::ServiceListener(
    dsb::comm::Reactor& reactor,
    std::uint64_t domainID,
    std::uint16_t port,
    NotificationHandler onNotification)
    : m_impl(std::make_unique<Impl>(
        reactor,
        domainID,
        port,
        std::move(onNotification)))
{
}


ServiceListener::~ServiceListener() DSB_NOEXCEPT
{
    // Do nothing.  This is only so we can use Impl anonymously
    // in the header.
}

ServiceListener::ServiceListener(ServiceListener&& other) DSB_NOEXCEPT
    : m_impl(std::move(other.m_impl))
{
}

ServiceListener& ServiceListener::operator=(ServiceListener&& other) DSB_NOEXCEPT
{
    m_impl = std::move(other.m_impl);
    return *this;
}


}} // namespace
