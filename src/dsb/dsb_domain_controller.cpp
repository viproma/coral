#ifdef _WIN32
#   define NOMINMAX
#endif

#include "dsb/domain/controller.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <deque>
#include <iostream>
#include <iterator>
#include <map>

#include "boost/chrono.hpp"
#include "boost/foreach.hpp"
#include "boost/lexical_cast.hpp"
#include "boost/numeric/conversion/cast.hpp"
#include "boost/range/algorithm/find_if.hpp"

#include "dsb/bus/domain_data.hpp"
#include "dsb/comm/messaging.hpp"
#include "dsb/comm/reactor.hpp"
#include "dsb/error.hpp"
#include "dsb/inproc_rpc.hpp"
#include "dsb/protobuf.hpp"
#include "dsb/protocol/domain.hpp"
#include "dsb/protocol/glue.hpp"
#include "dsb/util.hpp"

#include "domain.pb.h"
#include "inproc_rpc.pb.h"


namespace dp = dsb::protocol::domain;

namespace
{
    const auto SLAVEPROVIDER_TIMEOUT = boost::chrono::seconds(10);


    void HandleGetSlaveTypes(
        zmq::socket_t& rpcSocket,
        dsb::bus::DomainData& domainData)
    {
        // Create a mapping from slave type UUIDs to SlaveTypeInfo objects.
        // Each such object contains a list of the providers which offer this
        // slave type.
        std::map<std::string, dsbproto::inproc_rpc::SlaveTypeInfo> slaveTypesByUUID;
        BOOST_FOREACH (const auto& providerSlaveTypesPair,
                       domainData.SlaveTypesByProvider()) {
            BOOST_FOREACH (const auto& slaveType,
                           providerSlaveTypesPair.second.slave_type()) {
                auto it = slaveTypesByUUID.find(slaveType.uuid());
                if (it == slaveTypesByUUID.end()) {
                    it = slaveTypesByUUID.insert(
                        std::make_pair(std::string(slaveType.uuid()),
                                       dsbproto::inproc_rpc::SlaveTypeInfo())).first;
                    *it->second.mutable_slave_type_info() = slaveType;
                }
                *it->second.add_provider() = providerSlaveTypesPair.first;
            }
        }

        // Put them all into a SlaveTypeList object.
        dsbproto::inproc_rpc::SlaveTypeList slaveTypeList;
        BOOST_FOREACH (const auto& slaveType, slaveTypesByUUID) {
            *slaveTypeList.add_slave_type() = slaveType.second;
        }

        dsb::inproc_rpc::ReturnGetSlaveTypes(rpcSocket, slaveTypeList);
    }

