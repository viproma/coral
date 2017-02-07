/*
Copyright 2013-2017, SINTEF Ocean and the Coral contributors.
This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifdef _WIN32
#   define NOMINMAX
#endif
#include "coral/net/service.hpp"

#include <algorithm> // std::copy
#include <cassert>
#include <cstring>
#include <exception>
#include <unordered_map>
#include <utility>
#include <vector>

#include "boost/numeric/conversion/cast.hpp"

#include "coral/error.hpp"
#include "coral/log.hpp"
#include "coral/net/ip.hpp"
#include "coral/net/udp.hpp"
#include "coral/net/zmqx.hpp"
#include "coral/util.hpp"


namespace coral
{
namespace net
{
namespace service
{


// =============================================================================
// Beacon
// =============================================================================

namespace
{
    void BeaconThread(
        std::chrono::milliseconds period,
        const std::vector<char>& message,
        std::shared_ptr<coral::net::udp::BroadcastSocket> udpSocket,
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
                if (coral::net::zmqx::ToString(msg) == "STOP") break;
            }
            if (std::chrono::steady_clock::now() >= nextBeacon) {
                try {
                    udpSocket->Send(message.data(), message.size());
                } catch (const std::exception& e) {
                    coral::log::Log(coral::log::error,
                        boost::format("Beacon thread terminating due to error: %s ")
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
    //      partition ID:       32-bit unsigned integer, network byte order
    //      service type size:  8-bit unsigned integer
    //      service name size:  8-bit unsigned integer
    //      payload size:       16-bit unsigned integer, network byte order
    //      service type:       variable-length ASCII string
    //      service name:       variable-length ASCII string
    //      payload:            variable-length byte array
    //
    const char* const protocolMagic = "\0DSD"; // Dynamic Service Discovery
    const std::size_t protocolMagicSize = 4;
    const std::size_t minMessageSize =
        protocolMagicSize
        + 1  // version
        + 4  // partition ID
        + 1  // serviceType size
        + 1  // serviceIdentifier size
        + 2; // payload size
}

Beacon::Beacon(
    std::uint32_t partitionID,
    const std::string& serviceType,
    const std::string& serviceIdentifier,
    const char* payload,
    std::size_t payloadSize,
    std::chrono::milliseconds period,
    const ip::Address& networkInterface,
    ip::Port port)
    : m_socket(coral::net::zmqx::GlobalContext(), ZMQ_PAIR)
{
    CORAL_INPUT_CHECK(serviceType.size() < 256u);
    CORAL_INPUT_CHECK(serviceIdentifier.size() < 256u);
    CORAL_INPUT_CHECK(payloadSize == 0 || payload != nullptr);
    CORAL_INPUT_CHECK(payloadSize < (1u << 16));
    CORAL_INPUT_CHECK(period > std::chrono::milliseconds(0));

    // Create the thread-to-thread channel
    const auto endpoint = "inproc://" + coral::util::RandomUUID();
    m_socket.bind(endpoint);
    auto otherSocket = std::make_shared<zmq::socket_t>(
        coral::net::zmqx::GlobalContext(), ZMQ_PAIR);
    otherSocket->connect(endpoint);

    // Set up the UDP socket
    auto udpSocket = std::make_shared<coral::net::udp::BroadcastSocket>(
        networkInterface,
        port,
        coral::net::udp::BroadcastSocket::onlySend);

    // Create the message to broadcast
    const auto messageSize =
        minMessageSize
        + serviceType.size()
        + serviceIdentifier.size()
        + payloadSize;
    if (messageSize > 1500) {
        // Source of the "1500 bytes" recommendation:
        // http://zguide.zeromq.org/page:all#Cooperative-Discovery-Using-UDP-Broadcasts
        CORAL_LOG_DEBUG("Beacon packet size exceeds 1500 bytes");
    }
    auto message = std::vector<char>(messageSize);
    std::memcpy(&message[0], protocolMagic, protocolMagicSize);
    message[protocolMagicSize] = 0;
    coral::util::EncodeUint32(partitionID, &message[protocolMagicSize+1]);
    message[protocolMagicSize + 5] = static_cast<char>(serviceType.size());
    message[protocolMagicSize + 6] = static_cast<char>(serviceIdentifier.size());
    coral::util::EncodeUint16(
        static_cast<std::uint16_t>(payloadSize),
        &message[protocolMagicSize+7]);
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


Beacon::~Beacon() CORAL_NOEXCEPT
{
    if (m_thread.joinable()) Stop();
}


void Beacon::Stop()
{
    m_socket.send("STOP", 4);
    m_thread.join();
    m_socket.close();
}


// =============================================================================
// Listener
// =============================================================================

class Listener::Impl
{
public:
    Impl(
        coral::net::Reactor& reactor,
        std::uint32_t partitionID,
        const ip::Endpoint& endpoint,
        NotificationHandler onNotification);
    ~Impl() CORAL_NOEXCEPT;
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;

private:
    void IncomingBeacon();

    coral::net::Reactor& m_reactor;
    std::uint32_t m_partitionID;
    NotificationHandler m_onNotification;
    coral::net::udp::BroadcastSocket m_udpSocket;
};


Listener::Impl::Impl(
    coral::net::Reactor& reactor,
    std::uint32_t partitionID,
    const ip::Endpoint& endpoint,
    NotificationHandler onNotification)
    : m_reactor(reactor)
    , m_partitionID(partitionID)
    , m_onNotification(onNotification)
    , m_udpSocket(endpoint.Address(), endpoint.Port())
{
    CORAL_INPUT_CHECK(onNotification != nullptr);
    reactor.AddNativeSocket(
        m_udpSocket.NativeHandle(),
        [this] (coral::net::Reactor&, coral::net::Reactor::NativeSocket) {
            IncomingBeacon();
        });
}


Listener::Impl::~Impl() CORAL_NOEXCEPT
{
    m_reactor.RemoveNativeSocket(m_udpSocket.NativeHandle());
}


void Listener::Impl::IncomingBeacon()
{
    char buffer[65535];
    ip::Address peerAddress;
    const auto msgSize = m_udpSocket.Receive(
        buffer,
        sizeof(buffer),
        &peerAddress);
    if (msgSize < minMessageSize) {
        CORAL_LOG_TRACE("Listener: Ignoring invalid message (too small)");
        return;
    }
    if (0 != std::memcmp(buffer, protocolMagic, protocolMagicSize)) {
        CORAL_LOG_TRACE("Listener: Ignoring invalid message (bad format)");
        return;
    }
    if (buffer[protocolMagicSize] != 0) {
        CORAL_LOG_TRACE(
            boost::format("Listener: Ignoring message of version %d")
            % static_cast<int>(buffer[protocolMagicSize]));
        return;
    }
    const auto partitionID = coral::util::DecodeUint32(buffer+protocolMagicSize+1);
    if (partitionID != m_partitionID) {
        CORAL_LOG_TRACE(
            boost::format("Listener: Ignoring message from partition %d")
            % partitionID);
        return;
    }
    const auto serviceTypeSize =
        static_cast<unsigned char>(buffer[protocolMagicSize+5]);
    const auto serviceIdentifierSize =
        static_cast<unsigned char>(buffer[protocolMagicSize+6]);
    const auto  payloadSize =
        coral::util::DecodeUint16(buffer+protocolMagicSize+7);
    if (static_cast<std::size_t>(msgSize) !=
            minMessageSize + serviceTypeSize + serviceIdentifierSize + payloadSize) {
        CORAL_LOG_TRACE("Listener: Ignoring invalid message (wrong size)");
        return;
    }
    m_onNotification(
        peerAddress,
        std::string(buffer + minMessageSize, serviceTypeSize),
        std::string(buffer + minMessageSize+serviceTypeSize, serviceIdentifierSize),
        payloadSize ? buffer + minMessageSize+serviceTypeSize+serviceIdentifierSize
                    : nullptr,
        payloadSize);
}


Listener::Listener(
    coral::net::Reactor& reactor,
    std::uint32_t partitionID,
    const ip::Endpoint& endpoint,
    NotificationHandler onNotification)
    : m_impl(std::make_unique<Impl>(
        reactor,
        partitionID,
        endpoint,
        std::move(onNotification)))
{
}


Listener::~Listener() CORAL_NOEXCEPT
{
    // Do nothing.  This is only so we can use Impl anonymously
    // in the header.
}

Listener::Listener(Listener&& other) CORAL_NOEXCEPT
    : m_impl(std::move(other.m_impl))
{
}

Listener& Listener::operator=(Listener&& other) CORAL_NOEXCEPT
{
    m_impl = std::move(other.m_impl);
    return *this;
}


// =============================================================================
// Tracker
// =============================================================================

using namespace std::placeholders;

class Tracker::Impl
{
public:
    Impl(
        coral::net::Reactor& reactor,
        std::uint32_t partitionID,
        const ip::Endpoint& endpoint)
        : m_reactor(reactor)
        , m_listener(
            reactor,
            partitionID,
            endpoint,
            std::bind(&Impl::OnNotification, this, _1, _2, _3, _4, _5))
        , m_expiryTimerID(-1)
        , m_smallestExpiryTime(std::chrono::milliseconds::max())
    {
    }

    ~Impl() CORAL_NOEXCEPT
    {
        if (m_expiryTimerID >= 0) {
            m_reactor.RemoveTimer(m_expiryTimerID);
        }
    }

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;

    void AddTrackedServiceType(
        const std::string& serviceType,
        std::chrono::milliseconds expiryTime,
        AppearedHandler onAppearance,
        PayloadChangedHandler onPayloadChange,
        DisappearedHandler onDisappearance)
    {
        auto& s = m_trackedServiceTypes[serviceType];
        s.expiryTime = expiryTime;
        s.onAppearance = std::move(onAppearance);
        s.onPayloadChange = std::move(onPayloadChange);
        s.onDisappearance = std::move(onDisappearance);

        if (expiryTime < m_smallestExpiryTime) {
            if (m_expiryTimerID >= 0) {
                m_reactor.RemoveTimer(m_expiryTimerID);
            }
            m_smallestExpiryTime = expiryTime;
            m_expiryTimerID = m_reactor.AddTimer(
                m_smallestExpiryTime,
                -1,
                [this] (coral::net::Reactor&, int) { CheckTimeouts(); });
        }
    }

private:
    void OnNotification(
        const ip::Address& address,
        const std::string& serviceType,
        const std::string& serviceID,
        const char* payload,
        std::size_t payloadSize)
    {
        const auto handlers = m_trackedServiceTypes.find(serviceType);
        if (handlers == m_trackedServiceTypes.end()) return;

        auto serviceTypeIt = m_currentServices.find(serviceType);
        if (serviceTypeIt == m_currentServices.end()) {
            serviceTypeIt = m_currentServices.insert(std::make_pair(
                serviceType,
                decltype(m_currentServices)::mapped_type())).first;
        }
        auto& services = serviceTypeIt->second;

        auto serviceIt = services.find(serviceID);
        if (serviceIt == services.end()) {
            // We have not seen this particular service before
            auto& service = services[serviceID];
            service.lastSeen = std::chrono::steady_clock::now();
            if (payloadSize > 0) {
                service.payload = std::vector<char>(payload, payload + payloadSize);
            }
            if (handlers->second.onAppearance) {
                handlers->second.onAppearance(
                    address,
                    serviceType,
                    serviceID,
                    service.payload.data(),
                    payloadSize);
            }
        } else {
            // we have seen this particular service before, so check if the
            // payload has changed.
            auto& service = services[serviceID];
            service.lastSeen = std::chrono::steady_clock::now();
            if (payloadSize != service.payload.size()
                    || std::memcmp(payload, service.payload.data(), payloadSize) != 0) {
                service.payload.resize(payloadSize);
                std::memcpy(service.payload.data(), payload, payloadSize);
                if (handlers->second.onPayloadChange) {
                    handlers->second.onPayloadChange(
                        address,
                        serviceType,
                        serviceID,
                        service.payload.data(),
                        payloadSize);
                }
            }
        }
    }

    void CheckTimeouts()
    {
        const auto now = std::chrono::steady_clock::now();
        for (auto& serviceType : m_currentServices) {
            const auto& tracked = m_trackedServiceTypes.at(serviceType.first);
            for (auto service = begin(serviceType.second);
                    service != end(serviceType.second);
                    ) {
                if (now > service->second.lastSeen + tracked.expiryTime) {
                    const auto serviceID = service->first;
                    service = serviceType.second.erase(service);
                    if (tracked.onDisappearance) {
                        tracked.onDisappearance(
                            serviceType.first,
                            serviceID);
                    }
                } else {
                    ++service;
                }
            }
        }
    }

    struct TrackedServiceType
    {
        std::chrono::milliseconds expiryTime;
        AppearedHandler onAppearance;
        PayloadChangedHandler onPayloadChange;
        DisappearedHandler onDisappearance;
    };

    struct Service
    {
        std::chrono::steady_clock::time_point lastSeen;
        std::vector<char> payload;
    };

    coral::net::Reactor& m_reactor;
    Listener m_listener;
    std::unordered_map<std::string, TrackedServiceType> m_trackedServiceTypes;
    std::unordered_map<
            std::string,
            std::unordered_map<std::string, Service>
        > m_currentServices;

    int m_expiryTimerID;
    std::chrono::milliseconds m_smallestExpiryTime;
};


Tracker::Tracker(
    coral::net::Reactor& reactor,
    std::uint32_t partitionID,
    const ip::Endpoint& endpoint)
    : m_impl{std::make_unique<Impl>(reactor, partitionID, endpoint)}
{
}


Tracker::~Tracker() CORAL_NOEXCEPT
{
}


Tracker::Tracker(Tracker&& other) CORAL_NOEXCEPT
    : m_impl(std::move(other.m_impl))
{
}


Tracker& Tracker::operator=(Tracker&& other) CORAL_NOEXCEPT
{
    m_impl = std::move(other.m_impl);
    return *this;
}


void Tracker::AddTrackedServiceType(
    const std::string& serviceType,
    std::chrono::milliseconds timeout,
    AppearedHandler onAppearance,
    PayloadChangedHandler onPayloadChange,
    DisappearedHandler onDisappearance)
{
    m_impl->AddTrackedServiceType(
        serviceType,
        timeout,
        onAppearance,
        onPayloadChange,
        onDisappearance);
}


}}} // namespace
