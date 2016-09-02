#include "dsb/domain/controller.hpp"

#include <cassert>
#include <unordered_map>

#include "boost/optional.hpp"

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
    void SetupSlaveProviderTracking(
        dsb::protocol::ServiceTracker& tracker,
        SlaveProviderMap& slaveProviderMap,
        dsb::comm::Reactor& reactor);
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
    Private(
        const std::string& networkInterface,
        std::uint16_t discoveryPort)
        : m_thread{}
    {
        m_thread.Execute<void>([&]
            (dsb::comm::Reactor& reactor, BgData& bgData, std::promise<void> status)
        {
            try {
                bgData.serviceTracker.emplace(
                    reactor,
                    0,
                    networkInterface,
                    discoveryPort);
                SetupSlaveProviderTracking(
                    *bgData.serviceTracker,
                    bgData.slaveProviders,
                    reactor);
                status.set_value();
            } catch (...) {
                status.set_exception(std::current_exception());
            }
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
            [=] (
                dsb::comm::Reactor&,
                BgData& bgData,
                std::promise<std::vector<SlaveType>> result)
            {
                HandleGetSlaveTypes(
                    timeout,
                    bgData.slaveProviders,
                    std::move(result));
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
            [&] (
                dsb::comm::Reactor&,
                BgData& bgData,
                std::promise<dsb::net::SlaveLocator> result)
            {
                HandleInstantiateSlave(
                    slaveProviderID,
                    slaveTypeUUID,
                    timeout,   // instantiation timeout
                    2*timeout, // communication timeout
                    bgData.slaveProviders,
                    std::move(result));
            }
        ).get();
    }

private:
    struct BgData
    {
        SlaveProviderMap slaveProviders;
        boost::optional<dsb::protocol::ServiceTracker> serviceTracker;
    };
    dsb::async::CommThread<BgData> m_thread;
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

// Configures the service tracker to automatically add and remove
// slave providers in the slave provider map.
void SetupSlaveProviderTracking(
    dsb::protocol::ServiceTracker& tracker,
    SlaveProviderMap& slaveProviderMap,
    dsb::comm::Reactor& reactor)
{
    const auto slaveProviderMapPtr = &slaveProviderMap;
    const auto reactorPtr = &reactor;

    tracker.AddTrackedServiceType(
        "no.sintef.viproma.dsb.slave_provider",
        SLAVEPROVIDER_TIMEOUT,
        // Slave provider discovered:
        [slaveProviderMapPtr, reactorPtr] (
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
            slaveProviderMapPtr->insert(std::make_pair(
                serviceID,
                dsb::bus::SlaveProviderClient(*reactorPtr, address, port)));
            DSB_LOG_TRACE(
                boost::format("Slave provider discovered: %s @ %s:%d")
                % serviceID % address % port);
        },
        // Slave provider port changed:
        [slaveProviderMapPtr, reactorPtr] (
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
            slaveProviderMapPtr->erase(serviceID);
            slaveProviderMapPtr->insert(std::make_pair(
                serviceID,
                dsb::bus::SlaveProviderClient(*reactorPtr, address, port)));
            DSB_LOG_TRACE(
                boost::format("Slave provider updated: %s @ %s:%d")
                % serviceID % address % port);
        },
        // Slave provider disappeared:
        [slaveProviderMapPtr] (
            const std::string& serviceType,
            const std::string& serviceID)
        {
            slaveProviderMapPtr->erase(serviceID);
            DSB_LOG_TRACE(boost::format("Slave provider disappeared: %s")
                % serviceID);
        });
}

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
    DSB_NOEXCEPT
{
    if (slaveProviders.empty()) {
        promise.set_value(std::vector<dsb::domain::Controller::SlaveType>{});
        return;
    }
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
                const dsb::net::SlaveLocator& locator,
                const std::string& errorMessage)
            {
                if (!ec) {
                    sharedPromise->set_value(locator);
                } else {
                    sharedPromise->set_exception(std::make_exception_ptr(
                        std::runtime_error(ec.message() + " (" + errorMessage + ")")));
                }
            },
            commTimeout);
    } catch (...) {
        sharedPromise->set_exception(std::current_exception());
    }
}


} // anonymous namespace
}} // namespace dsb::domain
