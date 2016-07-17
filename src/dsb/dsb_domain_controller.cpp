#include "dsb/domain/controller.hpp"

#include <cassert>
#include <unordered_map>

#include "boost/lexical_cast.hpp"
#include "boost/numeric/conversion/cast.hpp"
#include "boost/range/algorithm/find_if.hpp"

#include "zmq.hpp"

#include "dsb/async.hpp"
#include "dsb/bus/slave_provider_comm.hpp"
#include "dsb/comm/messaging.hpp"
#include "dsb/comm/reactor.hpp"
#include "dsb/comm/util.hpp"
#include "dsb/error.hpp"
#include "dsb/log.hpp"
#include "dsb/protocol/discovery.hpp"
#include "dsb/protocol/glue.hpp"
#include "dsb/util.hpp"

#include "domain.pb.h"


namespace dsb
{
namespace domain
{

namespace
{
    // The period of silence before a slave provider is considered "lost".
    const auto SLAVEPROVIDER_TIMEOUT = std::chrono::minutes(10);

    // Mapping from slave provider IDs to slave provider client objects.
    typedef std::unordered_map<
            std::string,
            dsb::bus::SlaveProviderClient>
        SlaveProviderMap;

    // Forward declarations of internal functions, definitions are
    // further down.
    void HandleGetSlaveTypes(
        std::chrono::milliseconds timeout,
        SlaveProviderMap& slaveProviders,
        std::promise<std::vector<dsb::domain::Controller::SlaveType>> promise)
        DSB_NOEXCEPT;
    void HandleInstantiateSlave(
        const std::string& slaveProviderID,
        const std::string& slaveTypeUUID,
        std::chrono::milliseconds instantiationTimeout,
        std::chrono::milliseconds commTimeout,
        SlaveProviderMap& slaveProviders,
        std::promise<dsb::net::SlaveLocator> promise)
        DSB_NOEXCEPT;
}


using namespace std::placeholders;


class Controller::Private
{
public:
    explicit Private(
        const std::string& networkInterface,
        std::uint16_t discoveryPort)
        : m_slaveProviders{}
        , m_serviceTracker{boost::none}
        , m_thread{std::bind(&Private::Init, this, _1, networkInterface, discoveryPort)}
    {
    }

    ~Private() DSB_NOEXCEPT
    {
        // FIXME:
        // Ok, this sucks, and it is a general problem with the CommThread
        // API.  Basically, we shouldn't have to manually destroy these
        // objects, but at the same time, they must be destroyed *before*
        // the background thread terminates because they need to unregister
        // from the Reactor.
        m_thread.Execute<void>([this] (dsb::comm::Reactor&, std::promise<void> p)
        {
            m_serviceTracker = boost::none;
            m_slaveProviders.clear();
            p.set_value();
        }).get();
    }

    Private(const Private&) = delete;
    Private& operator=(const Private&) = delete;
    Private(Private&& other) = delete;
    Private& operator=(Private&& other) = delete;


    std::vector<SlaveType> GetSlaveTypes(
        std::chrono::milliseconds timeout)
    {
        return m_thread.Execute<std::vector<SlaveType>>(
            [=] (dsb::comm::Reactor&, std::promise<std::vector<SlaveType>> promise)
            {
                HandleGetSlaveTypes(
                    timeout,
                    m_slaveProviders,
                    std::move(promise));
            }
        ).get();
    }

