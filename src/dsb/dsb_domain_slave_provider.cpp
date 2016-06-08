#include "dsb/domain/slave_provider.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>

#include "boost/numeric/conversion/cast.hpp"
#include "zmq.hpp"

#include "dsb/comm/messaging.hpp"
#include "dsb/comm/util.hpp"
#include "dsb/error.hpp"
#include "dsb/protocol/glue.hpp"
#include "dsb/protobuf.hpp"
#include "dsb/protocol/domain.hpp"
#include "dsb/util.hpp"
#include "domain.pb.h"

namespace dp = dsb::protocol::domain;


namespace dsb
{
namespace domain
{


namespace
{
    // Defined below MessagingLoop();
    void HandleRequest(
        std::vector<zmq::message_t>& msg,
        const std::vector<std::unique_ptr<dsb::domain::ISlaveType>>& slaveTypes);

    // Slave provider messaging loop
    void MessagingLoop(
        std::shared_ptr<zmq::socket_t> killSocket,
        std::shared_ptr<zmq::socket_t> controlSocket,
        std::shared_ptr<zmq::socket_t> beaconSocket,
        std::shared_ptr<std::vector<std::unique_ptr<dsb::domain::ISlaveType>>> slaveTypesPtr,
        std::function<void(std::exception_ptr)> exceptionHandler)
    {
        auto& slaveTypes = *slaveTypesPtr;
        try {
            char slaveProviderIDBuffer[255];
            std::size_t slaveProviderIDSize = 255;
            controlSocket->getsockopt(
                ZMQ_IDENTITY, slaveProviderIDBuffer, &slaveProviderIDSize);
            const auto slaveProviderID =
                std::string(slaveProviderIDBuffer, slaveProviderIDSize);

            const std::size_t POLLITEM_COUNT = 2;
            zmq::pollitem_t pollItems[POLLITEM_COUNT] = {
                { static_cast<void*>(*killSocket),    0, ZMQ_POLLIN, 0 },
                { static_cast<void*>(*controlSocket), 0, ZMQ_POLLIN, 0 }
            };

            namespace bc = std::chrono;
            const auto HELLO_INTERVAL = bc::milliseconds(1000);
            auto nextHelloTime = bc::steady_clock::now() + HELLO_INTERVAL;
            for (;;) {
                const auto timeout = bc::duration_cast<bc::milliseconds>
                                     (nextHelloTime - bc::steady_clock::now());
                zmq::poll(pollItems, POLLITEM_COUNT, boost::numeric_cast<long>(timeout.count()));

                if (pollItems[0].revents & ZMQ_POLLIN) {
                    break;
                }

                if (pollItems[1].revents & ZMQ_POLLIN) {
                    std::vector<zmq::message_t> msg;
                    dsb::comm::Receive(*controlSocket, msg);
                    // TODO: Resolve this bug. See:
                    // http://stackoverflow.com/questions/23281405/how-to-use-a-stdvectorunique-ptrt-as-default-parameter
                    HandleRequest(msg, slaveTypes);
                    dsb::comm::Send(*controlSocket, msg);
                }

                if (bc::steady_clock::now() >= nextHelloTime) {
                    std::vector<zmq::message_t> msg;
                    msg.push_back(dp::CreateHeader(dp::MSG_SLAVEPROVIDER_HELLO,
                                                   dp::MAX_PROTOCOL_VERSION));
                    msg.push_back(dsb::comm::ToFrame(slaveProviderID));
                    dsb::comm::Send(*beaconSocket, msg);
                    nextHelloTime = bc::steady_clock::now() + HELLO_INTERVAL;
                }
            }
        } catch (...) {
            if (exceptionHandler) {
                exceptionHandler(std::current_exception());
            } else {
                throw;
            }
        }
    }

