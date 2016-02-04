#ifdef _WIN32
#   define NOMINMAX
#endif

#include "dsb/domain/controller.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <iterator>
#include <map>
#include <vector>

#include "boost/lexical_cast.hpp"
#include "boost/range/algorithm/find_if.hpp"

#include "zmq.hpp"

#include "dsb/async.hpp"
#include "dsb/bus/domain_data.hpp"
#include "dsb/comm/messaging.hpp"
#include "dsb/comm/reactor.hpp"
#include "dsb/comm/util.hpp"
#include "dsb/error.hpp"
#include "dsb/log.hpp"
#include "dsb/protobuf.hpp"
#include "dsb/protocol/domain.hpp"
#include "dsb/protocol/glue.hpp"
#include "dsb/util.hpp"

#include "domain.pb.h"


namespace dp = dsb::protocol::domain;

namespace dsb
{
namespace domain
{


namespace
{
    const auto SLAVEPROVIDER_TIMEOUT = std::chrono::seconds(10000);


    std::vector<dsb::domain::Controller::SlaveType> HandleGetSlaveTypes(
        dsb::bus::DomainData& domainData)
    {
        std::vector<dsb::domain::Controller::SlaveType> slaveTypes;
        std::map<std::string, std::size_t> slaveTypeIndices;
        for (const auto& providerSlaveTypesPair : domainData.SlaveTypesByProvider()) {
            for (const auto& slaveTypeDesc : providerSlaveTypesPair.second) {
                auto it = slaveTypeIndices.find(slaveTypeDesc.uuid());
                if (it == slaveTypeIndices.end()) {
                    slaveTypes.emplace_back();
                    slaveTypes.back().description =
                        dsb::protocol::FromProto(slaveTypeDesc);
                    it = slaveTypeIndices.insert(
                            std::make_pair(
                                std::string(slaveTypeDesc.uuid()),
                                slaveTypes.size() - 1)
                        ).first;
                }
                slaveTypes[it->second].providers.push_back(
                    providerSlaveTypesPair.first);
            }
        }
        return slaveTypes;
    }


    dsb::net::SlaveLocator HandleInstantiateSlave(
        const std::string& slaveTypeUUID,
        std::chrono::milliseconds timeout,
        std::string provider,
        zmq::socket_t& infoSocket,
        const std::string& p2pBrokerAddress,
        const dsb::bus::DomainData& domainData)
    {
        if (provider.empty()) {
            // Search through all slave types for all providers, and use the
            // first one that matches the UUID.
            for (const auto& providerSlaves : domainData.SlaveTypesByProvider()) {
                for (const auto& slaveTypeDesc : providerSlaves.second) {
                    if (slaveTypeDesc.uuid() == slaveTypeUUID) {
                        provider = providerSlaves.first;
                        break;
                    }
                }
                if (!provider.empty()) break;
            }
            if (provider.empty()) {
                throw std::runtime_error(
                    "Slave type not offered by any slave provider: "
                    + slaveTypeUUID);
            }
        } else {
            // Look for the specified provider in the domain data provider list.
            auto providerRng = domainData.SlaveTypesByProvider();
            const auto providerIt = boost::range::find_if(providerRng,
                [&](const std::pair<std::string, std::vector<dsbproto::model::SlaveTypeDescription>>& a) {
                    return a.first == provider;
                });

            if (providerIt == providerRng.end()) {
                throw std::runtime_error(
                    "Unknown slave provider name: " + provider);
            }

            // Provider was found; now check whether it provides slaves with
            // the given UUID.
            const auto& slaveRng = providerIt->second;
            const auto slaveIt = boost::range::find_if(slaveRng,
                [&](const dsbproto::model::SlaveTypeDescription& a) {
                    return a.uuid() == slaveTypeUUID;
                });
            if (slaveIt == slaveRng.end()) {
                throw std::runtime_error(
                    "Slave provider \"" + provider
                    + "\" does not provide slave: " + slaveTypeUUID);
            }
        }

        // Now we know both 'provider' and 'slaveTypeUUID' are valid.
        // Send the "instantiate slave" request on to the slave provider.
        std::vector<zmq::message_t> reqMsg;
        dsbproto::domain::InstantiateSlaveData data;
        data.set_slave_type_uuid(slaveTypeUUID);
        data.set_timeout_ms(boost::numeric_cast<std::int32_t>(timeout.count()));
        dsb::protocol::domain::CreateAddressedMessage(
            reqMsg,
            provider,
            dp::MSG_INSTANTIATE_SLAVE,
            domainData.SlaveProviderProtocol(provider),
            data);
        dsb::comm::Send(infoSocket, reqMsg);

        std::vector<zmq::message_t> repMsg;
        if (!dsb::comm::WaitForIncoming(infoSocket, 2*timeout)) {
            // We double the timeout here, since the same timeout is used
            // at the other end, and we don't want to cancel the operation
            // prematurely at this end.
            throw std::runtime_error(
                "Instantiation failed: No reply from slave provider");
        }
        dsb::comm::Receive(infoSocket, repMsg);
        if (repMsg.size() != 4 || repMsg[1].size() != 0) {
            throw dsb::error::ProtocolViolationException("Invalid reply format");
        }
        if (dsb::comm::ToString(repMsg[0]) != provider) {
            throw dsb::error::ProtocolViolationException(
                "Got reply from unexpected sender");
        }
        const auto replyHeader = dp::ParseHeader(repMsg[2]);
        switch (replyHeader.messageType) {
            case dp::MSG_INSTANTIATE_SLAVE_OK: {
                dsbproto::net::SlaveLocator slaveLocPb;
                dsb::protobuf::ParseFromFrame(repMsg[3], slaveLocPb);
                if (slaveLocPb.endpoint().empty()) {
                    slaveLocPb.set_endpoint(p2pBrokerAddress);
                } else if (slaveLocPb.endpoint()[0] == ':') {
                    throw std::logic_error("Unsupported feature used");
                }
                return dsb::net::SlaveLocator{
                    slaveLocPb.endpoint(),
                    slaveLocPb.identity()}; }
            case dp::MSG_INSTANTIATE_SLAVE_FAILED: {
                dsbproto::domain::Error errorPb;
                dsb::protobuf::ParseFromFrame(repMsg[3], errorPb);
                throw std::runtime_error(
                    "Instantiation failed: " + errorPb.message()); }
            default:
                throw dsb::error::ProtocolViolationException(
                    "Invalid reply to INSTANTIATE_SLAVE");
        }
    }


