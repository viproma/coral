/**
\file
\brief  Dynamic network discovery.
*/
#ifndef DSB_PROTOCOL_DISCOVERY_HPP
#define DSB_PROTOCOL_DISCOVERY_HPP

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>

#include "zmq.hpp"

#include "dsb/config.h"
#include "dsb/comm/reactor.hpp"


namespace dsb
{
namespace protocol
{


/**
\brief  A class for broadcasting information about a service, so it can be
        automatically detected on a network.

An object of this class will start broadcasting information about its service
immediately upon construction.  This happens in a background thread.  It is a
good idea to always call Stop() before the object is destroyed, so that errors
are handled properly.  (See ~ServiceBeacon() for more information.)

To detect services that are announced with this class, use ServiceListener.
*/
class ServiceBeacon
{
public:
    /**
    \brief  Constructor.

    \param [in] domainID
        This represents a way to divide the services on the same physical
        network into distinct partitions.  A ServiceListener will only detect
        services whose ServiceBeacon uses the same `domainID`.
    \param [in] serviceType
        The name of the service type, which may be any string of at most
        255 characters.  This is used to filter services in ServiceListener.
    \param [in] serviceIdentifier
        A name which identifies a particular service-providing entity.
        Its length may be up to 255 characters.  Normally, this will be a
        unique name, at least in the context of a particular service on a
        particular domain.
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
        Which UDP port to broadcast to.  The ServiceListener must use the
        same port.

    \throws std::runtime_error on error.
    */
    ServiceBeacon(
        std::uint64_t domainID,
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
    ~ServiceBeacon() DSB_NOEXCEPT;

    ServiceBeacon(const ServiceBeacon&) = delete;
    ServiceBeacon& operator=(const ServiceBeacon&) = delete;

    DSB_DEFINE_DEFAULT_MOVE(ServiceBeacon, m_thread, m_socket);

    /// Stops broadcasting service information.
    void Stop();

private:
    std::thread m_thread;
    zmq::socket_t m_socket;
};


/**
\brief  A class for detecting services on a network.

An object of this class can be used to listen for service announcements
broadcast by one or more ServiceBeacon instances.  (It is recommended
to read the documentation for that class too.)

Unlike ServiceBeacon, this class does not create a background thread;
rather it uses the reactor pattern (specifically, dsb::comm::Reactor) to
deal with incoming data in the current thread.
*/
class ServiceListener
{
public:
    /**
    \brief  The type for functions that handle incoming service notifications.

    Such a function must have the following signature:
    ~~~{.cpp}
    void handler(
        const std::string& address,     // the service's IP address
        const std::string& serviceType, // the service type (see ServiceBeacon)
        const std::string& serviceID,   // the service name (see ServiceBeacon)
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
    \param [in] domainID
        This must match the domain ID of any ServiceBeacon one wishes to
        detect.
    \param [in] networkInterface
        The name or IP address of the network interface to listen on,
        or "*" to listen on all interfaces.
    \param [in] port
        Which UDP port to listen on.  This must match the port used in the
        ServiceBeacon.
    \param [in] onNotification
        A function which will be called whenever a service notification
        is received.

    \throws std::runtime_error on network error.
    */
    ServiceListener(
        dsb::comm::Reactor& reactor,
        std::uint64_t domainID,
        const std::string& networkInterface,
        std::uint16_t port,
        NotificationHandler onNotification);

    /// Destructor
    ~ServiceListener() DSB_NOEXCEPT;

    ServiceListener(const ServiceListener&) = delete;
    ServiceListener& operator=(const ServiceListener&) = delete;

    /// Move constructor
    ServiceListener(ServiceListener&&) DSB_NOEXCEPT;

    /// Move assignment operator
    ServiceListener& operator=(ServiceListener&&) DSB_NOEXCEPT;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};


/**
\brief  A class for keeping track of services on a network.

An object of this class can be used to keep track of services that announce
their presence using ServiceBeacon.  It is built on top of ServiceListener,
but rather than forwarding "raw" beacon pings, it translates these into
events that indicate whether a new service has appeared on the network,
whether one has disappeared, or whether one has changed its data payload.

Unlike ServiceBeacon, this class does not create a background thread;
rather it uses the reactor pattern (specifically, dsb::comm::Reactor) to
deal with incoming data in the current thread.
*/
class ServiceTracker
{
public:
    /**
    \brief  The type for functions that are called when a service is discovered.

    Such a function must have the following signature:
    ~~~{.cpp}
    void handler(
        const std::string& address,     // the service's IP address
        const std::string& serviceType, // the service type (see ServiceBeacon)
        const std::string& serviceID,   // the service name (see ServiceBeacon)
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
        const std::string& serviceType, // the service type (see ServiceBeacon)
        const std::string& serviceID,   // the service name (see ServiceBeacon)
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
        const std::string& serviceType, // the service type (see ServiceBeacon)
        const std::string& serviceID);  // the service name (see ServiceBeacon)
    ~~~
    */
    typedef std::function<void(const std::string&, const std::string&)>
        DisappearedHandler;

    /**
    \brief  Constructor.

    \param [in] reactor
        Used to listen for incoming data.
    \param [in] domainID
        This must match the domain ID of any ServiceBeacon one wishes to
        detect.
    \param [in] networkInterface
        The name or IP address of the network interface to listen on,
        or "*" to listen on all interfaces.
    \param [in] port
        Which UDP port to listen on.  This must match the port used in the
        ServiceBeacon.

    \throws std::runtime_error on network error.
    */
    ServiceTracker(
        dsb::comm::Reactor& reactor,
        std::uint64_t domainID,
        const std::string& networkInterface,
        std::uint16_t port);

    /// Destructor.
    ~ServiceTracker() DSB_NOEXCEPT;

    ServiceTracker(const ServiceTracker&) = delete;
    ServiceTracker& operator=(const ServiceTracker&) = delete;

    /// Move constructor.
    ServiceTracker(ServiceTracker&&) DSB_NOEXCEPT;

    /// Move assignment operator.
    ServiceTracker& operator=(ServiceTracker&&) DSB_NOEXCEPT;

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


}} // namespace
#endif // header guard
