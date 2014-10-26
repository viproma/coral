#include <cassert>
#include <deque>
#include <exception>
#include <iostream>
#include <string>

#include "boost/chrono.hpp"
#include "boost/filesystem/path.hpp"
#include "boost/numeric/conversion/cast.hpp"
#include "boost/thread.hpp"
#include "zmq.hpp"

#include "fmilibcpp/Fmu.hpp"
#include "fmilibcpp/ImportContext.hpp"

#include "dsb/comm.hpp"
#include "dsb/error.hpp"
#include "dsb/protobuf.hpp"
#include "dsb/protocol/domain.hpp"
#include "dsb/util.hpp"

#include "domain.pb.h"


namespace dp = dsb::protocol::domain;


int main(int argc, const char** argv)
{
try {
    if (argc < 4) {
        const auto self = boost::filesystem::path(argv[0]).stem().string();
        std::cerr << "Usage: " << self << " <report> <info> <fmu path>\n"
                  << "  report   = Slave provider report endpoint (e.g. tcp://myhost:5432)\n"
                  << "  info     = Slave provider info endpoint (e.g. tcp://myhost:5432)\n"
                  << "  fmu path = Path to FMI1 FMU"
                  << std::endl;
        return 0;
    }
    const auto reportEndpoint = std::string(argv[1]);
    const auto infoEndpoint = std::string(argv[2]);
    const auto fmuPath = std::string(argv[3]);

    auto fmilibContext = fmilib::MakeImportContext();
    dsb::util::TempDir fmuUnzipDir;
    auto fmu = fmilibContext->Import(fmuPath, fmuUnzipDir.Path().string());

    auto context = zmq::context_t();
    auto report = zmq::socket_t(context, ZMQ_PUB);
    report.connect(reportEndpoint.c_str());

    const auto myId = dsb::util::RandomUUID();

    auto info = zmq::socket_t(context, ZMQ_DEALER);
    info.setsockopt(ZMQ_IDENTITY, myId.data(), myId.size());
    info.connect(infoEndpoint.c_str());

    namespace dp = dsb::protocol::domain;
    zmq::pollitem_t pollItem = { info, 0, ZMQ_POLLIN, 0 };

    namespace bc = boost::chrono;
    const auto HELLO_INTERVAL = bc::milliseconds(1000);
    auto nextHelloTime = bc::steady_clock::now() + HELLO_INTERVAL;
    for (;;) {
        const auto timeout = bc::duration_cast<bc::milliseconds>
                             (nextHelloTime - bc::steady_clock::now());
        zmq::poll(&pollItem, 1, boost::numeric_cast<long>(timeout.count()));
        if (pollItem.revents & ZMQ_POLLIN) {
            std::deque<zmq::message_t> msg;
            dsb::comm::Receive(info, msg);
            if (msg.size() < 4 || msg[0].size() > 0 || msg[2].size() > 0) {
                throw dsb::error::ProtocolViolationException("Wrong message format");
            }
            const auto header = dp::ParseHeader(msg[3]);
            switch (header.messageType) {
                case dp::MSG_GET_SLAVE_LIST: {
                    msg[3] = dp::CreateHeader(dp::MSG_SLAVE_LIST, header.protocol);
                    dsbproto::domain::SlaveTypeList stl;
                    auto st = stl.add_slave_type();
                    st->set_name(fmu->ModelName());
                    st->set_uuid(fmu->GUID());
                    st->set_description(fmu->Description());
                    st->set_author(fmu->Author());
                    st->set_version(fmu->ModelVersion());
                    msg.push_back(zmq::message_t());
                    dsb::protobuf::SerializeToFrame(stl, msg.back());
                    break; }
                default:
                    assert (false);
            }
            dsb::comm::Send(info, msg);
        }

        if (bc::steady_clock::now() >= nextHelloTime) {
            std::deque<zmq::message_t> msg;
            msg.push_back(dp::CreateHeader(dp::MSG_SLAVEPROVIDER_HELLO,
                                           dp::MAX_PROTOCOL_VERSION));
            msg.push_back(dsb::comm::ToFrame(myId));
            dsb::comm::Send(report, msg);
            nextHelloTime = bc::steady_clock::now() + HELLO_INTERVAL;
        }
    }
} catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
}
return 0;
}
