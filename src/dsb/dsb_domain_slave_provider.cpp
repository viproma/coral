#include "dsb/domain/slave_provider.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <thread>
#include <vector>

#include "boost/foreach.hpp"
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


void dsb::domain::SlaveProvider(
    const dsb::net::DomainLocator& domainLocator,
    const std::vector<dsb::domain::ISlaveType*>& slaveTypes)
{
    auto report = zmq::socket_t(dsb::comm::GlobalContext(), ZMQ_PUB);
    report.connect(domainLocator.ReportSlavePEndpoint().c_str());

    const auto myId = dsb::util::RandomUUID();

    auto info = zmq::socket_t(dsb::comm::GlobalContext(), ZMQ_DEALER);
    info.setsockopt(ZMQ_IDENTITY, myId.data(), myId.size());
    info.connect(domainLocator.InfoSlavePEndpoint().c_str());

    namespace dp = dsb::protocol::domain;
    zmq::pollitem_t pollItem = { static_cast<void*>(info), 0, ZMQ_POLLIN, 0 };

    namespace bc = std::chrono;
    const auto HELLO_INTERVAL = bc::milliseconds(1000);
    auto nextHelloTime = bc::steady_clock::now() + HELLO_INTERVAL;
    for (;;) {
        const auto timeout = bc::duration_cast<bc::milliseconds>
                             (nextHelloTime - bc::steady_clock::now());
        zmq::poll(&pollItem, 1, boost::numeric_cast<long>(timeout.count()));
        if (pollItem.revents & ZMQ_POLLIN) {
            std::vector<zmq::message_t> msg;
            dsb::comm::Receive(info, msg);
            if (msg.size() < 4 || msg[0].size() > 0 || msg[2].size() > 0) {
                throw dsb::error::ProtocolViolationException("Wrong message format");
            }
            const auto header = dp::ParseHeader(msg[3]);
            switch (header.messageType) {
                case dp::MSG_GET_SLAVE_LIST: {
                    msg[3] = dp::CreateHeader(dp::MSG_SLAVE_LIST, header.protocol);
                    dsbproto::domain::SlaveTypeList stl;
                    BOOST_FOREACH (const auto slaveType, slaveTypes) {
                        auto st = stl.add_slave_type();
                        st->set_name(slaveType->Name());
                        st->set_uuid(slaveType->Uuid());
                        st->set_description(slaveType->Description());
                        st->set_author(slaveType->Author());
                        st->set_version(slaveType->Version());
                        for (size_t i = 0; i < slaveType->VariableCount(); ++i) {
                            *st->add_variable() = dsb::protocol::ToProto(slaveType->Variable(i));
                        }
                    }
                    msg.push_back(zmq::message_t());
                    dsb::protobuf::SerializeToFrame(stl, msg.back());
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
                        [&](const dsb::domain::ISlaveType* s)
                            { return s->Uuid() == data.slave_type_uuid(); });

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
            dsb::comm::Send(info, msg);
        }

        if (bc::steady_clock::now() >= nextHelloTime) {
            std::vector<zmq::message_t> msg;
            msg.push_back(dp::CreateHeader(dp::MSG_SLAVEPROVIDER_HELLO,
                                           dp::MAX_PROTOCOL_VERSION));
            msg.push_back(dsb::comm::ToFrame(myId));
            dsb::comm::Send(report, msg);
            nextHelloTime = bc::steady_clock::now() + HELLO_INTERVAL;
        }
    }
}
