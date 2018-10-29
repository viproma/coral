/*
Copyright 2013-present, SINTEF Ocean.
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
#include <iostream>
#include <stdexcept>
#include <string>

#include <boost/filesystem.hpp>
#include <zmq.hpp>

#include <coral/fmi/fmu.hpp>
#include <coral/fmi/importer.hpp>
#include <coral/log.hpp>
#include <coral/net/zmqx.hpp>
#include <coral/slave.hpp>
#include <coral/util/console.hpp>


namespace
{
    const char* MY_NAME = "coralslave";
    const char* DEFAULT_NETWORK_INTERFACE = "127.0.0.1";
}


int main(int argc, const char** argv)
{
    // Declared here for use in exception handlers:
    zmq::context_t context;
    std::unique_ptr<zmq::socket_t> feedbackSocket;

try {
    namespace po = boost::program_options;
    po::options_description options("Options");
    options.add_options()
        ("control-port", po::value<std::uint16_t>()->default_value(0),
            "The port number to which the master will send commands. If left "
            "unspecified (or set to 0), an OS-assigned port will be used.")
        ("data-port", po::value<std::uint16_t>()->default_value(0),
            "The port number to which other slaves will send data. If left "
            "unspecified (or set to 0), an OS-assigned port will be used.")
        ("hangaround-time", po::value<int>()->default_value(-1),
            "A number of seconds after which the slave will shut itself down "
            "if no master has yet connected.  The special value -1, which is "
            "the default, means \"never\".")
        ("interface", po::value<std::string>()->default_value(DEFAULT_NETWORK_INTERFACE),
            "The IP address or (OS-specific) name of the network interface to "
            "use for network communications, or \"*\" for all/any.")
        ("no-output",
            "Disable file output of variable values.")
        ("output-dir,o", po::value<std::string>()->default_value("."),
            "The directory where output files should be written.")
        ("coralslaveprovider-endpoint", po::value<std::string>(),
            "For use by coralslaveprovider: An endpoint on which the provider "
            "is listening for status messages.");
    coral::util::AddLoggingOptions(options);
    po::options_description positionalOptions("Arguments");
    positionalOptions.add_options()
        ("fmu", po::value<std::string>(),
            "The FMU from which the slave should be instantiated.");
    po::positional_options_description positions;
    positions.add("fmu", 1);

    const auto args = coral::util::CommandLine(argc-1, argv+1);
    const auto optionValues = coral::util::ParseArguments(
        args, options, positionalOptions, positions,
        std::cerr,
        MY_NAME,
        "Slave (" CORAL_PROGRAM_NAME_VERSION ")\n\n"
        "Creates and executes an instance of an FMU for co-simulation.");
    if (!optionValues) return 0;
    coral::util::UseLoggingArguments(*optionValues);

    if (optionValues->count("coralslaveprovider-endpoint")) {
        CORAL_LOG_DEBUG("Assuming started by slave provider");
        const auto feedbackEndpoint =
            (*optionValues)["coralslaveprovider-endpoint"].as<std::string>();
        feedbackSocket = std::make_unique<zmq::socket_t>(context, ZMQ_PUSH);
        feedbackSocket->setsockopt(ZMQ_LINGER, 100 /* ms */);
        feedbackSocket->connect(feedbackEndpoint.c_str());
    }

    const auto controlPortN = (*optionValues)["control-port"].as<std::uint16_t>();
    const auto controlPort = controlPortN == 0
        ? coral::net::ip::Port{"*"}
        : coral::net::ip::Port{controlPortN};
    const auto dataPortN = (*optionValues)["data-port"].as<std::uint16_t>();
    const auto dataPort = dataPortN == 0
        ? coral::net::ip::Port{"*"}
        : coral::net::ip::Port{dataPortN};
    const auto hangaroundTime =
        std::chrono::seconds((*optionValues)["hangaround-time"].as<int>());
    if (hangaroundTime < std::chrono::seconds(-1)) {
        throw std::runtime_error("Invalid hangaround-time value");
    }
    const auto networkInterface = coral::net::ip::Address{
        (*optionValues)["interface"].as<std::string>()};
    const auto enableOutput = !optionValues->count("no-output");
    const auto outputDir = (*optionValues)["output-dir"].as<std::string>();

    if (!optionValues->count("fmu")) {
        throw std::runtime_error("No FMU specified");
    }
    const auto fmuPath = (*optionValues)["fmu"].as<std::string>();

    CORAL_LOG_DEBUG(boost::format("PID: %d") % getpid());
    coral::log::Log(coral::log::info, boost::format("FMU: %s") % fmuPath);
    CORAL_LOG_TRACE(boost::format("Network interface: %s") % networkInterface.ToString());
    CORAL_LOG_TRACE(boost::format("Hangaround time: %d s") % hangaroundTime.count());

    const auto fmuCacheDir = boost::filesystem::temp_directory_path() / "coral" / "cache";
    auto fmuImporter = coral::fmi::Importer::Create(fmuCacheDir);
    auto fmu = fmuImporter->Import(fmuPath);
    coral::log::Log(coral::log::info, boost::format("Model name: %s")
        % fmu->Description().Name());

    auto fmiSlave = fmu->InstantiateSlave();
    std::shared_ptr<coral::slave::Instance> slave;
    if (enableOutput) {
#ifdef _WIN32
        const char dirSep = '\\';
#else
        const char dirSep = '/';
#endif
        slave = std::make_shared<coral::slave::LoggingInstance>(
            fmiSlave,
            outputDir + dirSep);
    } else {
        slave = fmiSlave;
    }
    auto slaveRunner = coral::slave::Runner(
        slave,
        coral::net::ip::Endpoint(networkInterface, controlPort).ToEndpoint("tcp"),
        coral::net::ip::Endpoint(networkInterface, dataPort).ToEndpoint("tcp"),
        hangaroundTime);

    const auto controlEndpoint =
        coral::net::ip::Endpoint{slaveRunner.BoundControlEndpoint().Address()};
    const auto dataPubEndpoint =
        coral::net::ip::Endpoint{slaveRunner.BoundDataPubEndpoint().Address()};

    if (feedbackSocket) {
        const auto ceps = controlEndpoint.ToString();
        const auto deps = dataPubEndpoint.ToString();
        feedbackSocket->send("OK", 2, ZMQ_SNDMORE);
        feedbackSocket->send(ceps.data(), ceps.size(), ZMQ_SNDMORE);
        feedbackSocket->send(deps.data(), deps.size());
    } else {
        if (controlPort.IsAnyPort()) {
            std::cout
                << "Control port: "
                << controlEndpoint.Port().ToNumber() << std::endl;
        }
        if (dataPort.IsAnyPort()) {
            std::cout
                << "Data port: "
                << dataPubEndpoint.Port().ToNumber() << std::endl;
        }
    }

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
