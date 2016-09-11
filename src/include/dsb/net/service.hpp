/**
\file
\brief  Module header for dsb::net::service
*/
#ifndef DSB_NET_SERVICE_HPP
#define DSB_NET_SERVICE_HPP

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>

#include "zmq.hpp"

#include "dsb/config.h"
#include "dsb/net/reactor.hpp"


namespace dsb
{
namespace net
{

/// Dynamic network service discovery.
namespace service
{


/**
\brief  A class for broadcasting information about a service, so it can be
        automatically detected on a network.

An object of this class will start broadcasting information about its service
immediately upon construction.  This happens in a background thread.  It is a
good idea to always call Stop() before the object is destroyed, so that errors
are handled properly.  (See ~Beacon() for more information.)

To detect services that are announced with this class, use Listener.
*/
class Beacon
{
public:
    /**
    \brief  Constructor.

    \param [in] partitionID
        This represents a way to divide the services on the same physical
        network into distinct partitions.  A Listener will only detect
        services whose Beacon uses the same `partitionID`.
    \param [in] serviceType
        The name of the service type, which may be any string of at most
        255 characters.  This is used to filter services in Listener.
    \param [in] serviceIdentifier
        A name which identifies a particular service-providing entity.
        Its length may be up to 255 characters.  Normally, this will be a
        unique name, at least in the context of a particular service in a
        particular network partition.
    \param [in] payload
        A service-specific data payload.  If `payloadSize` is zero, this
        parameter is ignored.  Otherwise, it must point to an array of size
        at least `payloadSize`.  It is generally recommended that this
        payload be less than 1000 bytes, preferably as small as possible.
        (This has to do with the fact that UDP is used as the underlying
        protocol, and smaller messages make for more reliable delivery.)
    \param [in] payloadSize
        The size of the data payload.
    \param [in] period
        How often the service is announced on the network.  A smaller period
        generally leads to faster detection, but also causes more network
        traffic.  1 second is often a good tradeoff for many application.
    \param [in] networkInterface
        The name or IP address of the network interface to broadcast on,
        or "*" to broadcast on all interfaces.
    \param [in] port
        Which UDP port to broadcast to.  The Listener must use the
        same port.

    \throws std::runtime_error on error.
    */
    Beacon(
        std::uint32_t partitionID,
        const std::string& serviceType,
        const std::string& serviceIdentifier,
        const char* payload,
        std::uint16_t payloadSize,
        std::chrono::milliseconds period,
        const std::string& networkInterface,
        std::uint16_t port);

    /**
    \brief  Destructor.

    The destructor will call Stop() if this hasn't been done already, and
    if that function throws, std::terminate() will be called (because the
    destructor is `noexcept`).  It may therefore be a good idea to call
    Stop() manually before destruction, so that errors may be handled
    properly.
    */
    ~Beacon() DSB_NOEXCEPT;

    Beacon(const Beacon&) = delete;
    Beacon& operator=(const Beacon&) = delete;

    DSB_DEFINE_DEFAULT_MOVE(Beacon, m_thread, m_socket);

    /// Stops broadcasting service information.
    void Stop();

private:
    std::thread m_thread;
    zmq::socket_t m_socket;
};


/**
\brief  A class for detecting services on a network.

An object of this class can be used to listen for service announcements
broadcast by one or more Beacon instances.  (It is recommended
to read the documentation for that class too.)

Unlike Beacon, this class does not create a background thread;
rather it uses the reactor pattern (specifically, dsb::net::Reactor) to
deal with incoming data in the current thread.
*/
class Listener
{
public:
    /**
    \brief  The type for functions that handle incoming service notifications.

    Such a function must have the following signature:
    ~~~{.cpp}
    void handler(
        const std::string& address,     // the service's IP address
        const std::string& serviceType, // the service type (see Beacon)
        const std::string& serviceID,   // the service name (see Beacon)
        const char* payload,            // data payload (or null if none)
        std::size_t payloadSize);       // data payload size
    ~~~
    The `payload` array is not guaranteed to exist beyond this function call,
    so a copy must be made if the data is to be kept around.
    */
    typedef std::function<void (
            const std::string&,
            const std::string&,
            const std::string&,
            const char*,
            std::size_t)>
        NotificationHandler;

    /**
    \brief  Constructor.

    \param [in] reactor
        Used to listen for incoming data.
    \param [in] partitionID
        This must match the partition ID of any Beacon one wishes to
        detect.
    \param [in] networkInterface
        The name or IP address of the network interface to listen on,
        or "*" to listen on all interfaces.
    \param [in] port
        Which UDP port to listen on.  This must match the port used in the
        Beacon.
    \param [in] onNotification
        A function which will be called whenever a service notification
        is received.

    \throws std::runtime_error on network error.
    */
    Listener(
        dsb::net::Reactor& reactor,
        std::uint32_t partitionID,
        const std::string& networkInterface,
        std::uint16_t port,
        NotificationHandler onNotification);