    void HandleInstantiateSlave(
        zmq::socket_t& rpcSocket,
        zmq::socket_t& infoSocket,
        std::deque<zmq::message_t>& msg,
        const dsb::bus::DomainData& domainData)
    {
        std::string slaveTypeUUID;
        dsb::execution::Locator executionLocator;
        dsb::model::SlaveID slaveID = 0;
        std::string provider;
        dsb::inproc_rpc::UnmarshalInstantiateSlave(
            msg, slaveTypeUUID, executionLocator, slaveID, provider);

        if (provider.empty()) {
            // Search through all slave types for all providers, and use the
            // first one that matches the UUID.
            BOOST_FOREACH (const auto& providerSlaves, domainData.SlaveTypesByProvider()) {
                BOOST_FOREACH (const auto& slaveTypeInfo, providerSlaves.second.slave_type()) {
                    if (slaveTypeInfo.uuid() == slaveTypeUUID) {
                        provider = providerSlaves.first;
                        break;
                    }
                }
                if (!provider.empty()) break;
            }
            if (provider.empty()) {
                dsb::inproc_rpc::ThrowRuntimeError(rpcSocket,
                    "Slave type not offered by any slave provider: " + slaveTypeUUID);
                return;
            }
        } else {
            // Look for the specified provider in the domain data provider list.
            auto providerRng = domainData.SlaveTypesByProvider();
            const auto providerIt = boost::range::find_if(providerRng,
                [&](const std::pair<std::string, dsbproto::domain::SlaveTypeList>& a) { return a.first == provider; });

            if (providerIt == providerRng.end()) {
                dsb::inproc_rpc::ThrowRuntimeError(
                    rpcSocket, "Unknown slave provider name: " + provider);
                return;
            }

            // Provider was found; now check whether it provides slaves with
            // the given UUID.
            const auto slaveRng = providerIt->second.slave_type();
            const auto slaveIt = boost::range::find_if(slaveRng,
                [&](const dsbproto::domain::SlaveTypeInfo& a) { return a.uuid() == slaveTypeUUID; });
            if (slaveIt == slaveRng.end()) {
                dsb::inproc_rpc::ThrowRuntimeError(
                    rpcSocket,
                    "Slave provider \"" + provider + "\" does not provide slave: " + slaveTypeUUID);
                return;
            }
        }

        // Now we know both 'provider' and 'slaveTypeUUID' are valid.
        // Send the "instantiate slave" request on to the slave provider.
        std::deque<zmq::message_t> reqMsg;
        dsbproto::domain::InstantiateSlaveData data;
        data.set_slave_type_uuid(slaveTypeUUID);
        *data.mutable_execution_locator() = dsb::protocol::ToProto(executionLocator);
        data.set_slave_id(slaveID);
        dsb::protocol::domain::CreateAddressedMessage(
            reqMsg,
            provider,
            dp::MSG_INSTANTIATE_SLAVE,
            domainData.SlaveProviderProtocol(provider),
            data);
        dsb::comm::Send(infoSocket, reqMsg);

        std::deque<zmq::message_t> repMsg;
        if (!dsb::comm::Receive(infoSocket, repMsg, boost::chrono::milliseconds(2000))) {
            // TODO: Shouldn't use hardcoded timeout, and must handle the failure better.
            assert(!"Timeout waiting for reply from slave provider, SP possibly dead.");
            return;
        }
        if (repMsg.size() != 3 || repMsg[1].size() != 0) {
            throw dsb::error::ProtocolViolationException("Invalid reply format");
        }
        if (dsb::comm::ToString(repMsg[0]) != provider) {
            throw dsb::error::ProtocolViolationException(
                "Got reply from unexpected sender");
        }
        const auto replyHeader = dp::ParseHeader(repMsg[2]);
        switch (replyHeader.messageType) {
            case dp::MSG_INSTANTIATE_SLAVE_OK:
                dsb::inproc_rpc::ReturnSuccess(rpcSocket);
                break;
            case dp::MSG_INSTANTIATE_SLAVE_FAILED:
                dsb::inproc_rpc::ThrowRuntimeError(rpcSocket, "Instantiation failed");
                break;
            default:
                throw dsb::error::ProtocolViolationException(
                    "Invalid reply to INSTANTIATE_SLAVE");
        }
    }

    void HandleRPCCall(
        dsb::bus::DomainData& domainData,
        zmq::socket_t& rpcSocket,
        zmq::socket_t& infoSocket)
    {
        std::deque<zmq::message_t> msg;
        dsb::comm::Receive(rpcSocket, msg);
        switch (dsb::comm::DecodeRawDataFrame<dsb::inproc_rpc::CallType>(msg.front())) {
            case dsb::inproc_rpc::GET_SLAVE_TYPES_CALL:
                HandleGetSlaveTypes(rpcSocket, domainData);
                break;
            case dsb::inproc_rpc::INSTANTIATE_SLAVE_CALL:
                HandleInstantiateSlave(rpcSocket, infoSocket, msg, domainData);
                break;
            default:
                assert (!"Invalid RPC call");
                dsb::inproc_rpc::ThrowLogicError(rpcSocket, "Internal error");
                return;
        }
    }


    void HandleSlaveProviderHello(
        const boost::chrono::steady_clock::time_point recvTime,
        dsb::bus::DomainData& domainData,
        zmq::socket_t& infoSocket,
        const dp::Header& header,
        const zmq::message_t& body)
    {
        const auto providerId = dsb::comm::ToString(body);
        if (domainData.UpdateSlaveProvider(providerId, header.protocol, recvTime))
        {
            std::deque<zmq::message_t> msg;
            dp::CreateAddressedMessage(
                msg, providerId, dp::MSG_GET_SLAVE_LIST, header.protocol);
            dsb::comm::Send(infoSocket, msg);

            if (!dsb::comm::Receive(infoSocket, msg, boost::chrono::milliseconds(2000))) {
                // TODO: Shouldn't use hardcoded timeout, and must handle the failure better.
                assert(!"Timeout waiting for reply from slave provider, SP possibly dead.");
                return;
            }
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
            domainData.UpdateSlaveTypes(providerId, slaveTypeList);
        }
    }

