#include <algorithm>
#include <cassert>
#include <chrono>
#include <iostream>
#include <queue>
#include <map>
#include <string>
#include <thread>

#include "boost/foreach.hpp"
#include "boost/program_options.hpp"

#include "dsb/domain/controller.hpp"
#include "dsb/execution/controller.hpp"
#include "config_parser.hpp"


namespace {
    const char* self = "dsbexec";
}

dsb::net::DomainLocator MakeDomainLocator(const std::string& address)
{
    return dsb::net::GetDomainEndpoints(address);
}

int Test(int argc, const char** argv)
{
    assert (argc > 1 && std::string(argv[1]) == "test");
    --argc; ++argv;

    const auto address = "tcp://localhost";
    const auto domainLoc = MakeDomainLocator(address);
    auto domain = dsb::domain::Controller(domainLoc);

    // TODO: Handle this waiting more elegantly, e.g. wait until all required
    // slave types are available.  Also, the waiting time is related to the
    // slave provider heartbeat time.
    std::cout << "Connected to domain; waiting for data from slave providers..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));

    const auto execLoc = dsb::execution::SpawnExecution(
        domainLoc, std::string(), std::chrono::seconds(60));
    auto exec = dsb::execution::Controller(execLoc);

    try {
        exec.BeginConfig();
        exec.EndConfig();
        exec.SetSimulationTime(0, 1);


        exec.Terminate();
    } catch (const std::logic_error& e) {
        std::cerr << "logic_error: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "exception: " << e.what() << std::endl;
    }
    std::cout << "Done. Press ENTER to quit" << std::endl;
    std::cin.ignore();
    return 0;
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
                      << optDesc << "\n"
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
                "      instantiation_timeout_ms 10000\n";
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
        std::this_thread::sleep_for(std::chrono::seconds(2));

        std::cout << "Parsing execution configuration file '" << execConfigFile
                  << "'" << std::endl;
        const auto execConfig = ParseExecutionConfig(execConfigFile);

        std::cout << "Spawning new execution broker" << std::endl;
        const auto execLoc = dsb::execution::SpawnExecution(
            domainLoc, execName, execConfig.slaveTimeout);
        const auto execSpawnTime = std::chrono::high_resolution_clock::now();
        auto exec = dsb::execution::Controller(execLoc);
        exec.SetSimulationTime(execConfig.startTime, execConfig.stopTime);

        std::cout << "Parsing model configuration file '" << sysConfigFile
                  << "' and spawning slaves" << std::endl;
        std::vector<SimulationEvent> unsortedScenario;
        ParseSystemConfig(sysConfigFile, domain, exec, execLoc, unsortedScenario,
                          execConfig.commTimeout, execConfig.instantiationTimeout,
                          &std::clog);

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
        std::cout << "Waiting for slaves..." << std::endl;
        exec.EndConfig();
        std::cout << "All slaves are present. Press ENTER to start simulation." << std::endl;
        std::cin.ignore();
        const auto t0 = std::chrono::high_resolution_clock::now();
        if (t0 - execSpawnTime > execConfig.slaveTimeout) {
            throw std::runtime_error("Communications timeout reached");
        }

        // Super advanced master algorithm.
        const double maxTime = execConfig.stopTime - 0.9*execConfig.stepSize;
        double nextPerc = 0.05;
        const auto stepTimeout = std::chrono::seconds(
            boost::numeric_cast<typename std::chrono::seconds::rep>(
                execConfig.stepSize * execConfig.stepTimeoutMultiplier));

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
                exec.BeginConfig();
                std::map<dsb::model::SlaveID, std::vector<dsb::model::VariableSetting>>
                    varChanges;
                while (!scenario.empty() && scenario.top().timePoint <= time) {
                    varChanges[scenario.top().slave].push_back(
                        dsb::model::VariableSetting(
                            scenario.top().variable,
                            scenario.top().newValue));
                    scenario.pop();
                }
                for (auto it = begin(varChanges); it != end(varChanges); ++it) {
                    exec.SetVariables(it->first, it->second, execConfig.commTimeout);
                }
                exec.EndConfig();
            }
            if (exec.Step(execConfig.stepSize, stepTimeout) != dsb::execution::STEP_COMPLETE) {
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
        std::this_thread::sleep_for(std::chrono::seconds(2));

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


// Helper function which returns whether s contains c
bool Contains(const std::string& s, char c)
{
    return s.find(c) < s.size();
}


int LsVars(int argc, const char** argv)
{
    assert (argc > 1 && std::string(argv[1]) == "ls-vars");
    // Drop the first element (program name) to use "ls-vars" as the "program name".
    --argc; ++argv;

    try {
        namespace po = boost::program_options;
        po::options_description optDesc("Options");
        optDesc.add_options()
            ("help", "Display help message")
            ("type,t", po::value<std::string>()->default_value("birs"),
                "The data type(s) to include.  May contain one or more of the "
                "following characters: b=boolean, i=integer, r=real, s=string")
            ("causality,c", po::value<std::string>()->default_value("cilop"),
                "The causalities to include.  May contain one or more of the "
                "following characters: c=calculated parameter, i=input, "
                "l=local, o=output, p=parameter")
            ("variability,v", po::value<std::string>()->default_value("cdftu"),
                "The variabilities to include.  May contain one or more of the "
                "following characters: c=constant, d=discrete, f=fixed, "
                "t=tunable, u=continuous")
            ("long,l",
                "\"Long\" format.  Shows type, causality and variability as a "
                "3-character string after the variable name.");
        po::options_description argDesc;
        argDesc.add(optDesc);
        argDesc.add_options()
            ("domain",      po::value<std::string>(), "The address of the domain")
            ("slave-type",  po::value<std::string>(), "A slave type name");
        po::positional_options_description posArgDesc;
        posArgDesc.add("domain", 1)
                  .add("slave-type", 1);
        po::variables_map optMap;
        po::store(po::command_line_parser(argc, argv).options(argDesc)
                                                     .positional(posArgDesc)
                                                     .run(),
                  optMap);

        if (argc < 2 || optMap.count("help")) {
            std::cerr <<
                "Prints a list of variables for one slave type.\n\n"
                "Usage:\n"
                "  " << self << " ls-vars <domain> <slave type> [options...]\n\n"
                "Arguments:\n"
                "  domain       = The domain address, of the form \"tcp://hostname:port\",\n"
                "                 where the \":port\" part is only required if a nonstandard\n"
                "                 port is used.\n"
                "  slave type   = The name of the slave type whose variables are to\n"
                "                 be listed\n"
                "\n"
                << optDesc;
            return 0;
        }
        if (!optMap.count("domain")) throw std::runtime_error("Domain address not specified");
        if (!optMap.count("slave-type")) throw std::runtime_error("Slave type name not specified");

        const auto address = optMap["domain"].as<std::string>();
        const auto slaveType = optMap["slave-type"].as<std::string>();

        const auto types = optMap["type"].as<std::string>();
        const auto causalities = optMap["causality"].as<std::string>();
        const auto variabilities = optMap["variability"].as<std::string>();
        const bool longForm = optMap.count("long") > 0;

        // Now we have read all the command-line arguments, connect to the
        // domain and find the slave type
        const auto domainLoc = MakeDomainLocator(address);
        auto domain = dsb::domain::Controller(domainLoc);

        // TODO: Handle this waiting more elegantly, e.g. wait until all required
        // slave types are available.  Also, the waiting time is related to the
        // slave provider heartbeat time.
        std::this_thread::sleep_for(std::chrono::seconds(2));

        const auto slaveTypes = domain.GetSlaveTypes();
        const auto it = std::find_if(slaveTypes.begin(), slaveTypes.end(),
            [&](const dsb::domain::Controller::SlaveType& s) { return s.name == slaveType; });
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
        BOOST_FOREACH (const auto& v, it->variables) {
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
        std::this_thread::sleep_for(std::chrono::seconds(2));

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
            "  info     Shows detailed information about one slave type\n"
            "  list     Lists available slave types\n"
            "  ls-vars  Lists information about a slave type's variables\n"
            "  run    Runs a simulation\n"
            "\n"
            "Run <command> without any additional arguments for more specific help.\n";
        return 0;
    }
    const auto command = std::string(argv[1]);
    try {
        if (command == "run") return Run(argc, argv);
        else if (command == "list") return List(argc, argv);
        else if (command == "ls-vars") return LsVars(argc, argv);
        else if (command == "info") return Info(argc, argv);
        else if (command == "test") return Test(argc, argv);
        else {
            std::cerr << "Error: Invalid command: " << command;
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: Unexpected internal error: " << e.what() << std::endl;
        return 255;
    }
}
