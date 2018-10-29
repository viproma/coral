/*
Copyright 2013-present, SINTEF Ocean.
This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
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

#include <boost/program_options.hpp>

#include <coral/log.hpp>
#include <coral/master.hpp>
#include <coral/util/console.hpp>

#include "config_parser.hpp"


namespace {
    const std::string self = "coralmaster";
    const std::string DEFAULT_NETWORK_INTERFACE = "127.0.0.1";
    const std::uint16_t DEFAULT_DISCOVERY_PORT = 10272;

    void PrintExecConfigHelp()
    {
        std::cout <<
            "; The execution configuration file is a simple text file consisting of keys\n"
            "; and values, where each key is separated from its value by whitespace.\n"
            "; (Specifically, it must be in the Boost INFO format; see here for more info:\n"
            "; http://www.boost.org/doc/libs/release/libs/property_tree/).  This is an\n"
            "; example file which contains all the settings currently available.\n"
            "\n"
            "; Time step size (mandatory).\n"
            "step_size 0.2\n"
            "\n"
            "; Simulation start time (optional, defaults to 0).\n"
            "start 0.0\n"
            "\n"
            "; Simulation end time (optional, defaults to \"indefinitely\").\n"
            "stop 100.0\n"
            "\n"
            "; General command/communications timeout, in milliseconds (optional,\n"
            "; defaults to 1000 ms).\n"
            ";\n"
            "; This is how long the master will wait for replies to commands sent\n"
            "; to a slave before it considers the connection to be broken, and it is\n"
            "; also how long slaves wait for data from each other between time steps.\n"
            "; It should generally be a short duration, as it is used for \"cheap\"\n"
            "; operations (i.e., everything besides the \"perform time step\" command).\n"
            "; -1 is a special value which means \"wait indefinitely\", which should\n"
            "; only be used for debugging purposes.\n"
            "comm_timeout_ms 5000\n"
            "\n"
            "; Time step timeout multiplier (optional, defaults to 100).\n"
            ";\n"
            "; This controls the amount of time the slaves get to carry out a time\n"
            "; step.  The timeout is set equal to step_timeout_multiplier` times the\n"
            "; step size, where the step size is assumed to be in seconds.\n"
            "step_timeout_multiplier 10\n"
            "\n"
            "; Slave instantiation timeout, in milliseconds (optional, defaults\n"
            "; to 30,000 ms = 30 s).\n"
            ";\n"
            "; This is the maximum amount of time that may pass from the moment the\n"
            "; instantiation command is issued to when the slave is ready for\n"
            "; simulation.  Some slaves may take a long time to instantiate, either\n"
            "; because the FMU is very large and thus takes a long time to unpack\n"
            "; or because its instantiation routine is very demanding.\n"
            "; -1 is a special value which means \"wait indefinitely\", which should\n"
            "; only be used for debugging purposes.\n"
            "instantiation_timeout_ms 10000\n";
    }

    void PrintSysConfigHelp()
    {
        std::cout <<
            "; The system configuration file is a simple text file consisting of keys\n"
            "; and values in a tree-like structure.  (Specifically, it must be in the\n"
            "; Boost INFO format; see here for more info:\n"
            "; http://www.boost.org/doc/libs/release/libs/property_tree/).  This is an\n"
            "; example file which demonstrates the available settings.\n"
            "\n"
            "; The \"slaves\" section contains a list of all the subsimulators.\n"
            "slaves {\n"
            "    ; Each subsimulator is identified by a name, in this case \"mass\" and \"spring\".\n"
            "    mass {\n"
            "        type \"1d_mass\"        ; This is the type, i.e., the model name of an FMU.\n"
            "        init {\n"
            "            mass     20.0     ; Here, one can set initial values for different variables.\n"
            "            position  3.0\n"
            "        }\n"
            "    }\n"
            "    spring {\n"
            "        type \"1d_spring\"\n"
            "        init {\n"
            "            stiffness            10.0\n"
            "            uncompressed_length  5.0\n"
            "            position_a           0.0\n"
            "        }\n"
            "    }\n"
            "}\n"
            "\n"
            "; This section contains the variable connections, on the following form:\n"
            ";     <slave A>.<input variable> <slave B>.<output variable>\n"
            "; (To make the order easier to remember, mentally insert an \"equals\" sign\n"
            "; between them.)\n"
            "connections {\n"
            "    mass.force        spring.force\n"
            "    spring.position_b mass.position\n"
            "}\n"
            "\n"
            "; This section contains parameter changes that are to take place at a\n"
            "; specific point in time. There is one subsection for each time point.\n"
            "scenario {\n"
            "    1.5 {                         ; At 1.5 seconds...\n"
            "        spring.stiffness  5.0     ; ...the spring stiffness will be changed\n"
            "        mass.mass        10.0     ; ...along with the mass.\n"
            "    }\n"
            "    4.3 {                         ; You can have as many events as you like.\n"
            "        spring.stiffness  2.4\n"
            "    }\n"
            "}\n";
    }
}


int Run(const std::vector<std::string>& args)
{
    try {
        namespace po = boost::program_options;
        po::options_description options("Options");
        options.add_options()
            ("debug-pause",
                "Wait for a user keypress after slaves have been spawned, "
                "to allow time to attach a debugger.")
            ("interface", po::value<std::string>()->default_value(DEFAULT_NETWORK_INTERFACE),
                "The IP address or (OS-specific) name of the network interface to "
                "use for network communications, or \"*\" for all/any.")
            ("name,n", po::value<std::string>()->default_value(""),
                "The execution name.  If left unspecified, a name will be created "
                "based on the current date and time.")
            ("port", po::value<std::uint16_t>()->default_value(DEFAULT_DISCOVERY_PORT),
                "The UDP port used to listen for slave providers.")
            ("realtime,r", po::value<double>()->default_value(0.0),
                "Real-time multiplier, i.e., how fast the simulation should go "
                "compared to wall clock time.  A value of 1 means that the "
                "simulation should run in real time, while e.g. 2 means twice as "
                "fast.  The default is 0, which is a special value that means "
                "\"as fast as possible\".")
            ("warnings,w",
                "Enable warnings while parsing configuration files.")
            ("help-exec-config",
                "Display a help message about the format of execution configuration files "
                "and exit.")
            ("help-sys-config",
                "Display a help message about the format of system configuration files "
                "and exit.");
        coral::util::AddLoggingOptions(options);
        po::options_description positionalOptions("Arguments");
        positionalOptions.add_options()
            ("exec-config", po::value<std::string>(),
                "Configuration file which describes the execution settings "
                "(start time, step size, etc.).")
            ("sys-config",  po::value<std::string>(),
                "Configuration file which describes the system to simulate "
                "(slaves, connections, etc.).\n");
        po::positional_options_description positions;
        positions.add("exec-config", 1)
                 .add("sys-config", 1);

        const auto argValues = coral::util::ParseArguments(
            args, options, positionalOptions, positions,
            std::cerr,
            self + " run",
            "Runs a simulation.");
        if (!argValues) return 0;
        coral::util::UseLoggingArguments(*argValues);

        if (argValues->count("help-exec-config")) {
            PrintExecConfigHelp();
            return 0;
        }
        if (argValues->count("help-sys-config")) {
            PrintSysConfigHelp();
            return 0;
        }

        if (!argValues->count("exec-config")) throw std::runtime_error("No execution configuration file specified");
        if (!argValues->count("sys-config")) throw std::runtime_error("No system configuration file specified");
        const auto execConfigFile = (*argValues)["exec-config"].as<std::string>();
        const auto sysConfigFile = (*argValues)["sys-config"].as<std::string>();
        const auto debugPause= !!argValues->count("debug-pause");
        const auto networkInterface = coral::net::ip::Address{
            (*argValues)["interface"].as<std::string>()};
        const auto execName = (*argValues)["name"].as<std::string>();
        const auto discoveryPort = coral::net::ip::Port{
            (*argValues)["port"].as<std::uint16_t>()};
        const auto realtimeMultiplier = (*argValues)["realtime"].as<double>();
        const auto warningStream = argValues->count("warnings") ? &std::clog : nullptr;

        auto providers = coral::master::ProviderCluster{
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
        coral::master::ExecutionOptions execOptions;
        execOptions.startTime                   = execConfig.startTime;
        execOptions.maxTime                     = execConfig.stopTime;
        execOptions.slaveVariableRecvTimeout    = execConfig.commTimeout;

        std::cout << "Creating new execution" << std::endl;
        auto exec = coral::master::Execution(execName, execOptions);

        std::cout << "Parsing model configuration file '" << sysConfigFile
                  << "' and spawning slaves" << std::endl;
        std::vector<SimulationEvent> unsortedScenario;
        std::function<void()> debugPauseCallback;
        if (debugPause) {
            debugPauseCallback = [] ()
            {
                std::cout << "Slave processes spawned. Press ENTER to continue. "
                            "[--debug-pause]" << std::endl;
                std::cin.ignore();
            };
        }

        ParseSystemConfig(
            sysConfigFile,
            providers,
            exec,
            unsortedScenario,
            execConfig.commTimeout,
            execConfig.instantiationTimeout,
            warningStream,
            debugPauseCallback);

        // Put the scenario events into a priority queue, in order of ascending
        // event time.
        auto eventTimeGreater = [] (const SimulationEvent& a, const SimulationEvent& b) {
            return a.timePoint > b.timePoint;
        };
        auto scenario = std::priority_queue
            <SimulationEvent, decltype(unsortedScenario), decltype(eventTimeGreater)>
            (eventTimeGreater, std::move(unsortedScenario));

        // Super advanced master algorithm.
        std::cout << "Simulation started" << std::endl;
        const auto t0 = std::chrono::high_resolution_clock::now();
        const double maxTime = execConfig.stopTime - 0.9*execConfig.stepSize;
        double nextPerc = 0.05;
        const auto stepTimeout = std::chrono::milliseconds(
            boost::numeric_cast<typename std::chrono::milliseconds::rep>(
                execConfig.stepSize * 1000 * execConfig.stepTimeoutMultiplier));

        const double clockRes = // the resolution of the clock, in secs/tick
            static_cast<double>(std::chrono::high_resolution_clock::duration::period::num)
            / std::chrono::high_resolution_clock::duration::period::den;
        auto prevRealTime = std::chrono::high_resolution_clock::now();
        auto prevSimTime = execConfig.startTime;

        auto targetWallClockTime = std::chrono::steady_clock::now();
        const double wallClockTicksPerSec =
            static_cast<double>(std::chrono::steady_clock::duration::period::den)
            / std::chrono::steady_clock::duration::period::num;
        const auto wallClockStepSize = std::chrono::steady_clock::duration(
            static_cast<std::chrono::steady_clock::duration::rep>(
                execConfig.stepSize * wallClockTicksPerSec / realtimeMultiplier));
        if (realtimeMultiplier > 0.0) {
            CORAL_LOG_DEBUG(boost::format("Real-time step size is %d microseconds")
                % std::chrono::duration_cast<std::chrono::microseconds>(
                    wallClockStepSize).count());
        }

        for (double time = execConfig.startTime;
             time < maxTime;
             time += execConfig.stepSize)
        {
            if (!scenario.empty() && scenario.top().timePoint <= time) {
                std::vector<coral::master::SlaveConfig> settings;
                std::map<coral::model::SlaveID, std::size_t> indexes;
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
                        coral::model::VariableSetting(
                            scenEvent.variable,
                            scenEvent.newValue));
                    scenario.pop();
                }
                exec.Reconfigure(settings, execConfig.commTimeout);
            }
            if (exec.Step(execConfig.stepSize, stepTimeout) != coral::master::StepResult::completed) {
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

            if (realtimeMultiplier > 0.0) {
                targetWallClockTime += wallClockStepSize;
                std::this_thread::sleep_until(targetWallClockTime);
            }
        }

        // Termination
        const auto t1 = std::chrono::high_resolution_clock::now();
        const auto simTime = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);
        std::cout << "Completed in " << simTime.count() << " ms." << std::endl;
        exec.Terminate();
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
        coral::util::AddLoggingOptions(options);
        const auto argValues = coral::util::ParseArguments(
            args,
            options,
            po::options_description("Arguments"),
            po::positional_options_description(),
            std::cerr,
            self + " list",
            "Lists the slave types that are available on the network.");
        if (!argValues) return 0;
        coral::util::UseLoggingArguments(*argValues);
        const auto networkInterface = coral::net::ip::Address{
            (*argValues)["interface"].as<std::string>()};
        const auto discoveryPort = coral::net::ip::Port{
            (*argValues)["port"].as<std::uint16_t>()};

        auto providers = coral::master::ProviderCluster{
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
            //CORAL_LOG_TRACE(st.description.Name());
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
                "l=local, o=output, p=parameter.")
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
                "following characters: b=boolean, i=integer, r=real, s=string.")
            ("variability,v", po::value<std::string>()->default_value("cdftu"),
                "The variabilities to include.  May contain one or more of the "
                "following characters: c=constant, d=discrete, f=fixed, "
                "t=tunable, u=continuous.");
        coral::util::AddLoggingOptions(options);
        po::options_description positionalOptions("Arguments");
        positionalOptions.add_options()
            ("slave-type",  po::value<std::string>(),
                "The name of the slave type whose variables are to be listed.");
        po::positional_options_description positions;
        positions.add("slave-type", 1);
        const auto argValues = coral::util::ParseArguments(
            args, options, positionalOptions, positions,
            std::cerr,
            self + " ls-vars",
            "Prints a list of variables for one slave type.");
        if (!argValues) return 0;
        coral::util::UseLoggingArguments(*argValues);

        if (!argValues->count("slave-type")) throw std::runtime_error("Slave type name not specified");
        const auto slaveType =     (*argValues)["slave-type"].as<std::string>();

        const auto causalities =   (*argValues)["causality"].as<std::string>();
        const auto networkInterface = coral::net::ip::Address{
            (*argValues)["interface"].as<std::string>()};
        const bool longForm =      (*argValues).count("long") > 0;
        const auto discoveryPort = coral::net::ip::Port{
            (*argValues)["port"].as<std::uint16_t>()};
        const auto types =         (*argValues)["type"].as<std::string>();
        const auto variabilities = (*argValues)["variability"].as<std::string>();

        auto providers = coral::master::ProviderCluster{
            networkInterface,
            discoveryPort};

        // TODO: Handle this waiting more elegantly, e.g. wait until all required
        // slave types are available.  Also, the waiting time is related to the
        // slave provider heartbeat time.
        std::this_thread::sleep_for(std::chrono::seconds(2));

        const auto slaveTypes = providers.GetSlaveTypes(std::chrono::seconds(1));
        const auto it = std::find_if(slaveTypes.begin(), slaveTypes.end(),
            [&](const coral::master::ProviderCluster::SlaveType& s) {
                return s.description.Name() == slaveType;
            });
        if (it == slaveTypes.end()) {
            throw std::runtime_error("Unknown slave type: " + slaveType);
        }
        // `it` is now an iterator which points to the slave type.

        // Create mappings from enums to characters specified in options.
        std::map<coral::model::DataType, char> typeChar;
        typeChar[coral::model::REAL_DATATYPE   ] = 'r';
        typeChar[coral::model::INTEGER_DATATYPE] = 'i';
        typeChar[coral::model::BOOLEAN_DATATYPE] = 'b';
        typeChar[coral::model::STRING_DATATYPE ] = 's';
        std::map<coral::model::Causality, char> causalityChar;
        causalityChar[coral::model::PARAMETER_CAUSALITY           ] = 'p';
        causalityChar[coral::model::CALCULATED_PARAMETER_CAUSALITY] = 'c';
        causalityChar[coral::model::INPUT_CAUSALITY               ] = 'i';
        causalityChar[coral::model::OUTPUT_CAUSALITY              ] = 'o';
        causalityChar[coral::model::LOCAL_CAUSALITY               ] = 'l';
        std::map<coral::model::Variability, char> variabilityChar;
        variabilityChar[coral::model::CONSTANT_VARIABILITY  ] = 'c';
        variabilityChar[coral::model::FIXED_VARIABILITY     ] = 'f';
        variabilityChar[coral::model::TUNABLE_VARIABILITY   ] = 't';
        variabilityChar[coral::model::DISCRETE_VARIABILITY  ] = 'd';
        variabilityChar[coral::model::CONTINUOUS_VARIABILITY] = 'u';

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
        coral::util::AddLoggingOptions(options);
        po::options_description positionalOptions("Arguments");
        positionalOptions.add_options()
            ("slave-type",  po::value<std::string>(), "A slave type name.");
        po::positional_options_description positions;
        positions.add("slave-type", 1);
        const auto argValues = coral::util::ParseArguments(
            args, options, positionalOptions, positions,
            std::cerr,
            self + " info",
            "Shows detailed information about a slave type.");
        if (!argValues) return 0;
        coral::util::UseLoggingArguments(*argValues);

        if (!argValues->count("slave-type")) throw std::runtime_error("Slave type name not specified");
        const auto slaveType = (*argValues)["slave-type"].as<std::string>();
        const auto networkInterface = coral::net::ip::Address{
            (*argValues)["interface"].as<std::string>()};
        const auto discoveryPort = coral::net::ip::Port{
            (*argValues)["port"].as<std::uint16_t>()};

        auto providers = coral::master::ProviderCluster{
            networkInterface,
            discoveryPort};

        // TODO: Handle this waiting more elegantly, e.g. wait until all required
        // slave types are available.  Also, the waiting time is related to the
        // slave provider heartbeat time.
        std::cout << "Looking for slave providers..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));

        const auto slaveTypes = providers.GetSlaveTypes(std::chrono::seconds(1));
        const auto it = std::find_if(slaveTypes.begin(), slaveTypes.end(),
            [&](const coral::master::ProviderCluster::SlaveType& s) {
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
            if (v.Causality() == coral::model::PARAMETER_CAUSALITY) {
                std::cout << "  " << v.Name() << "\n";
            }
        }
        std::cout << "}\ninputs {\n";
        for (const auto& v : it->description.Variables()) {
            if (v.Causality() == coral::model::INPUT_CAUSALITY) {
                std::cout << "  " << v.Name() << "\n";
            }
        }
        std::cout << "}\noutputs {\n";
        for (const auto& v : it->description.Variables()) {
            if (v.Causality() == coral::model::OUTPUT_CAUSALITY) {
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
    if (argc < 2) {
        std::cerr <<
            "Execution master (" CORAL_PROGRAM_NAME_VERSION ")\n\n"
            "This program will connect to the network and obtain information about\n"
            "available slave types, and can be used to run simple simulations.\n\n"
            "Usage:\n"
            "  " << self << " <command> [command-specific args]\n\n"
            "Commands:\n"
            "  info     Shows detailed information about one slave type.\n"
            "  list     Lists available slave types.\n"
            "  ls-vars  Lists information about a slave type's variables.\n"
            "  run      Runs a simulation.\n"
            "\n"
            "Run \"" << self << " <command> --help\" for command-specific information.\n";
        return 0;
    }
    const auto command = std::string(argv[1]);
    const auto args = coral::util::CommandLine(argc-2, argv+2);
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
