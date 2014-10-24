#ifdef _WIN32
#   define NOMINMAX
#endif

#include "dsb/domain.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <deque>
#include <iostream> // For 
#include <iterator>
#include <map>

#include "boost/chrono.hpp"
#include "boost/foreach.hpp"
#include "boost/lexical_cast.hpp"
#include "boost/numeric/conversion/cast.hpp"
#include "boost/thread.hpp"

#include "dsb/bus/domain_data.hpp"
#include "dsb/comm.hpp"
#include "dsb/error.hpp"
#include "dsb/protobuf.hpp"
#include "dsb/protocol/domain.hpp"
#include "dsb/util.hpp"

#include "domain.pb.h"


namespace dp = dsb::protocol::domain;

namespace
{
    const auto SLAVEPROVIDER_TIMEOUT = boost::chrono::seconds(10);


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
        }
    }


    void MessagingLoop(
        std::shared_ptr<zmq::context_t> context,
        std::shared_ptr<std::string> rpcEndpoint,
        std::shared_ptr<std::string> reportEndpoint,
        std::shared_ptr<std::string> infoEndpoint)
    {
        zmq::socket_t rpcSocket(*context, ZMQ_PAIR);
        rpcSocket.connect(rpcEndpoint->c_str());

        zmq::socket_t reportSocket(*context, ZMQ_SUB);
        dp::SubscribeToReports(reportSocket);
        reportSocket.connect(reportEndpoint->c_str());

        zmq::socket_t infoSocket(*context, ZMQ_REQ);
        infoSocket.connect(infoEndpoint->c_str());

        auto domainData = dsb::bus::DomainData(
            dp::MAX_PROTOCOL_VERSION, SLAVEPROVIDER_TIMEOUT);

        const size_t SOCKET_COUNT = 3;
        zmq::pollitem_t pollItems[SOCKET_COUNT] = {
            { rpcSocket,    0, ZMQ_POLLIN, 0 },
            { reportSocket, 0, ZMQ_POLLIN, 0 },
            { infoSocket,   0, ZMQ_POLLIN, 0 }
        };
        for (;;) {
            const auto pollTimeout = boost::numeric_cast<long>(
                boost::chrono::milliseconds(SLAVEPROVIDER_TIMEOUT).count()
                / (domainData.SlaveProviderCount() + 1));
            zmq::poll(pollItems, SOCKET_COUNT, pollTimeout);
            const auto recvTime = boost::chrono::steady_clock::now();

            try {
                if (pollItems[1].revents & ZMQ_POLLIN) {
                    HandleReportMsg(recvTime, domainData, reportSocket, infoSocket);
                }
            } catch (const dsb::error::ProtocolViolationException& e) {
                //TODO: Proper error reporting
                std::cerr << "Warning: Protocol violation: " << e.what() << std::endl;
            }
            domainData.PurgeSlaveProviders(recvTime);
        }
    }
}


namespace dsb
{
namespace domain
{


Controller::Controller(
    std::shared_ptr<zmq::context_t> context,
    const std::string& reportEndpoint,
    const std::string& infoEndpoint)
    : m_rpcSocket(*context, ZMQ_PAIR)
{
    auto rpcEndpoint =
        std::make_shared<std::string>("inproc://" + dsb::util::RandomUUID());
    m_rpcSocket.bind(rpcEndpoint->c_str());
    boost::thread(MessagingLoop,
        context,
        rpcEndpoint,
        std::make_shared<std::string>(reportEndpoint),
        std::make_shared<std::string>(infoEndpoint));
}



Controller::Controller(Controller&& other)
    : m_rpcSocket(std::move(other.m_rpcSocket))
{
}


Controller& Controller::operator=(Controller&& other)
{
    m_rpcSocket = std::move(other.m_rpcSocket);
    return *this;
}

}} // namespace