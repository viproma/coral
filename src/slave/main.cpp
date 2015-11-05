#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>

#include "boost/filesystem/path.hpp"
#include "boost/lexical_cast.hpp"
#include "zmq.hpp"

#include "dsb/execution/slave.hpp"
#include "dsb/fmi.hpp"


/*
Command line arguments:
 0  Program name (of course).
 1  Slave provider feedback endpoint (see below).
 2  FMU path
 3  The endpoint to which the slave should bind, in the URL format used by
    dsb::comm::P2PEndpoint.
 4  Communications timeout, i.e., the number of seconds of inactivity before
    the slave will shut itself down.
 5  The name of an output file, which will be written in CSV format.  This is
    optional, and if no file is specified, no file will be written.

The program will open a PUSH socket and connect it to the slave provider
feedback endpoint.  If anything goes wrong during the startup process, it will
send a 2-frame message on this channel, which contains (ERROR,<details>).
If all goes well, it will instead send a 2-frame message which contains
(OK,<bound endpoint>) just before the RunSlave() call.
*/
int main(int argc, const char** argv)
{
//    std::cin.ignore();
    // We use this socket to report back to the provider that started the slave.
    zmq::context_t context;
    std::unique_ptr<zmq::socket_t> feedbackSocket;
try {
    if (argc < 5) throw std::runtime_error("Missing command line arguments");

    const auto feedbackEndpoint = std::string(argv[1]);
    feedbackSocket = std::make_unique<zmq::socket_t>(context, ZMQ_PUSH);
    feedbackSocket->connect(feedbackEndpoint.c_str());

    const auto fmuPath = std::string(argv[2]);
    const auto bindpoint = std::string(argv[3]);
    const auto commTimeout = boost::chrono::seconds(std::atoi(argv[4]));

    std::ofstream csvOutput;
    if (argc > 5)
    {
        const auto outFile = std::string(argv[5]);
        csvOutput.open(outFile, std::ios_base::out | std::ios_base::trunc
#ifdef _WIN32
            , _SH_DENYWR // Don't let other processes/threads write to the file
#endif
        );
        if (!csvOutput) {
            throw std::runtime_error("Error opening output file for writing: " + outFile);
        }
        std::clog << "Output printed to: " << outFile << std::endl;
    }

    std::shared_ptr<dsb::execution::ISlaveInstance> fmiSlave =
        dsb::fmi::MakeSlaveInstance(fmuPath, csvOutput.is_open() ? &csvOutput : nullptr);
    auto slaveRunner = dsb::execution::SlaveRunner(fmiSlave, bindpoint, commTimeout);
    auto boundpoint = slaveRunner.BoundEndpoint();
    feedbackSocket->send("OK", 2, ZMQ_SNDMORE);
    feedbackSocket->send(boundpoint.data(), boundpoint.size());
    slaveRunner.Run();
    std::cout << "Slave shut down normally" << std::endl;

} catch (const std::runtime_error& e) {
    if (feedbackSocket) {
        feedbackSocket->send("ERROR", 5, ZMQ_SNDMORE);
        feedbackSocket->send(e.what(), std::strlen(e.what()));
    }
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
} catch (const std::exception& e) {
    auto msg = std::string("Internal error (") + e.what() + ')';
    if (feedbackSocket) {
        feedbackSocket->send("ERROR", 5, ZMQ_SNDMORE);
        feedbackSocket->send(msg.data(), msg.size());
    }
    std::cerr << msg << std::endl;
    return 2;
}
return 0;
}