    void HandleSlaveProviderHello(
        const std::chrono::steady_clock::time_point recvTime,
        dsb::bus::DomainData& domainData,
        zmq::socket_t& infoSocket,
        const dp::Header& header,
        const zmq::message_t& body)
    {
        const auto providerId = dsb::comm::ToString(body);
        if (domainData.UpdateSlaveProvider(providerId, header.protocol, recvTime))
        {
            std::vector<zmq::message_t> msg;
            dp::CreateAddressedMessage(
                msg, providerId, dp::MSG_GET_SLAVE_LIST, header.protocol);
            dsb::comm::Send(infoSocket, msg);

            if (!dsb::comm::WaitForIncoming(infoSocket, std::chrono::seconds(10))) {
                // TODO: Shouldn't use hardcoded timeout, and must handle the failure better.
                assert(!"Timeout waiting for reply from slave provider, SP possibly dead.");
                return;
            }
            dsb::comm::Receive(infoSocket, msg);
            if (msg.size() != 4 || msg[1].size() != 0) {
                throw dsb::error::ProtocolViolationException("Invalid reply format");
            }
            if (dsb::comm::ToString(msg[0]) != providerId) {
                throw dsb::error::ProtocolViolationException(
                    "Got reply from unexpected sender");
            }
            const auto replyHeader = dp::ParseHeader(msg[2]);
            if (replyHeader.messageType != dp::MSG_SLAVE_LIST) {
                throw dsb::error::ProtocolViolationException(
                    "Expected SLAVE_LIST, got: "
                    + boost::lexical_cast<std::string>(replyHeader.messageType));
            }
            dsbproto::domain::SlaveTypeList slaveTypeList;
            dsb::protobuf::ParseFromFrame(msg[3], slaveTypeList);
            std::vector<dsbproto::model::SlaveTypeDescription> slaveTypeVector;
            for (const auto& st : slaveTypeList.slave_type()) {
                slaveTypeVector.push_back(st.description());
            }
            domainData.UpdateSlaveTypes(providerId, std::move(slaveTypeVector));
        }
    }


