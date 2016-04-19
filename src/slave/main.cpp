#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>

#include "boost/filesystem.hpp"
#include "boost/lexical_cast.hpp"
#include "zmq.hpp"

#include "dsb/execution/logging_slave.hpp"
#include "dsb/execution/slave.hpp"
#include "dsb/fmi/fmu.hpp"
#include "dsb/fmi/importer.hpp"


/*
Command line arguments:
 0  Program name (of course).
 1  Slave provider feedback endpoint (see below).
 2  FMU path
 3  The endpoint to which the slave should bind, in the URL format used by
    dsb::comm::P2PEndpoint.
 4  Communications timeout, i.e., the number of seconds of inactivity before
    the slave will shut itself down.
 5  The name of an output directory, to which a file will be written in CSV
    format.  This is optional, and if no directory is specified, no file will
    be written.

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
    const auto commTimeout = std::chrono::seconds(std::atoi(argv[4]));

#ifdef _WIN32
    const char dirSep = '\\';
#else
    const char dirSep = '/';
#endif
    const auto outputDir = (argc > 5) ? std::string(argv[5]) + dirSep : std::string();

    const auto fmuCacheDir = boost::filesystem::temp_directory_path() / "dsb" / "cache";
    auto fmuImporter = dsb::fmi::Importer::Create(fmuCacheDir);
    auto fmu = fmuImporter->Import(fmuPath);
    auto fmiSlave = fmu->InstantiateSlave();
    std::shared_ptr<dsb::execution::ISlaveInstance> slave;
    if (outputDir.empty()) {
        slave = fmiSlave;
    } else {
        slave = std::make_shared<dsb::execution::LoggingSlaveInstance>(
            fmiSlave,
            outputDir);
    }

    auto slaveRunner = dsb::execution::SlaveRunner(slave, bindpoint, commTimeout);
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