    dsb::net::SlaveLocator InstantiateSlave(
        const std::string& slaveProviderID,
        const std::string& slaveTypeUUID,
        std::chrono::milliseconds timeout)
    {
        // Note: It is safe to capture by reference in the lambda because
        // the present thread is blocked waiting for the operation to complete.
        return m_thread.Execute<dsb::net::SlaveLocator>(
            [&] (dsb::comm::Reactor&, std::promise<dsb::net::SlaveLocator> promise)
            {
                HandleInstantiateSlave(
                    slaveProviderID,
                    slaveTypeUUID,
                    timeout,   // instantiation timeout
                    2*timeout, // communication timeout
                    m_slaveProviders,
                    std::move(promise));
            }
        ).get();
    }

private:
    // This function initialises the objects we will use in the background
    // thread, and is executed in that thread.
    void Init(
        dsb::comm::Reactor& reactor,
        const std::string& m_networkInterface,
        std::uint16_t m_discoveryPort)
    {
        const auto reactorPtr = &reactor;
        m_serviceTracker.emplace(reactor, 0, m_networkInterface, m_discoveryPort);
        m_serviceTracker->AddTrackedServiceType(
            "no.sintef.viproma.dsb.slave_provider",
            SLAVEPROVIDER_TIMEOUT,
            // Slave provider discovered:
            [reactorPtr, this] (
                const std::string& address,
                const std::string& serviceType,
                const std::string& serviceID,
                const char* payload,
                std::size_t payloadSize)
            {
                if (payloadSize != 2) {
                    DSB_LOG_TRACE("Domain controller ignoring slave provider beacon due to missing data");
                    return;
                }
                const auto port = dsb::util::DecodeUint16(payload);
                m_slaveProviders.insert(std::make_pair(
                    serviceID,
                    dsb::bus::SlaveProviderClient(*reactorPtr, address, port)));
                DSB_LOG_TRACE(
                    boost::format("Slave provider discovered: %s @ %s:%d")
                    % serviceID % address % port);
            },
            // Slave provider port changed:
            [reactorPtr, this] (
                const std::string& address,
                const std::string& serviceType,
                const std::string& serviceID,
                const char* payload,
                std::size_t payloadSize)
            {
                if (payloadSize != 2) {
                    DSB_LOG_TRACE("Domain controller ignoring slave provider beacon due to missing data");
                    return;
                }
                const auto port = dsb::util::DecodeUint16(payload);
                m_slaveProviders.erase(serviceID);
                m_slaveProviders.insert(std::make_pair(
                    serviceID,
                    dsb::bus::SlaveProviderClient(*reactorPtr, address, port)));
                DSB_LOG_TRACE(
                    boost::format("Slave provider updated: %s @ %s:%d")
                    % serviceID % address % port);
            },
            // Slave provider disappeared:
            [reactorPtr, this] (
                const std::string& serviceType,
                const std::string& serviceID)
            {
                m_slaveProviders.erase(serviceID);
                DSB_LOG_TRACE(boost::format("Slave provider disappeared: %s")
                    % serviceID);
            });
    }

    // For use in background thread
    SlaveProviderMap m_slaveProviders;
    boost::optional<dsb::protocol::ServiceTracker> m_serviceTracker;

    // Background thread
    dsb::async::CommThread m_thread;
};


Controller::Controller(
    const std::string& networkInterface,
    std::uint16_t discoveryPort)
    : m_private{std::make_unique<Private>(networkInterface, discoveryPort)}
{
}


Controller::~Controller() DSB_NOEXCEPT
{
    // Do nothing, everything's handled by ~Private().
}


Controller::Controller(Controller&& other) DSB_NOEXCEPT
    : m_private{std::move(other.m_private)}
{
}


Controller& Controller::operator=(Controller&& other) DSB_NOEXCEPT
{
    m_private = std::move(other.m_private);
    return *this;
}


std::vector<Controller::SlaveType> Controller::GetSlaveTypes(
    std::chrono::milliseconds timeout)
{
    return m_private->GetSlaveTypes(timeout);
}


dsb::net::SlaveLocator Controller::InstantiateSlave(
    const std::string& slaveProviderID,
    const std::string& slaveTypeUUID,
    std::chrono::milliseconds timeout)
{
    return m_private->InstantiateSlave(
        slaveProviderID,
        slaveTypeUUID,
        timeout);
}


namespace // Internal functions
{


// This struct contains the state of an ongoing GetSlaveTypes request
// to multiple slave providers.
struct GetSlaveTypesRequest
{
    int remainingReplies = 0;
    std::vector<dsb::domain::Controller::SlaveType> slaveTypes;
    std::unordered_map<std::string, std::size_t> slaveTypeIndices;

