#include <chrono>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

#include "boost/filesystem.hpp"
#include "boost/foreach.hpp"
#include "boost/lexical_cast.hpp"
#include "boost/program_options.hpp"

#include "zmq.hpp"

#include "dsb/comm/messaging.hpp"
#include "dsb/comm/p2p.hpp"
#include "dsb/comm/util.hpp"
#include "dsb/domain/slave_provider.hpp"
#include "dsb/fmi.hpp"
#include "dsb/util.hpp"


// Note: Not threadsafe
std::string RandomString(size_t length)
{
    static const auto wordChars = "0123456789ABCDEFGHIJKLMNOPQRSTUVXYZabcdefghijklmnopqrstuvxyz-_";
    static const auto wordCharsLen = std::strlen(wordChars);
    auto id = std::string(length, '\xFF');
    for (size_t i = 0; i < length; ++i) {
        id[i] = wordChars[std::rand() % wordCharsLen];
    }
    return id;
}


struct StartSlave
{
public:
    StartSlave(
        const std::string& proxyEndpoint,
        const std::string& slaveExe,
        std::chrono::seconds commTimeout,
        const std::string& outputDir)
        : m_proxyEndpoint(proxyEndpoint),
          m_slaveExe(slaveExe),
          m_commTimeout(commTimeout),
          m_outputDir(outputDir.empty() ? "." : outputDir)
    {
    }

    dsb::net::SlaveLocator operator()(
        const std::string& fmuPath,
        std::chrono::milliseconds instantiationTimeout)
    {
        const auto fmuBaseName = boost::filesystem::path(fmuPath).stem().string();
        const auto outputFile = m_outputDir + '/'
            + dsb::util::Timestamp() + '_' + fmuBaseName + '_'
            + RandomString(6) + ".csv";

        auto slaveStatusSocket = zmq::socket_t(dsb::comm::GlobalContext(), ZMQ_PULL);
        const auto slaveStatusPort = dsb::comm::BindToEphemeralPort(slaveStatusSocket);
        const auto slaveStatusEp = "tcp://localhost:" + boost::lexical_cast<std::string>(slaveStatusPort);

        const auto slaveBindEndpoint =
            dsb::comm::P2PEndpoint(m_proxyEndpoint, RandomString(6));

        std::vector<std::string> args;
        args.push_back(slaveStatusEp);
        args.push_back(fmuPath);
        args.push_back(slaveBindEndpoint.URL());
        args.push_back(std::to_string(m_commTimeout.count()));
        args.push_back(outputFile);

        std::cout << "\nStarting slave...\n"
            << "  FMU      : " << fmuPath << '\n'
            << "  Output   : " << outputFile << '\n'
            << std::flush;
        dsb::util::SpawnProcess(m_slaveExe, args);

        std::clog << "Waiting for verification..." << std::flush;
        std::vector<zmq::message_t> slaveStatus;
        const auto feedbackTimedOut = !dsb::comm::Receive(
            slaveStatusSocket,
            slaveStatus,
            instantiationTimeout);
        if (feedbackTimedOut) {
            throw std::runtime_error(
                "Slave took more than "
                + boost::lexical_cast<std::string>(instantiationTimeout.count())
                + " milliseconds to start; presumably it has failed altogether");
        } else if (slaveStatus.size() != 2) {
            throw std::runtime_error("Invalid data received from slave executable");
        } else if (dsb::comm::ToString(slaveStatus[0]) == "ERROR") {
            throw std::runtime_error(dsb::comm::ToString(slaveStatus.at(1)));
        } else if (dsb::comm::ToString(slaveStatus[0]) != "OK") {
            throw std::runtime_error("Invalid data received from slave executable");
        }
        std::clog << "OK" << std::endl;
        // At this point, we know that slaveStatus contains two frames, where
        // the first one is "OK", signifying that the slave seems to be up and
        // running.  The second one contains the bound endpoint for the slave.
        const auto slaveBoundEndpoint = dsb::comm::P2PEndpoint(
            dsb::comm::ToString(slaveStatus[1]));
        // Later, slaveBoundEndpoint may be different (e.g. if the slave binds
        // locally to a different port or endpoint than the slave provider, but
        // for now we only support the proxy solution.
        assert(slaveBoundEndpoint.Endpoint() == slaveBindEndpoint.Endpoint());
        assert(slaveBoundEndpoint.Identity() == slaveBindEndpoint.Identity());
        return dsb::net::SlaveLocator(
            std::string(), // signifying that we use same proxy as provider
            slaveBoundEndpoint.Identity());
    }

private:
    std::string m_proxyEndpoint;
    std::string m_slaveExe;
    std::chrono::seconds m_commTimeout;
    std::string m_outputDir;
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
    namespace po = boost::program_options;
    po::options_description optDesc("Options");
    optDesc.add_options()
        ("help",      "Display help message")
        ("slave-exe", po::value<std::string>(),
            "The path to the DSB slave executable")
        ("output-dir,o", po::value<std::string>()->default_value(""),
            "The directory where output files should be written")
        ("timeout", po::value<unsigned int>()->default_value(3600),
            "The number of seconds of inactivity before a slave shuts itself down");
    po::options_description argDesc;
    argDesc.add(optDesc);
    argDesc.add_options()
        ("domain",    po::value<std::string>(),                    "The address of the domain")
        ("fmu",       po::value<std::vector<std::string>>(),       "The FMU files and directories");
    po::positional_options_description posArgDesc;
    posArgDesc.add("domain", 1)
              .add("fmu", -1);
    po::variables_map optMap;
    po::store(po::command_line_parser(argc, argv).options(argDesc)
                                                 .positional(posArgDesc)
                                                 .run(),
              optMap);

