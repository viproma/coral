#include <algorithm>
#include <cassert>
#include <iostream>
#include <queue>
#include <string>

#include "boost/chrono.hpp"
#include "boost/foreach.hpp"
#include "boost/program_options.hpp"
#include "boost/thread.hpp"

#include "dsb/domain/controller.hpp"
#include "dsb/execution/controller.hpp"
#include "config_parser.hpp"


namespace {
    const char* self = "dsbexec";
}

dsb::domain::Locator MakeDomainLocator(const std::string& address)
{
    return dsb::domain::GetDomainEndpoints(address);
}

int Run(int argc, const char** argv)
{
    assert (argc > 1 && std::string(argv[1]) == "run");
    // Drop the first element (program name) to use "run" as the "program name".
    --argc; ++argv;

    try {
        namespace po = boost::program_options;
        po::options_description optDesc("Options");
        optDesc.add_options()
            ("help",      "Display help message")
            ("name,n", po::value<std::string>()->default_value(""), "The execution name (if left unspecified, a timestamp will be used)");
        po::options_description argDesc;
        argDesc.add(optDesc);
        argDesc.add_options()
            ("domain",      po::value<std::string>(), "The address of the domain")
            ("exec-config", po::value<std::string>(), "Execution configuration file")
            ("sys-config",  po::value<std::string>(), "System configuration file");
        po::positional_options_description posArgDesc;
        posArgDesc.add("domain", 1)
                  .add("exec-config", 1)
                  .add("sys-config", 1);
        po::variables_map optMap;
        po::store(po::command_line_parser(argc, argv).options(argDesc)
                                                     .positional(posArgDesc)
                                                     .run(),
                  optMap);

        if (argc < 2 || optMap.count("help")) {
            std::cerr <<
                "Runs a simulation.\n\n"
                "Usage:\n"
                "  " << self << " run <domain> <exec.config> <sys.config> [options...]\n\n"
                "Arguments:\n"
                "  domain       = The domain address, of the form \"tcp://hostname:port\",\n"
                "                 where the \":port\" part is only required if a nonstandard\n"
                "                 port is used.\n"
                "  exec. config = Configuration file which describes the simulation\n"
                "                 settings (start time, step size, etc.)\n"
                "  sys. config  = Configuration file which describes the system to\n"
                "                 simulate (slaves, connections, etc.)\n\n"
                      << optDesc;
            return 0;
        }
        if (!optMap.count("domain")) throw std::runtime_error("Domain address not specified");
        if (!optMap.count("exec-config")) throw std::runtime_error("No execution configuration file specified");
        if (!optMap.count("sys-config")) throw std::runtime_error("No system configuration file specified");

        const auto address = optMap["domain"].as<std::string>();
        const auto execConfigFile = optMap["exec-config"].as<std::string>();
        const auto sysConfigFile = optMap["sys-config"].as<std::string>();
        const auto execName = optMap["name"].as<std::string>();

        const auto domainLoc = MakeDomainLocator(address);
        auto domain = dsb::domain::Controller(domainLoc);

        // TODO: Handle this waiting more elegantly, e.g. wait until all required
        // slave types are available.  Also, the waiting time is related to the
        // slave provider heartbeat time.
        std::cout << "Connected to domain; waiting for data from slave providers..." << std::endl;
        boost::this_thread::sleep_for(boost::chrono::seconds(2));

        const auto execConfig = ParseExecutionConfig(execConfigFile);
        const auto execLoc = dsb::execution::SpawnExecution(
            domainLoc, execName, execConfig.commTimeout);
        const auto execSpawnTime = boost::chrono::high_resolution_clock::now();
        auto exec = dsb::execution::Controller(execLoc);
        exec.SetSimulationTime(execConfig.startTime, execConfig.stopTime);
        std::vector<SimulationEvent> unsortedScenario;
        ParseSystemConfig(sysConfigFile, domain, exec, execLoc, unsortedScenario, &std::clog);

        // Put the scenario events into a priority queue, in order of ascending
        // event time.
        auto eventTimeGreater = [] (const SimulationEvent& a, const SimulationEvent& b) {
            return a.timePoint > b.timePoint;
        };
        auto scenario = std::priority_queue
            <SimulationEvent, decltype(unsortedScenario), decltype(eventTimeGreater)>
            (eventTimeGreater, std::move(unsortedScenario));

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
        if (t0 - execSpawnTime > execConfig.commTimeout) {
            throw std::runtime_error("Communications timeout reached");
        }

        // Super advanced master algorithm.
        std::cout << "+-------------------+\n|" << std::flush;
        const double maxTime = execConfig.stopTime - 0.9*execConfig.stepSize;
        double nextPerc = 0.05;
        for (double time = execConfig.startTime;
             time < maxTime;
             time += execConfig.stepSize)
        {
            while (!scenario.empty() && scenario.top().timePoint <= time) {
                exec.SetVariables(
                    scenario.top().slave,
                    std::vector<dsb::model::VariableValue>(1, scenario.top().variableChange));
                scenario.pop();
            }
            exec.Step(time, execConfig.stepSize);
            if ((time-execConfig.startTime)/(execConfig.stopTime-execConfig.startTime) >= nextPerc) {
                //std::cout << (nextPerc * 100.0) << "%" << std::endl;
                std::cout << '#' << std::flush;
                nextPerc += 0.05;
            }
        }
        std::cout << "|\n+-------------------+\n" << std::endl;

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
        std::cerr <<
            "Lists the slave types that are available on a domain.\n\n"
            "Usage:\n"
            "  " << self << " list <domain>\n\n"
            "Arguments:\n"
            "  domain = The domain address, of the form \"tcp://hostname:port\",\n"
            "           where the \":port\" part is only required if a nonstandard\n"
            "           port is used.\n";
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
        std::cerr <<
            "Shows detailed information about a slave type.\n\n"
            "Usage:\n"
            "  " << self << " info <domain> <slave type>\n\n"
            "Arguments:\n"
            "  domain     = The domain address, of the form \"tcp://hostname:port\",\n"
            "               where the \":port\" part is only required if a nonstandard\n"
            "               port is used.\n"
            "  slave type = A slave type name\n";
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
        std::cerr <<
            "Master demonstrator.\n"
            "This program will connect to a domain and obtain information about\n"
            "available slave types, and can be used to run simple simulations.\n\n"
            "Usage:\n"
            "  " << self << " <command> [command-specific args]\n\n"
            "Commands:\n"
            "  info   Shows detailed information about one slave type\n"
            "  list   Lists available slave types\n"
            "  run    Runs a simulation\n"
            "\n"
            "Run <command> without any additional arguments for more specific help.\n";
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
