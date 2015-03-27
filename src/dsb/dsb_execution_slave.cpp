#include "dsb/execution/slave.hpp"

#include <deque>
#include <utility>

#include "zmq.hpp"

#include "dsb/bus/slave_agent.hpp"
#include "dsb/comm/messaging.hpp"
#include "dsb/comm/util.hpp"
#include "dsb/util.hpp"


namespace dsb
{
namespace execution
{


void RunSlave(dsb::model::SlaveID id,
    const std::string& controlEndpoint,
    const std::string& dataPubEndpoint,
    const std::string& dataSubEndpoint,
    ISlaveInstance& slaveInstance,
    boost::chrono::seconds commTimeout)
{
    auto control = zmq::socket_t(dsb::comm::GlobalContext(), ZMQ_REQ);

    // Encode `id` into a 2-byte buffer, and use it as the ZMQ socket identity.
    char idBuffer[2];
    dsb::util::EncodeUint16(id, idBuffer);
    control.setsockopt(ZMQ_IDENTITY, idBuffer, 2);

    control.connect(controlEndpoint.c_str());
    auto dataPub = zmq::socket_t(dsb::comm::GlobalContext(), ZMQ_PUB);
    dataPub.connect(dataPubEndpoint.c_str());
    auto dataSub = zmq::socket_t(dsb::comm::GlobalContext(), ZMQ_SUB);
    dataSub.connect(dataSubEndpoint.c_str());

    dsb::bus::SlaveAgent slave(
        id,
        std::move(dataSub),
        std::move(dataPub),
        slaveInstance);
    std::deque<zmq::message_t> msg;
    slave.Start(msg);
    for (;;) {
        dsb::comm::Send(control, msg);
        if (!dsb::comm::Receive(control, msg, commTimeout)) {
            throw TimeoutException(commTimeout);
        }
        try {
            slave.RequestReply(msg);
        } catch (const dsb::bus::Shutdown&) {
            // TODO: Handle slave shutdown via other means?
            return;
        }
    }
};


}} // namespace