    void AddReply(
        const std::string& slaveProviderID,
        const std::error_code& ec,
        const dsb::model::SlaveTypeDescription* types,
        std::size_t typeCount,
        std::promise<std::vector<dsb::domain::Controller::SlaveType>>& promise);
};


void HandleGetSlaveTypes(
    std::chrono::milliseconds timeout,
    SlaveProviderMap& slaveProviders,
    std::promise<std::vector<dsb::domain::Controller::SlaveType>> promise)
{
    const auto sharedPromise =
        std::make_shared<decltype(promise)>(std::move(promise));
    try {
        const auto state = std::make_shared<GetSlaveTypesRequest>();
        for (auto& slaveProvider : slaveProviders) {
            const auto slaveProviderID = slaveProvider.first;
            slaveProvider.second.GetSlaveTypes(
                [sharedPromise, state, slaveProviderID] (
                    const std::error_code& ec,
                    const dsb::model::SlaveTypeDescription* slaveTypes,
                    std::size_t slaveTypeCount)
                {
                    state->AddReply(
                        slaveProviderID,
                        ec,
                        slaveTypes,
                        slaveTypeCount,
                        *sharedPromise);
                },
                timeout);
            ++(state->remainingReplies);
        }
        DSB_LOG_TRACE(boost::format("Sent GetSlaveTypes request to %d providers")
            % state->remainingReplies);
    } catch (...) {
        sharedPromise->set_exception(std::current_exception());
    }
}


void GetSlaveTypesRequest::AddReply(
    const std::string& slaveProviderID,
    const std::error_code& ec,
    const dsb::model::SlaveTypeDescription* types,
    std::size_t typeCount,
    std::promise<std::vector<dsb::domain::Controller::SlaveType>>& promise)
{
    --remainingReplies;
    if (!ec) {
        for (std::size_t i = 0; i < typeCount; ++i) {
            auto stIt = slaveTypeIndices.find(types[i].UUID());
            if (stIt == slaveTypeIndices.end()) {
                slaveTypes.emplace_back();
                slaveTypes.back().description = types[i];
                stIt = slaveTypeIndices.insert(
                    std::make_pair(types[i].UUID(), slaveTypes.size() - 1)).first;
            }
            slaveTypes[stIt->second].providers.push_back(
                slaveProviderID);
        }
        DSB_LOG_TRACE(
            boost::format("GetSlaveTypes request to slave provider %s returned %d types")
            % slaveProviderID
            % typeCount);
    } else {
        dsb::log::Log(dsb::log::warning, boost::format(
            "GetSlaveTypes request to slave provider %s failed (%s)")
            % slaveProviderID
            % ec.message());
    }
    if (remainingReplies == 0) {
        // All slave providers have replied, now finalise async call.
        promise.set_value(std::move(slaveTypes));
    }
}


void HandleInstantiateSlave(
    const std::string& slaveProviderID,
    const std::string& slaveTypeUUID,
    std::chrono::milliseconds instantiationTimeout,
    std::chrono::milliseconds commTimeout,
    SlaveProviderMap& slaveProviders,
    std::promise<dsb::net::SlaveLocator> promise)
    DSB_NOEXCEPT
{
    const auto sharedPromise =
        std::make_shared<decltype(promise)>(std::move(promise));
    try {
        const auto slaveProvider = slaveProviders.find(slaveProviderID);
        if (slaveProvider == slaveProviders.end()) {
            promise.set_exception(std::make_exception_ptr(
                std::runtime_error("Unknown slave provider: " + slaveProviderID)));
            return;
        }
        slaveProvider->second.InstantiateSlave(
            slaveTypeUUID,
            instantiationTimeout,
            [sharedPromise] (
                const std::error_code& ec,
                const dsb::comm::P2PEndpoint endpoint,
                const std::string& errorMessage)
            {
                if (ec) {
                    sharedPromise->set_exception(std::make_exception_ptr(
                        std::runtime_error(ec.message() + " (" + errorMessage + ")")));
                    return;
                } else {
                    sharedPromise->set_value(dsb::net::SlaveLocator(
                        endpoint.Endpoint(),
                        endpoint.IsBehindProxy()
                            ? endpoint.Identity()
                            : std::string{}));
                    return;
                }
            },
            commTimeout);
    } catch (...) {
        sharedPromise->set_exception(std::current_exception());
    }
}


} // anonymous namespace
}} // namespace dsb::domain
