#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>

#include "boost/chrono.hpp"
#include "boost/foreach.hpp"
#include "boost/thread.hpp"

#include "dsb/domain/controller.hpp"
#include "dsb/execution/controller.hpp"
#include "config_parser.hpp"


namespace {
    const char* self = "dsbexec";
}

dsb::domain::Locator MakeDomainLocator(const std::string& address)
{
    return dsb::domain::Locator(
        address + ":51380",
        address + ":51382",
        address + ":51384");
}

int Run(int argc, const char** argv)
{
    assert (argc > 1 && std::string(argv[1]) == "run");
    if (argc < 5) {
        std::cerr << "Usage: " << self << " run <domain> <exec. config> <sys. config>\n"
                  << "  domain       = domain address (e.g. tcp://localhost)\n"
                  << "  exec. config = the execution configuration file\n"
                  << "  sys. config  = the system configuration file"
                  << std::endl;
        return 0;
    }
    try {
        const auto address = std::string(argv[2]);
        const auto execConfigFile = std::string(argv[3]);
        const auto sysConfigFile = std::string(argv[4]);

        const auto domainLoc = MakeDomainLocator(address);
        auto domain = dsb::domain::Controller(domainLoc);

        // TODO: Handle this waiting more elegantly, e.g. wait until all required
        // slave types are available.  Also, the waiting time is related to the
        // slave provider heartbeat time.
        std::cout << "Connected to domain; waiting for data from slave providers..." << std::endl;
        boost::this_thread::sleep_for(boost::chrono::seconds(2));

        const auto execLoc = dsb::execution::SpawnExecution(domainLoc);
        auto exec = dsb::execution::Controller(execLoc);
        const auto execConfig = ParseExecutionConfig(execConfigFile);
        exec.SetSimulationTime(execConfig.startTime, execConfig.stopTime);
        ParseSystemConfig(sysConfigFile, domain, exec, execLoc);

        // This is to work around "slow joiner syndrome".  It lets slaves'
        // subscriptions take effect before we start the simulation.
        std::cout <<
            "Waiting for slaves..."
            "\n[NOTE TO TESTERS: If the program appears to hang at this point, "
            "it could be because one or more of the slaves failed to start. "
            "Check that the number of slave windows matches the number of "
            "expected slaves.]" << std::endl;
        exec.WaitForReady();
        std::cout << "All slaves are present. Press ENTER to start simulation." << std::endl;
        std::cin.ignore();
        const auto t0 = boost::chrono::high_resolution_clock::now();

        // Super advanced master algorithm.
        const double maxTime = execConfig.stopTime - 0.9*execConfig.stepSize;
        double nextPerc = 0.1;
        for (double time = execConfig.startTime;
             time < maxTime;
             time += execConfig.stepSize)
        {
            exec.Step(time, execConfig.stepSize);
            if ((time-execConfig.startTime)/(execConfig.stopTime-execConfig.startTime) >= nextPerc) {
                std::cout << (nextPerc * 100.0) << "%" << std::endl;
                nextPerc += 0.1;
            }
        }

        // Termination
        const auto t1 = boost::chrono::high_resolution_clock::now();
        const auto simTime = boost::chrono::round<boost::chrono::milliseconds>(t1 - t0);
        std::cout << "Completed in " << simTime << '.' << std::endl;

        // Give ZMQ time to send all TERMINATE messages
        exec.Terminate();
        std::cout << "Terminated. Press ENTER to quit." << std::endl;
        std::cin.ignore();
    } catch (const std::runtime_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}


int List(int argc, const char** argv)
{
    assert (argc > 1 && std::string(argv[1]) == "list");
    if (argc < 3) {
        std::cerr << "Usage: " << self << " list <domain>\n"
                  << "  domain = domain address (e.g. tcp://localhost)"
                  << std::endl;
        return 0;
    }
    try {
        const auto address = std::string(argv[2]);
        const auto domainLoc = MakeDomainLocator(address);

        auto domain = dsb::domain::Controller(domainLoc);

        // TODO: Handle this waiting more elegantly, e.g. wait until all required
        // slave types are available.  Also, the waiting time is related to the
        // slave provider heartbeat time.
        std::cout << "Connected to domain; waiting for data from slave providers..." << std::endl;
        boost::this_thread::sleep_for(boost::chrono::seconds(2));

        auto slaveTypes = domain.GetSlaveTypes();
        BOOST_FOREACH (const auto& st, slaveTypes) {
            std::cout << st.name << '\n';
        }
    } catch (const std::runtime_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}


int Info(int argc, const char** argv)
{
    assert (argc > 1 && std::string(argv[1]) == "info");
    if (argc < 4) {
        std::cerr << "Usage: " << self << " info <domain> <slave type>\n"
                  << "  domain     = domain address (e.g. tcp://localhost)\n"
                  << "  slave type = a slave type name\n"
                  << std::flush;
        return 0;
    }
    try {
        const auto address = std::string(argv[2]);
        const auto slaveType = std::string(argv[3]);

        const auto domainLoc = MakeDomainLocator(address);
        auto domain = dsb::domain::Controller(domainLoc);

        // TODO: Handle this waiting more elegantly, e.g. wait until all required
        // slave types are available.  Also, the waiting time is related to the
        // slave provider heartbeat time.
        std::cout << "Connected to domain; waiting for data from slave providers..." << std::endl;
        boost::this_thread::sleep_for(boost::chrono::seconds(2));

        const auto slaveTypes = domain.GetSlaveTypes();
        const auto it = std::find_if(slaveTypes.begin(), slaveTypes.end(),
            [&](const dsb::domain::Controller::SlaveType& s) { return s.name == slaveType; });
        if (it == slaveTypes.end()) {
            throw std::runtime_error("Unknown slave type: " + slaveType);
        }
        const auto versionSpec = it->version.empty() ? std::string()
                                                     : " v" + it->version;
        std::cout << "\nname " << it->name << '\n'
                  << "uuid " << it->uuid << '\n'
                  << "description " << it->description << '\n'
                  << "author " << it->author << '\n'
                  << "version " << it->version << '\n'
                  << "parameters {\n";
        BOOST_FOREACH (const auto& v, it->variables) {
            if (v.Causality() == dsb::model::PARAMETER_CAUSALITY) {
                std::cout << "  " << v.Name() << "\n";
            }
        }
        std::cout << "}\ninputs {\n";
        BOOST_FOREACH (const auto& v, it->variables) {
            if (v.Causality() == dsb::model::INPUT_CAUSALITY) {
                std::cout << "  " << v.Name() << "\n";
            }
        }
        std::cout << "}\noutputs {\n";
        BOOST_FOREACH (const auto& v, it->variables) {
            if (v.Causality() == dsb::model::OUTPUT_CAUSALITY) {
                std::cout << "  " << v.Name() << "\n";
            }
        }
        std::cout << "}\nproviders {\n";
        BOOST_FOREACH (const auto& p, it->providers) {
            std::cout << "  " << p << "\n";
        }
        std::cout << "}" << std::endl;
    } catch (const std::runtime_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}


int main(int argc, const char** argv)
{
    if (argc < 2) {
        std::cerr << "Usage: " << self << " <command> [command-specific args]\n"
                  << "\n"
                  << "Commands:\n"
                  << "  list   List available slave types\n"
                  << "  run    Run a simulation\n"
                  << "\n"
                  << "Run <command> without any additional arguments for more specific help.\n"
                  << std::flush;
        return 0;
    }
    const auto command = std::string(argv[1]);
    if (command == "run") return Run(argc, argv);
    else if (command == "list") return List(argc, argv);
    else if (command == "info") return Info(argc, argv);
    else {
        std::cerr << "Error: Invalid command: " << command;
        return 1;
    }
}