    if (argc < 2 || optMap.count("help")) {
        const auto self = boost::filesystem::path(argv[0]).stem().string();
        std::cerr <<
            "Slave provider demonstrator.\n"
            "This program loads one or more FMUs and makes them available as\n"
            "slaves on a domain.\n\n"
            "Usage:\n"
            "  " << self << " <domain> <fmus...> [options...]\n\n"
            "Arguments:\n"
            "  domain   The domain address, of the form \"tcp://hostname:port\",\n"
            "           where the \":port\" part is only required if a nonstandard\n"
            "           port is used.\n"
            "  fmus     FMU files and directories. Directories will be scanned\n"
            "           recursively for files with an \".fmu\" extension.\n\n"
            << optDesc;
        return 0;
    }

    if (!optMap.count("domain")) throw std::runtime_error("Domain address not specified");
    if (!optMap.count("fmu")) throw std::runtime_error("No FMUs specified");

    const auto domainAddress = optMap["domain"].as<std::string>();
    const auto outputDir = optMap["output-dir"].as<std::string>();
    const auto timeout = std::chrono::seconds(optMap["timeout"].as<unsigned int>());

    std::string slaveExe;
    if (optMap.count("slave-exe")) {
        slaveExe = optMap["slave-exe"].as<std::string>();
    } else if (const auto slaveExeEnv = std::getenv("DSB_SLAVE_EXE")) {
        slaveExe = slaveExeEnv;
    } else {
#ifdef _WIN32
        const auto exeName = "slave.exe";
#else
        const auto exeName = "slave";
#endif
        auto tryPath = dsb::util::ThisExePath().parent_path() / exeName;
        if (boost::filesystem::exists(tryPath)) {
            slaveExe = tryPath.string();
        } else {
            throw std::runtime_error("Slave executable not specified or found");
        }
    }
    assert (!slaveExe.empty());

    const auto domainLoc = dsb::net::GetDomainEndpoints(domainAddress);

    std::vector<std::string> fmuPaths;
    BOOST_FOREACH (const auto& fmuSpec, optMap["fmu"].as<std::vector<std::string>>()) {
        if (boost::filesystem::is_directory(fmuSpec)) {
            ScanDirectoryForFMUs(fmuSpec, fmuPaths);
        } else {
            fmuPaths.push_back(fmuSpec);
        }
    }
    std::vector<std::unique_ptr<dsb::domain::ISlaveType>> fmus;
    std::vector<dsb::domain::ISlaveType*> fmuPtrs;
    for (auto it = fmuPaths.begin(); it != fmuPaths.end(); ++it) {
        fmus.push_back(dsb::fmi::MakeSlaveType(*it,
            StartSlave(domainLoc.InfoSlavePEndpoint(), slaveExe, timeout, outputDir)));
        fmuPtrs.push_back(fmus.back().get());
        std::cout << "FMU loaded: " << *it << std::endl;
    }
    std::cout << fmus.size() << " FMUs loaded" << std::endl;
    dsb::domain::SlaveProvider(domainLoc, fmuPtrs);
} catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
}
return 0;
}
