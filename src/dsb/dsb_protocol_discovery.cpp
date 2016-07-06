#include "dsb/protocol/discovery.hpp"

#include <algorithm> // std::copy
#include <cassert>
#include <cstring>
#include <exception>
#include <utility>
#include <vector>

#include "boost/numeric/conversion/cast.hpp"

#include "dsb/comm/messaging.hpp"
#include "dsb/comm/ip.hpp"
#include "dsb/comm/udp.hpp"
#include "dsb/comm/util.hpp"
#include "dsb/error.hpp"
#include "dsb/log.hpp"
#include "dsb/util.hpp"


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
        std::shared_ptr<dsb::comm::UDPBroadcastSocket> udpSocket,
        std::shared_ptr<zmq::socket_t> inprocSocket)
    {
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
                try {
                    udpSocket->Send(message.data(), message.size());
                } catch (const std::exception& e) {
                    dsb::log::Log(dsb::log::error,
                        boost::format("ServiceBeacon thread terminating due to error: %s ")
                            % e.what());
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
    const std::string& networkInterface,
    std::uint16_t port)
    : m_socket(dsb::comm::GlobalContext(), ZMQ_PAIR)
{
    DSB_INPUT_CHECK(serviceType.size() < 256u);
    DSB_INPUT_CHECK(serviceIdentifier.size() < 256u);
    DSB_INPUT_CHECK(payloadSize == 0 || payload != nullptr);
    DSB_INPUT_CHECK(period > std::chrono::milliseconds(0));
    DSB_INPUT_CHECK(!networkInterface.empty());

    // Create the thread-to-thread channel
    const auto endpoint = "inproc://" + dsb::util::RandomUUID();
    m_socket.bind(endpoint);
    auto otherSocket = std::make_shared<zmq::socket_t>(
        dsb::comm::GlobalContext(), ZMQ_PAIR);
    otherSocket->connect(endpoint);

    // Set up the UDP socket
    auto udpSocket = std::make_shared<dsb::comm::UDPBroadcastSocket>(
        networkInterface,
        port,
        dsb::comm::UDPBroadcastSocket::onlySend);

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
        std::move(udpSocket),
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
        const std::string& networkInterface,
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
    dsb::comm::UDPBroadcastSocket m_udpSocket;
};


ServiceListener::Impl::Impl(
    dsb::comm::Reactor& reactor,
    std::uint64_t domainID,
    const std::string& networkInterface,
    std::uint16_t port,
    NotificationHandler onNotification)
    : m_reactor(reactor)
    , m_domainID(domainID)
    , m_onNotification(onNotification)
    , m_udpSocket(networkInterface, port)
{
    DSB_INPUT_CHECK(onNotification != nullptr);
    reactor.AddNativeSocket(
        m_udpSocket.NativeHandle(),
        [this] (dsb::comm::Reactor&, dsb::comm::Reactor::NativeSocket) {
            IncomingBeacon();
        });
}


ServiceListener::Impl::~Impl() DSB_NOEXCEPT
{
    m_reactor.RemoveNativeSocket(m_udpSocket.NativeHandle());
}


void ServiceListener::Impl::IncomingBeacon()
{
    char buffer[65535];
    in_addr peerAddress;
    auto msgSize = m_udpSocket.Receive(
        buffer,
        sizeof(buffer),
        &peerAddress);
    if (msgSize < minMessageSize) {
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
        dsb::comm::IPAddressToString(peerAddress),
        std::string(buffer + minMessageSize, serviceTypeSize),
        std::string(buffer + minMessageSize+serviceTypeSize, serviceIdentifierSize),
        payloadSize ? buffer + minMessageSize+serviceTypeSize+serviceIdentifierSize
                    : nullptr,
        payloadSize);
}


ServiceListener::ServiceListener(
    dsb::comm::Reactor& reactor,
    std::uint64_t domainID,
    const std::string& networkInterface,
    std::uint16_t port,
    NotificationHandler onNotification)
    : m_impl(std::make_unique<Impl>(
        reactor,
        domainID,
        networkInterface,
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