    void HandleReportMsg(
        const std::chrono::steady_clock::time_point recvTime,
        dsb::bus::DomainData& domainData,
        zmq::socket_t& reportSocket,
        zmq::socket_t& infoSocket)
    {
        std::vector<zmq::message_t> msg;
        dsb::comm::Receive(reportSocket, msg);
        const auto header = dp::ParseHeader(msg.at(0));
        assert (header.protocol == dp::MAX_PROTOCOL_VERSION);
        switch (header.messageType) {
            case dp::MSG_SLAVEPROVIDER_HELLO:
                if (msg.size() < 2) {
                    throw dsb::error::ProtocolViolationException(
                        "Wrong number of frames in SLAVEPROVIDER_HELLO");
                }
                HandleSlaveProviderHello(
                    recvTime,
                    domainData,
                    infoSocket,
                    header,
                    msg.at(1));
                break;
            case dp::MSG_UPDATE_AVAILABLE:
                assert (!"We do not handle UPDATE messages yet");
                break;
            default:
                assert (!"Unknown report message received");
        }
    }
} // anonymous namespace


class Controller::Private
{
public:
    explicit Private(const dsb::net::DomainLocator& locator)
        : m_reportSocket{dsb::comm::GlobalContext(), ZMQ_SUB}
        , m_infoSocket{dsb::comm::GlobalContext(), ZMQ_REQ}
        , m_domainData{dp::MAX_PROTOCOL_VERSION, SLAVEPROVIDER_TIMEOUT}
        , m_domainLocator{std::make_shared<dsb::net::DomainLocator>(locator)}
        , m_thread{[this] (dsb::comm::Reactor& reactor)
            {
                dp::SubscribeToReports(m_reportSocket);
                m_reportSocket.connect(
                    m_domainLocator->ReportMasterEndpoint());
                m_infoSocket.connect(
                    m_domainLocator->InfoMasterEndpoint());

                reactor.AddSocket(
                    m_reportSocket,
                    [&] (dsb::comm::Reactor& r, zmq::socket_t&)
                    {
                        HandleReportMsg(
                            std::chrono::steady_clock::now(),
                            m_domainData,
                            m_reportSocket,
                            m_infoSocket);
                    });
                reactor.AddTimer(
                    std::chrono::milliseconds(SLAVEPROVIDER_TIMEOUT)
                        / (m_domainData.SlaveProviderCount() + 1),
                    -1,
                    [&] (dsb::comm::Reactor& r, int)
                    {
                        m_domainData.PurgeSlaveProviders(
                            std::chrono::steady_clock::now());
                    });
            }}
    {
    }

    ~Private() DSB_NOEXCEPT
    {
    }

    Private(const Private&) = delete;
    Private& operator=(const Private&) = delete;
    Private(Private&& other) = delete;
    Private& operator=(Private&& other) = delete;


    std::vector<SlaveType> GetSlaveTypes()
    {
        return m_thread.Execute<std::vector<SlaveType>>(
            [this] (dsb::comm::Reactor&, std::promise<std::vector<SlaveType>> promise)
            {
                try {
                    promise.set_value(HandleGetSlaveTypes(m_domainData));
                } catch (...) {
                    promise.set_exception(std::current_exception());
                }
            }
        ).get();
    }

    dsb::net::SlaveLocator InstantiateSlave(
        const std::string& slaveTypeUUID,
        std::chrono::milliseconds timeout,
        const std::string& provider)
    {
        // Note: It is safe to capture by reference in the lambda because
        // the present thread is blocked waiting for the operation to complete.
        return m_thread.Execute<dsb::net::SlaveLocator>(
            [&] (dsb::comm::Reactor&, std::promise<dsb::net::SlaveLocator> promise)
            {
                try {
                    promise.set_value(HandleInstantiateSlave(
                        slaveTypeUUID,
                        timeout,
                        provider,
                        m_infoSocket,
                        m_domainLocator->InfoMasterEndpoint(),
                        m_domainData));
                } catch (...) {
                    promise.set_exception(std::current_exception());
                }
            }
        ).get();
    }

private:
    // For use in background thread
    zmq::socket_t m_reportSocket;
    zmq::socket_t m_infoSocket;
    dsb::bus::DomainData m_domainData;
    std::shared_ptr<dsb::net::DomainLocator> m_domainLocator;

    // Background thread
    dsb::async::CommThread m_thread;
};


Controller::Controller(const dsb::net::DomainLocator& locator)
    : m_private{std::make_unique<Private>(locator)}
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


std::vector<Controller::SlaveType> Controller::GetSlaveTypes()
{
    return m_private->GetSlaveTypes();
}


dsb::net::SlaveLocator Controller::InstantiateSlave(
    const std::string& slaveTypeUUID,
    std::chrono::milliseconds timeout,
    const std::string& provider)
{
    return m_private->InstantiateSlave(slaveTypeUUID, timeout, provider);
}


}} // namespace
