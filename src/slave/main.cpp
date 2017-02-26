/*
Copyright 2013-2017, SINTEF Ocean and the Coral contributors.
This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifdef _WIN32
#   include <process.h>
#else
#   include <unistd.h>
#endif

#include <memory>
#include <stdexcept>
#include <string>

#include "boost/filesystem.hpp"
#include "zmq.hpp"

#include "coral/fmi/fmu.hpp"
#include "coral/fmi/importer.hpp"
#include "coral/log.hpp"
#include "coral/net/zmqx.hpp"
#include "coral/slave.hpp"


/*
Command line arguments:
 0  Program name (of course).
 1  Slave provider feedback endpoint (see below).
 2  FMU path
 3  The network interface to which the slave should bind, or "*" for all
    interfaces.
 4  Master-to-slave communications timeout, i.e., the number of seconds
    without any command from the master before the slave will shut itself down.
 5  The name of an output directory, to which a file will be written in CSV
    format.  This is optional, and if no directory is specified, no file will
    be written.

The program will open a PUSH socket and connect it to the slave provider
feedback endpoint.  If anything goes wrong during the startup process, it will
send a 2-frame message on this channel, which contains (ERROR,<details>).
If all goes well, it will instead send a 2-frame text message which contains
(OK,<bound port>) just before the RunSlave() call.
*/
int main(int argc, const char** argv)
{
#ifdef CORAL_LOG_TRACE_ENABLED
    coral::log::SetLevel(coral::log::trace);
#elif defined(CORAL_LOG_DEBUG_ENABLED)
    coral::log::SetLevel(coral::log::debug);
#endif

    // We use this socket to report back to the provider that started the slave.
    zmq::context_t context;
    std::unique_ptr<zmq::socket_t> feedbackSocket;
try {
    if (argc < 5) throw std::runtime_error("Missing command line arguments");

    const auto feedbackEndpoint = std::string(argv[1]);
    feedbackSocket = std::make_unique<zmq::socket_t>(context, ZMQ_PUSH);
    feedbackSocket->setsockopt(ZMQ_LINGER, 100 /* ms */);
    feedbackSocket->connect(feedbackEndpoint.c_str());

    const auto fmuPath = std::string(argv[2]);
    const auto networkInterface = std::string(argv[3]);
    const auto commTimeout = std::chrono::seconds(std::atoi(argv[4]));

    CORAL_LOG_DEBUG(boost::format("PID: %d") % getpid());
    coral::log::Log(coral::log::info, boost::format("FMU: %s") % fmuPath);
    CORAL_LOG_TRACE(boost::format("Network interface: %s") % networkInterface);
    CORAL_LOG_TRACE(boost::format("Master-to-slave communication silence timeout: %d s")
        % commTimeout.count());

#ifdef _WIN32
    const char dirSep = '\\';
#else
    const char dirSep = '/';
#endif
    const auto outputDir = (argc > 5) ? std::string(argv[5]) + dirSep : std::string();

    const auto fmuCacheDir = boost::filesystem::temp_directory_path() / "coral" / "cache";
    auto fmuImporter = coral::fmi::Importer::Create(fmuCacheDir);
    auto fmu = fmuImporter->Import(fmuPath);
    coral::log::Log(coral::log::info, boost::format("Model name: %s")
        % fmu->Description().Name());

    auto fmiSlave = fmu->InstantiateSlave();
    std::shared_ptr<coral::slave::Instance> slave;
    if (outputDir.empty()) {
        slave = fmiSlave;
    } else {
        slave = std::make_shared<coral::slave::LoggingInstance>(
            fmiSlave,
            outputDir);
    }

    const auto bindpoint =
        coral::net::ip::Endpoint(networkInterface, "*").ToEndpoint("tcp");
    auto slaveRunner = coral::slave::Runner(
        slave,
        bindpoint,
        bindpoint,
        commTimeout);

    const auto controlEndpoint =
        coral::net::ip::Endpoint{
            slaveRunner.BoundControlEndpoint().Address()
        }.ToString();
    const auto dataPubEndpoint =
        coral::net::ip::Endpoint{
            slaveRunner.BoundDataPubEndpoint().Address()
        }.ToString();

    feedbackSocket->send("OK", 2, ZMQ_SNDMORE);
    feedbackSocket->send(controlEndpoint.data(), controlEndpoint.size(), ZMQ_SNDMORE);
    feedbackSocket->send(dataPubEndpoint.data(), dataPubEndpoint.size());
    slaveRunner.Run();
    CORAL_LOG_DEBUG("Normal shutdown");

} catch (const std::runtime_error& e) {
    if (feedbackSocket) {
        feedbackSocket->send("ERROR", 5, ZMQ_SNDMORE);
        feedbackSocket->send(e.what(), std::strlen(e.what()));
    }
    coral::log::Log(coral::log::error, e.what());
    return 1;
} catch (const std::exception& e) {
    const auto msg = std::string("Internal error (") + e.what() + ')';
    if (feedbackSocket) {
        feedbackSocket->send("ERROR", 5, ZMQ_SNDMORE);
        feedbackSocket->send(msg.data(), msg.size());
    }
    coral::log::Log(coral::log::error, msg);
    return 2;
}
return 0;
}
