#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <queue>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include "boost/program_options.hpp"

#include "dsb/log.hpp"
#include "dsb/master.hpp"
#include "dsb/util/console.hpp"

#include "config_parser.hpp"


namespace {
    const std::string self = "dsbexec";
    const std::string DEFAULT_NETWORK_INTERFACE = "*";
    const std::uint16_t DEFAULT_DISCOVERY_PORT = 10272;
}


int Run(const std::vector<std::string>& args)
{
    try {
        namespace po = boost::program_options;
        po::options_description options("Options");
        options.add_options()
            ("interface", po::value<std::string>()->default_value(DEFAULT_NETWORK_INTERFACE),
                "The IP address or (OS-specific) name of the network interface to "
                "use for network communications, or \"*\" for all/any.")
            ("name,n", po::value<std::string>()->default_value(""),
                "The execution name (if left unspecified, a timestamp will be used)")
            ("port", po::value<std::uint16_t>()->default_value(DEFAULT_DISCOVERY_PORT),
                "The UDP port used to listen for slave providers.")
            ("warnings,w",
                "Enable warnings while parsing execution configuration file");
        po::options_description positionalOptions("Arguments");
        positionalOptions.add_options()
            ("exec-config", po::value<std::string>(),
                "Configuration file which describes the simulation settings "
                "(start time, step size, etc.)")
            ("sys-config",  po::value<std::string>(),
                "Configuration file which describes the system to simulate "
                "(slaves, connections, etc.)\n");
        po::positional_options_description positions;
        positions.add("exec-config", 1)
                 .add("sys-config", 1);

        const auto argValues = dsb::util::ParseArguments(
            args, options, positionalOptions, positions,
            std::cerr,
            self + " run",
            "Runs a simulation.",
            // Extra help:
            "Execution configuration file:\n"
            "  The execution configuration file is a simple text file consisting of keys\n"
            "  and values, where each key is separated from its value by whitespace.\n"
            "  (Specifically, it must be in the Boost INFO format; see here for more info:\n"
            "  http://www.boost.org/doc/libs/release/libs/property_tree/ ).  The\n"
            "  following example file contains all the settings currently available:\n"
            "\n"
            "      ; Time step size (mandatory)\n"
            "      step_size 0.2\n"
            "\n"
            "      ; Simulation start time (optional, defaults to 0)\n"
            "      start 0.0\n"
            "\n"
            "      ; Simulation end time (optional, defaults to \"indefinitely\")\n"
            "      stop 100.0\n"
            "\n"
            "      ; General command/communications timeout, in milliseconds (optional,\n"
            "      ; defaults to 1000 ms)\n"
            "      ;\n"
            "      ; This is how long the master will wait for replies to commands sent\n"
            "      ; to a slave before it considers the connection to be broken.  It should\n"
            "      ; generally be a short duration, as it is used for \"cheap\" operations\n"
            "      ; (i.e., everything besides the \"perform time step\" command).\n"
            "      comm_timeout_ms 5000\n"
            "\n"
            "      ; Time step timeout multiplier (optional, defaults to 100)\n"
            "      ;\n"
            "      ; This controls the amount of time the slaves get to carry out a time\n"
            "      ; step.  The timeout is set equal to step_timeout_multiplier` times the\n"
            "      ; step size, where the step size is assumed to be in seconds.\n"
            "      step_timeout_multiplier 10\n"
            "\n"
            "      ; Slave timeout, in seconds (optional, defaults to 3600 s = 1 hour)\n"
            "      ;\n"
            "      ; This controls how long the slaves (and the execution broker, if this\n"
            "      ; is used) will wait for commands from the master.  This should\n"
            "      ; generally be a long duration, as the execution master could for\n"
            "      ; instance be waiting for some user input before starting/continuing\n"
            "      ; the simulation.\n"
            "      slave_timeout_s 1000\n"
            "\n"
            "      ; Slave instantiation timeout, in milliseconds (optional, defaults\n"
            "      ; to 30,000 ms = 30 s)\n"
            "      ;\n"
            "      ; This is the maximum amount of time that may pass from the moment the\n"
            "      ; instantiation command is issued to when the slave is ready for\n"
            "      ; simulation.  Some slaves may take a long time to instantiate, either\n"
            "      ; because the FMU is very large and thus takes a long time to unpack\n"
            "      ; or because its instantiation routine is very demanding.\n"
            "      instantiation_timeout_ms 10000\n");
        if (!argValues) return 0;

        if (!argValues->count("exec-config")) throw std::runtime_error("No execution configuration file specified");
        if (!argValues->count("sys-config")) throw std::runtime_error("No system configuration file specified");
        const auto execConfigFile = (*argValues)["exec-config"].as<std::string>();
        const auto sysConfigFile = (*argValues)["sys-config"].as<std::string>();
        const auto networkInterface = dsb::net::ip::Address{
            (*argValues)["interface"].as<std::string>()};
        const auto execName = (*argValues)["name"].as<std::string>();
        const auto discoveryPort = dsb::net::ip::Port{
            (*argValues)["port"].as<std::uint16_t>()};
        const auto warningStream = argValues->count("warnings") ? &std::clog : nullptr;

        auto providers = dsb::master::ProviderCluster{
            networkInterface,
            discoveryPort};

        // TODO: Handle this waiting more elegantly, e.g. wait until all required
        // slave types are available.  Also, the waiting time is related to the
        // slave provider heartbeat time.
        std::cout << "Looking for slave providers..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));

        std::cout << "Parsing execution configuration file '" << execConfigFile
                  << "'" << std::endl;
        const auto execConfig = ParseExecutionConfig(execConfigFile);

        std::cout << "Creating new execution" << std::endl;
        auto exec = dsb::master::Execution(
            execName,
            execConfig.startTime,
            execConfig.stopTime);
        const auto execSpawnTime = std::chrono::high_resolution_clock::now();

        std::cout << "Parsing model configuration file '" << sysConfigFile
                  << "' and spawning slaves" << std::endl;
        std::vector<SimulationEvent> unsortedScenario;
        ParseSystemConfig(sysConfigFile, providers, exec, unsortedScenario,
                          execConfig.commTimeout, execConfig.instantiationTimeout,
                          warningStream);

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
        std::cout << "All slaves are present. Press ENTER to start simulation." << std::endl;
        std::cin.ignore();
        const auto t0 = std::chrono::high_resolution_clock::now();
        if (t0 - execSpawnTime > execConfig.slaveTimeout) {
            throw std::runtime_error("Communications timeout reached");
        }

        // Super advanced master algorithm.
        const double maxTime = execConfig.stopTime - 0.9*execConfig.stepSize;
        double nextPerc = 0.05;
        const auto stepTimeout = std::chrono::milliseconds(
            boost::numeric_cast<typename std::chrono::milliseconds::rep>(
                execConfig.stepSize * 1000 * execConfig.stepTimeoutMultiplier));

        const double clockRes = // the resolution of the clock, in ticks/sec
            static_cast<double>(std::chrono::high_resolution_clock::duration::period::num)
            / std::chrono::high_resolution_clock::duration::period::den;
        auto prevRealTime = std::chrono::high_resolution_clock::now();
        auto prevSimTime = execConfig.startTime;

        for (double time = execConfig.startTime;
             time < maxTime;
             time += execConfig.stepSize)
        {
            if (!scenario.empty() && scenario.top().timePoint <= time) {
                std::vector<dsb::master::SlaveConfig> settings;
                std::map<dsb::model::SlaveID, std::size_t> indexes;
                while (!scenario.empty() && scenario.top().timePoint <= time) {
                    const auto& scenEvent = scenario.top();
                    auto indexIt = indexes.find(scenEvent.slave);
                    if (indexIt == indexes.end()) {
                        indexIt = indexes.insert(
                                std::make_pair(scenEvent.slave, settings.size())
                            ).first;
                        settings.emplace_back();
                        settings.back().slaveID = scenEvent.slave;
                    }
                    settings[indexIt->second].variableSettings.push_back(
                        dsb::model::VariableSetting(
                            scenEvent.variable,
                            scenEvent.newValue));
                    scenario.pop();
                }
                exec.Reconfigure(settings, execConfig.commTimeout);
            }
            if (exec.Step(execConfig.stepSize, stepTimeout) != dsb::master::StepResult::completed) {
                throw std::runtime_error("One or more slaves failed to perform the time step");
            }
            exec.AcceptStep(execConfig.commTimeout);

            // Print how far we've gotten in the simulation and how fast it's
            // going.
            if ((time-execConfig.startTime)/(execConfig.stopTime-execConfig.startTime) >= nextPerc) {
                const auto realTime = std::chrono::high_resolution_clock::now();
                const auto rti = (time - prevSimTime)
                    / ((realTime - prevRealTime).count() * clockRes);
                std::cout << (nextPerc * 100.0) << "%  RTI=" << rti << std::endl;
                nextPerc += 0.05;
                prevRealTime = realTime;
                prevSimTime = time;
            }
        }

        // Termination
        const auto t1 = std::chrono::high_resolution_clock::now();
        const auto simTime = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);
        std::cout << "Completed in " << simTime.count() << " ms." << std::endl;

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


int List(const std::vector<std::string>& args)
{
    try {
        namespace po = boost::program_options;
        po::options_description options("Options");
        options.add_options()
            ("interface", po::value<std::string>()->default_value(DEFAULT_NETWORK_INTERFACE),
                "The IP address or (OS-specific) name of the network interface to "
                "use for network communications, or \"*\" for all/any.")
            ("port", po::value<std::uint16_t>()->default_value(DEFAULT_DISCOVERY_PORT),
                "The UDP port used to listen for slave providers.");
        const auto argValues = dsb::util::ParseArguments(
            args,
            options,
            po::options_description("Arguments"),
            po::positional_options_description(),
            std::cerr,
            self + " list",
            "Lists the slave types that are available on the network.");
        if (!argValues) return 0;
        const auto networkInterface = dsb::net::ip::Address{
            (*argValues)["interface"].as<std::string>()};
        const auto discoveryPort = dsb::net::ip::Port{
            (*argValues)["port"].as<std::uint16_t>()};

        auto providers = dsb::master::ProviderCluster{
            networkInterface,
            discoveryPort};

        // TODO: Handle this waiting more elegantly, e.g. wait until all required
        // slave types are available.  Also, the waiting time is related to the
        // slave provider heartbeat time.
        std::cout << "Looking for slave providers..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));

        auto slaveTypes = providers.GetSlaveTypes(std::chrono::seconds(1));
        for (const auto& st : slaveTypes) {
            std::cout << st.description.Name() << '\n';
            //DSB_LOG_TRACE(st.description.Name());
        }
    } catch (const std::runtime_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}


// Helper function which returns whether s contains c
bool Contains(const std::string& s, char c)
{
    return s.find(c) < s.size();
}


int LsVars(const std::vector<std::string>& args)
{
    try {
        namespace po = boost::program_options;
        po::options_description options("Options");
        options.add_options()
            ("causality,c", po::value<std::string>()->default_value("cilop"),
                "The causalities to include.  May contain one or more of the "
                "following characters: c=calculated parameter, i=input, "
                "l=local, o=output, p=parameter")
            ("interface", po::value<std::string>()->default_value(DEFAULT_NETWORK_INTERFACE),
                "The IP address or (OS-specific) name of the network interface to "
                "use for network communications, or \"*\" for all/any.")
            ("long,l",
                "\"Long\" format.  Shows type, causality and variability as a "
                "3-character string after the variable name.")
            ("port", po::value<std::uint16_t>()->default_value(DEFAULT_DISCOVERY_PORT),
                "The UDP port used to listen for slave providers.")
            ("type,t", po::value<std::string>()->default_value("birs"),
                "The data type(s) to include.  May contain one or more of the "
                "following characters: b=boolean, i=integer, r=real, s=string")
            ("variability,v", po::value<std::string>()->default_value("cdftu"),
                "The variabilities to include.  May contain one or more of the "
                "following characters: c=constant, d=discrete, f=fixed, "
                "t=tunable, u=continuous");
        po::options_description positionalOptions("Arguments");
        positionalOptions.add_options()
            ("slave-type",  po::value<std::string>(),
                "The name of the slave type whose variables are to be listed.");
        po::positional_options_description positions;
        positions.add("slave-type", 1);
        const auto argValues = dsb::util::ParseArguments(
            args, options, positionalOptions, positions,
            std::cerr,
            self + " ls-vars",
            "Prints a list of variables for one slave type.");
        if (!argValues) return 0;

        if (!argValues->count("slave-type")) throw std::runtime_error("Slave type name not specified");
        const auto slaveType =     (*argValues)["slave-type"].as<std::string>();

        const auto causalities =   (*argValues)["causality"].as<std::string>();
        const auto networkInterface = dsb::net::ip::Address{
            (*argValues)["interface"].as<std::string>()};
        const bool longForm =      (*argValues).count("long") > 0;
        const auto discoveryPort = dsb::net::ip::Port{
            (*argValues)["port"].as<std::uint16_t>()};
        const auto types =         (*argValues)["type"].as<std::string>();
        const auto variabilities = (*argValues)["variability"].as<std::string>();

        auto providers = dsb::master::ProviderCluster{
            networkInterface,
            discoveryPort};

        // TODO: Handle this waiting more elegantly, e.g. wait until all required
        // slave types are available.  Also, the waiting time is related to the
        // slave provider heartbeat time.
        std::this_thread::sleep_for(std::chrono::seconds(2));

        const auto slaveTypes = providers.GetSlaveTypes(std::chrono::seconds(1));
        const auto it = std::find_if(slaveTypes.begin(), slaveTypes.end(),
            [&](const dsb::master::ProviderCluster::SlaveType& s) {
                return s.description.Name() == slaveType;
            });
        if (it == slaveTypes.end()) {
            throw std::runtime_error("Unknown slave type: " + slaveType);
        }
        // `it` is now an iterator which points to the slave type.

        // Create mappings from enums to characters specified in options.
        std::map<dsb::model::DataType, char> typeChar;
        typeChar[dsb::model::REAL_DATATYPE   ] = 'r';
        typeChar[dsb::model::INTEGER_DATATYPE] = 'i';
        typeChar[dsb::model::BOOLEAN_DATATYPE] = 'b';
        typeChar[dsb::model::STRING_DATATYPE ] = 's';
        std::map<dsb::model::Causality, char> causalityChar;
        causalityChar[dsb::model::PARAMETER_CAUSALITY           ] = 'p';
        causalityChar[dsb::model::CALCULATED_PARAMETER_CAUSALITY] = 'c';
        causalityChar[dsb::model::INPUT_CAUSALITY               ] = 'i';
        causalityChar[dsb::model::OUTPUT_CAUSALITY              ] = 'o';
        causalityChar[dsb::model::LOCAL_CAUSALITY               ] = 'l';
        std::map<dsb::model::Variability, char> variabilityChar;
        variabilityChar[dsb::model::CONSTANT_VARIABILITY  ] = 'c';
        variabilityChar[dsb::model::FIXED_VARIABILITY     ] = 'f';
        variabilityChar[dsb::model::TUNABLE_VARIABILITY   ] = 't';
        variabilityChar[dsb::model::DISCRETE_VARIABILITY  ] = 'd';
        variabilityChar[dsb::model::CONTINUOUS_VARIABILITY] = 'u';

        // Finally, list the variables
        for (const auto& v : it->description.Variables()) {
            const auto vt = typeChar.at(v.DataType());
            const auto vc = causalityChar.at(v.Causality());
            const auto vv = variabilityChar.at(v.Variability());
            if (Contains(types, vt) && Contains(causalities, vc)
                && Contains(variabilities, vv))
            {
                std::cout << v.Name();
                if (longForm) {
                    std::cout << ' ' << vt << vc << vv;
                }
                std::cout << '\n';
            }
        }
    } catch (const std::runtime_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}


int Info(const std::vector<std::string>& args)
{
    try {
        namespace po = boost::program_options;
        po::options_description options("Options");
        options.add_options()
            ("interface", po::value<std::string>()->default_value(DEFAULT_NETWORK_INTERFACE),
                "The IP address or (OS-specific) name of the network interface to "
                "use for network communications, or \"*\" for all/any.")
            ("port", po::value<std::uint16_t>()->default_value(DEFAULT_DISCOVERY_PORT),
                "The UDP port used to listen for slave providers.");
        po::options_description positionalOptions("Arguments");
        positionalOptions.add_options()
            ("slave-type",  po::value<std::string>(), "A slave type name");
        po::positional_options_description positions;
        positions.add("slave-type", 1);
        const auto argValues = dsb::util::ParseArguments(
            args, options, positionalOptions, positions,
            std::cerr,
            self + " info",
            "Shows detailed information about a slave type.");
        if (!argValues) return 0;

        if (!argValues->count("slave-type")) throw std::runtime_error("Slave type name not specified");
        const auto slaveType = (*argValues)["slave-type"].as<std::string>();
        const auto networkInterface = dsb::net::ip::Address{
            (*argValues)["interface"].as<std::string>()};
        const auto discoveryPort = dsb::net::ip::Port{
            (*argValues)["port"].as<std::uint16_t>()};

        auto providers = dsb::master::ProviderCluster{
            networkInterface,
            discoveryPort};

        // TODO: Handle this waiting more elegantly, e.g. wait until all required
        // slave types are available.  Also, the waiting time is related to the
        // slave provider heartbeat time.
        std::cout << "Looking for slave providers..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));

        const auto slaveTypes = providers.GetSlaveTypes(std::chrono::seconds(1));
        const auto it = std::find_if(slaveTypes.begin(), slaveTypes.end(),
            [&](const dsb::master::ProviderCluster::SlaveType& s) {
                return s.description.Name() == slaveType; });
        if (it == slaveTypes.end()) {
            throw std::runtime_error("Unknown slave type: " + slaveType);
        }
        std::cout << "\nname " << it->description.Name() << '\n'
                  << "uuid " << it->description.UUID() << '\n'
                  << "description " << it->description.Description() << '\n'
                  << "author " << it->description.Author() << '\n'
                  << "version " << it->description.Version() << '\n'
                  << "parameters {\n";
        for (const auto& v : it->description.Variables()) {
            if (v.Causality() == dsb::model::PARAMETER_CAUSALITY) {
                std::cout << "  " << v.Name() << "\n";
            }
        }
        std::cout << "}\ninputs {\n";
        for (const auto& v : it->description.Variables()) {
            if (v.Causality() == dsb::model::INPUT_CAUSALITY) {
                std::cout << "  " << v.Name() << "\n";
            }
        }
        std::cout << "}\noutputs {\n";
        for (const auto& v : it->description.Variables()) {
            if (v.Causality() == dsb::model::OUTPUT_CAUSALITY) {
                std::cout << "  " << v.Name() << "\n";
            }
        }
        std::cout << "}\nproviders {\n";
        for (const auto& p : it->providers) {
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
#ifdef DSB_LOG_TRACE_ENABLED
    dsb::log::SetLevel(dsb::log::trace);
#elif defined(DSB_LOG_DEBUG_ENABLED)
    dsb::log::SetLevel(dsb::log::debug);
#endif

    if (argc < 2) {
        std::cerr <<
            "Execution master (DSB v" DSB_VERSION_STRING ")\n\n"
            "This program will connect to the network and obtain information about\n"
            "available slave types, and can be used to run simple simulations.\n\n"
            "Usage:\n"
            "  " << self << " <command> [command-specific args]\n\n"
            "Commands:\n"
            "  info     Shows detailed information about one slave type\n"
            "  list     Lists available slave types\n"
            "  ls-vars  Lists information about a slave type's variables\n"
            "  run    Runs a simulation\n"
            "\n"
            "Run <command> without any additional arguments for more specific help.\n";
        return 0;
    }
    const auto command = std::string(argv[1]);
    const auto args = dsb::util::CommandLine(argc-2, argv+2);
    try {
        if (command == "run") return Run(args);
        else if (command == "list") return List(args);
        else if (command == "ls-vars") return LsVars(args);
        else if (command == "info") return Info(args);
        else {
            std::cerr << "Error: Invalid command: " << command;
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: Unexpected internal error: " << e.what() << std::endl;
        return 255;
    }
}