    void HandleReportMsg(
        const boost::chrono::steady_clock::time_point recvTime,
        dsb::bus::DomainData& domainData,
        zmq::socket_t& reportSocket,
        zmq::socket_t& infoSocket)
    {
        std::deque<zmq::message_t> msg;
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


    void MessagingLoop(
        std::shared_ptr<zmq::context_t> context,
        std::shared_ptr<std::string> rpcEndpoint,
        std::shared_ptr<std::string> destroyEndpoint,
        std::shared_ptr<std::string> reportEndpoint,
        std::shared_ptr<std::string> infoEndpoint)
    {
        zmq::socket_t rpcSocket(*context, ZMQ_PAIR);
        rpcSocket.connect(rpcEndpoint->c_str());

        zmq::socket_t destroySocket(*context, ZMQ_PAIR);
        destroySocket.connect(destroyEndpoint->c_str());

        zmq::socket_t reportSocket(*context, ZMQ_SUB);
        dp::SubscribeToReports(reportSocket);
        reportSocket.connect(reportEndpoint->c_str());

        zmq::socket_t infoSocket(*context, ZMQ_REQ);
        infoSocket.connect(infoEndpoint->c_str());

        auto domainData = dsb::bus::DomainData(
            dp::MAX_PROTOCOL_VERSION, SLAVEPROVIDER_TIMEOUT);

        dsb::comm::Reactor reactor;
        reactor.AddSocket(destroySocket, [&](dsb::comm::Reactor& r, zmq::socket_t&) {
            r.Stop();
        });
        reactor.AddSocket(rpcSocket, [&](dsb::comm::Reactor& r, zmq::socket_t&) {
            HandleRPCCall(domainData, rpcSocket, infoSocket);
        });
        reactor.AddSocket(reportSocket, [&](dsb::comm::Reactor& r, zmq::socket_t&) {
            HandleReportMsg(boost::chrono::steady_clock::now(), domainData, reportSocket, infoSocket);
        });
        reactor.AddTimer(
            boost::chrono::milliseconds(SLAVEPROVIDER_TIMEOUT) / (domainData.SlaveProviderCount() + 1),
            -1,
            [&](dsb::comm::Reactor& r, int) {
                domainData.PurgeSlaveProviders(boost::chrono::steady_clock::now());
            });

        for (;;) {
            try { reactor.Run(); break; }
            catch (const dsb::error::ProtocolViolationException& e) {
                //TODO: Proper error reporting
                std::cerr << "Warning: Protocol violation: " << e.what() << std::endl;
            }
        }
    }
}


namespace dsb
{
namespace domain
{


Controller::Controller(const dsb::domain::Locator& locator)
    : m_context(std::make_shared<zmq::context_t>()),
      m_rpcSocket(*m_context, ZMQ_PAIR),
      m_destroySocket(*m_context, ZMQ_PAIR),
      m_active(true)
{
    const auto rpcEndpoint = std::make_shared<std::string>(
        "inproc://" + dsb::util::RandomUUID());
    m_rpcSocket.bind(rpcEndpoint->c_str());
    const auto destroyEndpoint = std::make_shared<std::string>(
        "inproc://" + dsb::util::RandomUUID());
    m_destroySocket.bind(destroyEndpoint->c_str());
    m_thread = boost::thread(MessagingLoop,
        m_context,
        rpcEndpoint,
        destroyEndpoint,
        std::make_shared<std::string>(locator.ReportMasterEndpoint()),
        std::make_shared<std::string>(locator.InfoMasterEndpoint()));
}



Controller::Controller(Controller&& other) DSB_NOEXCEPT
    : m_context(std::move(other.m_context)),
      m_rpcSocket(std::move(other.m_rpcSocket)),
      m_destroySocket(std::move(other.m_destroySocket)),
      m_active(dsb::util::MoveAndReplace(other.m_active, false)),
      m_thread(std::move(other.m_thread))
{
}


Controller& Controller::operator=(Controller&& other) DSB_NOEXCEPT
{
    m_rpcSocket     = std::move(other.m_rpcSocket);
    m_destroySocket = std::move(other.m_destroySocket);
    m_active        = dsb::util::MoveAndReplace(other.m_active, false);
    m_thread        = std::move(other.m_thread);
    // Move the context last, in case it overwrites and destroys another
    // context that is used by the above sockets.
    m_context       = std::move(other.m_context);
    return *this;
}


Controller::~Controller()
{
    if (m_active) {
        m_destroySocket.send("", 0);
    }
    m_thread.join();
}


std::vector<Controller::SlaveType> Controller::GetSlaveTypes()
{
    std::vector<Controller::SlaveType> ret;
    dsb::inproc_rpc::CallGetSlaveTypes(m_rpcSocket, ret);
    return ret;
}


void Controller::InstantiateSlave(
    const std::string& slaveTypeUUID,
    const dsb::execution::Locator& executionLocator,
    dsb::model::SlaveID slaveID,
    const std::string& provider)
{
    dsb::inproc_rpc::CallInstantiateSlave(
        m_rpcSocket,
        slaveTypeUUID,
        executionLocator,
        slaveID,
        provider);
}


}} // namespace