    /// Destructor
    ~Listener() DSB_NOEXCEPT;

    Listener(const Listener&) = delete;
    Listener& operator=(const Listener&) = delete;

    /// Move constructor
    Listener(Listener&&) DSB_NOEXCEPT;

    /// Move assignment operator
    Listener& operator=(Listener&&) DSB_NOEXCEPT;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};


/**
\brief  A class for keeping track of services on a network.

An object of this class can be used to keep track of services that announce
their presence using Beacon.  It is built on top of Listener,
but rather than forwarding "raw" beacon pings, it translates these into
events that indicate whether a new service has appeared on the network,
whether one has disappeared, or whether one has changed its data payload.

Unlike Beacon, this class does not create a background thread;
rather it uses the reactor pattern (specifically, dsb::net::Reactor) to
deal with incoming data in the current thread.
*/
class Tracker
{
public:
    /**
    \brief  The type for functions that are called when a service is discovered.

    Such a function must have the following signature:
    ~~~{.cpp}
    void handler(
        const std::string& address,     // the service's IP address
        const std::string& serviceType, // the service type (see Beacon)
        const std::string& serviceID,   // the service name (see Beacon)
        const char* payload,            // data payload (or null if none)
        std::size_t payloadSize);       // data payload size
    ~~~
    The `payload` array is not guaranteed to exist beyond this function call,
    so a copy must be made if the data is to be kept around.
    */
    typedef std::function<void (
            const std::string&,
            const std::string&,
            const std::string&,
            const char*,
            std::size_t)>
        AppearedHandler;

    /**
    \brief  The type for functions that are called when a service changes its
            data payload.

    Such a function must have the following signature:
    ~~~{.cpp}
    void handler(
        const std::string& address,     // the service's IP address
        const std::string& serviceType, // the service type (see Beacon)
        const std::string& serviceID,   // the service name (see Beacon)
        const char* payload,            // data payload (or null if none)
        std::size_t payloadSize);       // data payload size
    ~~~
    The `payload` array is not guaranteed to exist beyond this function call,
    so a copy must be made if the data is to be kept around.
    */
    typedef AppearedHandler PayloadChangedHandler;

    /**
    \brief  The type for functions that are called when a service disappears.

    Such a function must have the following signature:
    ~~~{.cpp}
    void handler(
        const std::string& serviceType, // the service type (see Beacon)
        const std::string& serviceID);  // the service name (see Beacon)
    ~~~
    */
    typedef std::function<void(const std::string&, const std::string&)>
        DisappearedHandler;

    /**
    \brief  Constructor.

    \param [in] reactor
        Used to listen for incoming data.
    \param [in] partitionID
        This must match the partition ID of any Beacon one wishes to
        detect.
    \param [in] networkInterface
        The name or IP address of the network interface to listen on,
        or "*" to listen on all interfaces.
    \param [in] port
        Which UDP port to listen on.  This must match the port used in the
        Beacon.

    \throws std::runtime_error on network error.
    */
    Tracker(
        dsb::net::Reactor& reactor,
        std::uint32_t partitionID,
        const std::string& networkInterface,
        std::uint16_t port);

    /// Destructor.
    ~Tracker() DSB_NOEXCEPT;

    Tracker(const Tracker&) = delete;
    Tracker& operator=(const Tracker&) = delete;

    /// Move constructor.
    Tracker(Tracker&&) DSB_NOEXCEPT;

    /// Move assignment operator.
    Tracker& operator=(Tracker&&) DSB_NOEXCEPT;

    /**
    \brief  Adds (or updates the settings for) a tracked service type.

    \param [in] serviceType
        The service type to listen for.
    \param [in] timeout
        How long a period of silence from a particular service of this type
        must pass before it is considered to have disappeared.  This should
        be at least a few times larger than the services' beacon period.
    \param [in] onAppearance
        A function that should be called when a new service of this type is
        discovered, or `nullptr` if this event is not to be handled.
    \param [in] onPayloadChange
        A function that should be called when a previously discoverd service
        of this type changes its data "payload", or `nullptr` if this event
        is not to be handled.
    \param [in] onDisappearance
        A function that should be called when a previously discovered service
        of this type disappears again (i.e. a period of `timeout` has passed
        without any pings having been received from it), or `nullptr` if this
        event is not to be handled.
    */
    void AddTrackedServiceType(
        const std::string& serviceType,
        std::chrono::milliseconds timeout,
        AppearedHandler onAppearance,
        PayloadChangedHandler onPayloadChange,
        DisappearedHandler onDisappearance);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};


}}} // namespace
#endif // header guard
