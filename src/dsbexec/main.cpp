#include <cassert>
#include <iostream>
#include <memory>
#include <string>

#include "boost/chrono.hpp"
#include "boost/foreach.hpp"
#include "boost/program_options.hpp"
#include "boost/thread.hpp"
#include "zmq.hpp"

#include "dsb/domain/controller.hpp"
#include "dsb/execution/controller.hpp"
#include "config_parser.hpp"


namespace {
    const char* self = "dsbexec";
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

        const auto reportEndpoint = address + ":51380";
        const auto infoEndpoint = address + ":51382";
        const auto execLoc = dsb::execution::Locator(
            address + ":51390",
            address + ":51391",
            address + ":51392",
            address + ":51393");

        auto context = std::make_shared<zmq::context_t>();
        auto domain = dsb::domain::Controller(context, reportEndpoint, infoEndpoint);

        // TODO: Handle this waiting more elegantly, e.g. wait until all required
        // slave types are available.  Also, the waiting time is related to the
        // slave provider heartbeat time.
        std::cout << "Connected to domain; waiting for data from slave providers..." << std::endl;
        boost::this_thread::sleep_for(boost::chrono::seconds(2));

        auto controller = dsb::execution::SpawnExecution(context, execLoc.MasterEndpoint());
        const auto execConfig = ParseExecutionConfig(execConfigFile);
        controller.SetSimulationTime(execConfig.startTime, execConfig.stopTime);
        ParseSystemConfig(sysConfigFile, domain, controller, execLoc);

        // This is to work around "slow joiner syndrome".  It lets slaves'
        // subscriptions take effect before we start the simulation.
        std::cout << "Press ENTER to start simulation." << std::endl;
        std::cin.ignore();
        const auto t0 = boost::chrono::high_resolution_clock::now();

        // Super advanced master algorithm.
        const double maxTime = execConfig.stopTime - 0.9*execConfig.stepSize;
        double nextPerc = 0.1;
        for (double time = execConfig.startTime;
             time < maxTime;
             time += execConfig.stepSize)
        {
            controller.Step(time, execConfig.stepSize);
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
        controller.Terminate();
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
        const auto reportEndpoint = address + ":51380";
        const auto infoEndpoint = address + ":51382";

        auto context = std::make_shared<zmq::context_t>();
        auto domain = dsb::domain::Controller(context, reportEndpoint, infoEndpoint);

        // TODO: Handle this waiting more elegantly, e.g. wait until all required
        // slave types are available.  Also, the waiting time is related to the
        // slave provider heartbeat time.
        std::cout << "Connected to domain; waiting for data from slave providers..." << std::endl;
        boost::this_thread::sleep_for(boost::chrono::seconds(2));

        auto slaveTypes = domain.GetSlaveTypes();
        BOOST_FOREACH (const auto& st, slaveTypes) {
            std::cout << st.name << ": "
                        << st.uuid << ", "
                        << st.description << ", "
                        << st.author << ", "
                        << st.version << std::endl;
            BOOST_FOREACH (const auto& v, st.variables) {
                std::cout << "  v(" << v.ID() << "): " << v.Name() << std::endl;
            }
            BOOST_FOREACH (const auto& p, st.providers) {
                std::cout << "  " << p << std::endl;
            }
        }
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
    else {
        std::cerr << "Error: Invalid command: " << command;
        return 1;
    }
}