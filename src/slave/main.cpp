#include <iostream>
#include <utility>

#include "boost/filesystem/path.hpp"
#include "boost/lexical_cast.hpp"
#include "zmq.hpp"

#include "dsb/bus/slave_agent.hpp"
#include "dsb/comm.hpp"
#include "dsb/compat_helpers.hpp"
#include "dsb/control.hpp"
#include "dsb/slave/fmi.hpp"
#include "dsb/util.hpp"


int main(int argc, const char** argv)
{
try {
    if (argc < 6) {
        const auto self = boost::filesystem::path(argv[0]).stem().string();
        std::cerr << "Usage: " << self << " <id> <control> <data pub> <data sub> <fmu path> [output file]\n"
                  << "  id          = a number in the range 1 - 65535\n"
                  << "  control     = Control socket endpoint (e.g. tcp://myhost:5432)\n"
                  << "  data pub    = Publisher socket endpoint\n"
                  << "  data sub    = Subscriber socket endpoint\n"
                  << "  fmu path    = Path to FMI1 FMU\n"
                  << "  output file = Name of output file to write.\n"
                  << "\n"
                  << "The output file will be written in CSV format.  If no output file name is given, no output will be written."
                  << std::endl;
        return 0;
    }
    const auto id = boost::lexical_cast<uint16_t>(argv[1]);
    if (id == 0) {
        std::cerr << "0 is not a valid slave ID" << std::endl;
        return 1;
    }
    const auto controlEndpoint = std::string(argv[2]);
    const auto dataPubEndpoint = std::string(argv[3]);
    const auto dataSubEndpoint = std::string(argv[4]);
    const auto fmuPath = std::string(argv[5]);

    std::ofstream csvOutput;
    if (argc > 6)
    {
        csvOutput.open(argv[6]);
        if (!csvOutput) {
            std::cerr << "Error opening output file for writing: " << argv[6] << std::endl;
            return 1;
        }
    }

    auto context = zmq::context_t();
    auto control = zmq::socket_t(context, ZMQ_REQ);

    // Encode `id` into a 2-byte buffer, and use it as the ZMQ socket identity.
    char idBuffer[2];
    dsb::util::EncodeUint16(id, idBuffer);
    control.setsockopt(ZMQ_IDENTITY, idBuffer, 2);

    control.connect(controlEndpoint.c_str());
    auto dataPub = zmq::socket_t(context, ZMQ_PUB);
    dataPub.connect(dataPubEndpoint.c_str());
    auto dataSub = zmq::socket_t(context, ZMQ_SUB);
    dataSub.connect(dataSubEndpoint.c_str());

    dsb::bus::SlaveAgent slave(
        id,
        std::move(dataSub),
        std::move(dataPub),
        std::make_unique<dsb::slave::FmiSlaveInstance>(
            fmuPath, csvOutput.is_open() ? &csvOutput : nullptr));
    std::deque<zmq::message_t> msg;
    slave.Start(msg);
    for (;;) {
        dsb::comm::Send(control, msg);
        dsb::comm::Receive(control, msg);
        slave.RequestReply(msg);
    }
} catch (const dsb::bus::Shutdown& e) {
    std::cerr << "Shutdown: " << e.what() << std::endl;
} catch (const dsb::control::RemoteErrorException& e) {
    std::cerr << "Received ERROR message: " << e.what() << std::endl;
    return 2;
}
return 0;
}