    void HandleRequest(
        std::vector<zmq::message_t>& msg,
        const std::vector<std::unique_ptr<dsb::domain::ISlaveType>>& slaveTypes)
    {
        if (msg.size() < 4 || msg[0].size() > 0 || msg[2].size() > 0) {
            throw dsb::error::ProtocolViolationException("Wrong message format");
        }
        namespace dp = dsb::protocol::domain;
        const auto header = dp::ParseHeader(msg[3]);
        switch (header.messageType) {
            case dp::MSG_GET_SLAVE_LIST: {
                msg[3] = dp::CreateHeader(dp::MSG_SLAVE_LIST, header.protocol);
                dsbproto::domain::SlaveTypeList stList;
                for (const auto& slaveType : slaveTypes) {
                    auto stInfo = stList.add_slave_type();
                    *stInfo->mutable_description() =
                        dsb::protocol::ToProto(slaveType->Description());
                }
                msg.push_back(zmq::message_t());
                dsb::protobuf::SerializeToFrame(stList, msg.back());
                break; }

            case dp::MSG_INSTANTIATE_SLAVE: {
                if (msg.size() != 5) {
                    throw dsb::error::ProtocolViolationException(
                        "Wrong INSTANTIATE_SLAVE message format");
                }
                dsbproto::domain::InstantiateSlaveData data;
                dsb::protobuf::ParseFromFrame(msg[4], data);
                const auto stIt = std::find_if(
                    slaveTypes.begin(),
                    slaveTypes.end(),
                    [&](const std::unique_ptr<dsb::domain::ISlaveType>& s) {
                        return s->Description().UUID() == data.slave_type_uuid();
                    });

                const auto instantiationTimeout = std::chrono::milliseconds(data.timeout_ms());
                dsb::net::SlaveLocator slaveLocator;
                if (stIt != slaveTypes.end()
                    && (*stIt)->Instantiate(instantiationTimeout, slaveLocator))
                {
                    msg[3] = dp::CreateHeader(
                        dp::MSG_INSTANTIATE_SLAVE_OK, header.protocol);
                    dsbproto::net::SlaveLocator slaveLocPb;
                    slaveLocPb.set_endpoint(slaveLocator.Endpoint());
                    slaveLocPb.set_identity(slaveLocator.Identity());
                    dsb::protobuf::SerializeToFrame(slaveLocPb, msg[4]);
                } else {
                    msg[3] = dp::CreateHeader(
                        dp::MSG_INSTANTIATE_SLAVE_FAILED,
                        header.protocol);
                    dsbproto::domain::Error errorPb;
                    errorPb.set_message((*stIt)->InstantiationFailureDescription());
                    dsb::protobuf::SerializeToFrame(errorPb, msg[4]);
                }
                break; }

            default:
                assert (false);
        }
    }
}


SlaveProvider::SlaveProvider(
    const dsb::net::DomainLocator& domainLocator,
    std::vector<std::unique_ptr<dsb::domain::ISlaveType>>&& slaveTypes,
    std::function<void(std::exception_ptr)> exceptionHandler)
{
    // We set up all the sockets in the "foreground" thread so that any
    // exceptions (e.g. due to invalid endpoints) are thrown there.
    const auto killEndpoint = "inproc://" + dsb::util::RandomUUID();
    m_killSocket = std::make_unique<zmq::socket_t>(
        dsb::comm::GlobalContext(), ZMQ_PAIR);
    m_killSocket->bind(killEndpoint);
    auto otherKillSocket = std::make_shared<zmq::socket_t>(
        dsb::comm::GlobalContext(), ZMQ_PAIR);
    otherKillSocket->connect(killEndpoint);

    const auto slaveProviderID = dsb::util::RandomUUID();
    auto controlSocket = std::make_shared<zmq::socket_t>(
        dsb::comm::GlobalContext(), ZMQ_DEALER);
    controlSocket->setsockopt(
        ZMQ_IDENTITY, slaveProviderID.data(), slaveProviderID.size());
    controlSocket->connect(domainLocator.InfoSlavePEndpoint());

    auto beaconSocket = std::make_shared<zmq::socket_t>(
        dsb::comm::GlobalContext(), ZMQ_PUB);
    beaconSocket->connect(domainLocator.ReportSlavePEndpoint());

    m_thread = std::thread{&MessagingLoop,
        otherKillSocket,
        controlSocket,
        beaconSocket,
        std::make_shared<std::vector<std::unique_ptr<dsb::domain::ISlaveType>>>(std::move(slaveTypes)),
        exceptionHandler};
}


// This is just here so we can declare m_killSocket to be a std::unique_ptr
// to an incomplete type in the header.
SlaveProvider::~SlaveProvider() DSB_NOEXCEPT { }


void SlaveProvider::Stop()
{
    if (m_thread.joinable()) {
        char dummy = 0;
        m_killSocket->send(&dummy, 0, ZMQ_DONTWAIT);
        m_killSocket->recv(&dummy, 1, ZMQ_DONTWAIT);
        m_thread.join();
    }
}


}} // namespace
