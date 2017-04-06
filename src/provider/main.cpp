/*
Copyright 2013-2017, SINTEF Ocean and the Coral contributors.
This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>

#include <zmq.hpp>

#include <coral/fmi/fmu.hpp>
#include <coral/fmi/importer.hpp>
#include <coral/log.hpp>
#include <coral/net.hpp>
#include <coral/net/zmqx.hpp>
#include <coral/provider.hpp>
#include <coral/util.hpp>
#include <coral/util/console.hpp>


namespace
{
    const std::string DEFAULT_NETWORK_INTERFACE = "*";
    const std::uint16_t DEFAULT_DISCOVERY_PORT = 10272;
#ifdef _WIN32
    const std::string DEFAULT_SLAVE_EXE = "coralslave.exe";
#else
    const std::string DEFAULT_SLAVE_EXE = "coralslave";
#endif
}


struct MySlaveCreator : public coral::provider::SlaveCreator
{
public:
    MySlaveCreator(
        coral::fmi::Importer& importer,
        const boost::filesystem::path& fmuPath,
        const coral::net::ip::Address& networkInterface,
        const std::string& slaveExe,
        std::chrono::seconds masterInactivityTimeout,
        const std::string& outputDir)
        : m_fmuPath{fmuPath}
        , m_fmu{importer.Import(fmuPath)}
        , m_networkInterface{networkInterface}
        , m_slaveExe(slaveExe)
        , m_masterInactivityTimeout{masterInactivityTimeout}
        , m_outputDir(outputDir.empty() ? "." : outputDir)
    {
    }

    const coral::model::SlaveTypeDescription& Description() const override
    {
        return m_fmu->Description();
    }

    bool Instantiate(
        std::chrono::milliseconds timeout,
        coral::net::SlaveLocator& slaveLocator) override
    {
        m_instantiationFailureDescription.clear();
        try {
            auto slaveStatusSocket = zmq::socket_t(coral::net::zmqx::GlobalContext(), ZMQ_PULL);
            const auto slaveStatusPort = coral::net::zmqx::BindToEphemeralPort(slaveStatusSocket);
            const auto slaveStatusEp = "tcp://localhost:" + boost::lexical_cast<std::string>(slaveStatusPort);

            std::vector<std::string> args;
            args.push_back(slaveStatusEp);
            args.push_back(m_fmuPath.string());
            args.push_back(m_networkInterface.ToString());
            args.push_back(std::to_string(m_masterInactivityTimeout.count()));
            args.push_back(m_outputDir);

            std::cout << "\nStarting slave...\n"
                << "  FMU       : " << m_fmuPath << '\n'
                << std::flush;
            coral::util::SpawnProcess(m_slaveExe, args);

            std::clog << "Waiting for verification..." << std::flush;
            std::vector<zmq::message_t> slaveStatus;
            const auto feedbackTimedOut = !coral::net::zmqx::WaitForIncoming(
                slaveStatusSocket,
                timeout);
            if (feedbackTimedOut) {
                throw std::runtime_error(
                    "Slave took more than "
                    + boost::lexical_cast<std::string>(timeout.count())
                    + " milliseconds to start; presumably it has failed altogether");
            }
            coral::net::zmqx::Receive(slaveStatusSocket, slaveStatus);
            if (coral::net::zmqx::ToString(slaveStatus[0]) == "ERROR" &&
                    slaveStatus.size() == 2) {
                throw std::runtime_error(coral::net::zmqx::ToString(slaveStatus[1]));
            } else if (coral::net::zmqx::ToString(slaveStatus[0]) != "OK" ||
                    slaveStatus.size() < 3 ||
                    slaveStatus[1].size() == 0 ||
                    slaveStatus[2].size() == 0) {
                throw std::runtime_error("Invalid data received from slave executable");
            }
            // At this point, we know that slaveStatus contains three frames, where
            // the first one is "OK", signifying that the slave seems to be up and
            // running.  The following two contains the endpoints to which the slave
            // is bound.
            slaveLocator = coral::net::SlaveLocator{
                coral::net::ip::Endpoint{coral::net::zmqx::ToString(slaveStatus[1])}
                    .ToEndpoint("tcp"),
                coral::net::ip::Endpoint{coral::net::zmqx::ToString(slaveStatus[2])}
                    .ToEndpoint("tcp")
            };

            std::clog << "OK" << std::endl;
            return true;
        } catch (const std::exception& e) {
            m_instantiationFailureDescription = e.what();
            return false;
        }
    }

    std::string InstantiationFailureDescription() const override
    {
        return m_instantiationFailureDescription;
    }

private:
    boost::filesystem::path m_fmuPath;
    std::shared_ptr<coral::fmi::FMU> m_fmu;
    coral::net::ip::Address m_networkInterface;
    std::string m_slaveExe;
    std::chrono::seconds m_masterInactivityTimeout;
    std::string m_outputDir;
    std::string m_instantiationFailureDescription;
};


void ScanDirectoryForFMUs(
    const std::string& directory,
    std::vector<std::string>& fmuPaths)
{
    namespace fs = boost::filesystem;
    for (auto it = fs::recursive_directory_iterator(directory);
         it != fs::recursive_directory_iterator();
         ++it)
    {
        if (it->path().extension() == ".fmu") {
            fmuPaths.push_back(it->path().string());
        }
    }
}


int main(int argc, const char** argv)
{
try {
#ifdef CORAL_LOG_TRACE_ENABLED
    coral::log::SetLevel(coral::log::trace);
#elif defined(CORAL_LOG_DEBUG_ENABLED)
    coral::log::SetLevel(coral::log::debug);
#endif

    const auto fmuCacheDir = boost::filesystem::temp_directory_path() / "coral" / "cache";
    auto importer = coral::fmi::Importer::Create(fmuCacheDir);

    namespace po = boost::program_options;
    po::options_description options("Options");
    options.add_options()
        ("clean-cache",
            "Clear the cache which contains previously unpacked FMU contents."
            "The program will exit immediately after performing this action.")
        ("interface", po::value<std::string>()->default_value(DEFAULT_NETWORK_INTERFACE),
            "The IP address or (OS-specific) name of the network interface to "
            "use for network communications, or \"*\" for all/any.")
        ("output-dir,o", po::value<std::string>()->default_value("."),
            "The directory where output files should be written")
        ("port", po::value<std::uint16_t>()->default_value(DEFAULT_DISCOVERY_PORT),
            "The UDP port used to broadcast information about this slave provider. "
            "The master must listen on the same port.")
        ("slave-exe", po::value<std::string>(),
            "The path to the slave executable")
        ("timeout", po::value<int>()->default_value(3600),
            "The number of seconds slaves should wait for commands from a master "
            "before assuming that the connection is broken and shutting themselves "
            "down.  The special value -1 means \"never\".");
    po::options_description positionalOptions("Arguments");
    positionalOptions.add_options()
        ("fmu",       po::value<std::vector<std::string>>(), "The FMU files and directories");
    po::positional_options_description positions;
    positions.add("fmu", -1);

    const auto args = coral::util::CommandLine(argc-1, argv+1);
    const auto optionValues = coral::util::ParseArguments(
        args, options, positionalOptions, positions,
        std::cerr,
        "slave_provider",
        "Slave provider (" CORAL_PROGRAM_NAME_VERSION ")\n\n"
        "This program loads one or more FMUs and makes them available as\n"
        "slaves on a domain.");
    if (!optionValues) return 0;
    if (optionValues->count("clean-cache")) {
        importer->CleanCache();
        return 0;
    }
    if (!optionValues->count("fmu")) throw std::runtime_error("No FMUs specified");

    const auto networkInterface = coral::net::ip::Address{
        (*optionValues)["interface"].as<std::string>()};
    const auto outputDir = (*optionValues)["output-dir"].as<std::string>();
    const auto discoveryPort = coral::net::ip::Port{
        (*optionValues)["port"].as<std::uint16_t>()};
    const auto timeout = std::chrono::seconds((*optionValues)["timeout"].as<int>());
    if (timeout < std::chrono::seconds(-1)) {
        throw std::runtime_error("Invalid timeout value");
    }

    std::string slaveExe;
    if (optionValues->count("slave-exe")) {
        slaveExe = (*optionValues)["slave-exe"].as<std::string>();
    } else if (const auto slaveExeEnv = std::getenv("CORAL_SLAVE_EXE")) {
        slaveExe = slaveExeEnv;
    } else {
        const auto tryPath = coral::util::ThisExePath().parent_path()
            / DEFAULT_SLAVE_EXE;
        if (boost::filesystem::exists(tryPath)) {
            slaveExe = tryPath.string();
        } else {
            throw std::runtime_error("Slave executable not specified or found");
        }
    }
    assert (!slaveExe.empty());

    std::vector<std::string> fmuPaths;
    for (const auto& fmuSpec :
            (*optionValues)["fmu"].as<std::vector<std::string>>()) {
        if (boost::filesystem::is_directory(fmuSpec)) {
            ScanDirectoryForFMUs(fmuSpec, fmuPaths);
        } else {
            fmuPaths.push_back(fmuSpec);
        }
    }

    std::vector<std::unique_ptr<coral::provider::SlaveCreator>> fmus;
    int failedFMUS = 0;
    for (const auto& p : fmuPaths) {
        try {
            fmus.push_back(std::make_unique<MySlaveCreator>(
                *importer,
                p,
                networkInterface,
                slaveExe,
                timeout,
                outputDir));
            std::cout << "FMU loaded: " << p << std::endl;
        } catch (const std::runtime_error& e) {
            ++failedFMUS;
            std::cerr << "Error: Failed to load FMU \"" << p
                << "\": " << e.what() << std::endl;
        }
    }
    std::cout << fmus.size() << " FMUs loaded";
    if (failedFMUS > 0) {
        std::cout << ", " << failedFMUS << " failed";
    }
    std::cout << std::endl;

    coral::provider::SlaveProvider slaveProvider{
        coral::util::RandomUUID(),
        std::move(fmus),
        networkInterface,
        discoveryPort,
        [](std::exception_ptr e) {
            try { std::rethrow_exception(e); }
            catch (const std::exception& e) {
                std::cerr << "Error: " << e.what() << std::endl;
                std::exit(1);
            }
        }
    };
    std::cout << "Press ENTER to quit" << std::flush;
    std::cin.ignore();
    slaveProvider.Stop();
} catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
}
return 0;
}
