#include <iostream>
#include <memory>
#include <string>

#include "boost/chrono.hpp"
#include "boost/foreach.hpp"
#include "zmq.hpp"

#include "dsb/domain/controller.hpp"
#include "dsb/execution/controller.hpp"
#include "config_parser.hpp"


namespace {
    const char* self = "dsbexec";
}


int main(int argc, const char** argv)
{
    if (argc < 4) {
        std::cerr << "Usage: " << self << " <exec. config> <sys. config> <report> <info>\n"
                  << "  exec. config = the execution configuration file\n"
                  << "  sys. config  = the system configuration file\n"
                  << "  address      = address of the domain/execution brokers (e.g. tcp://localhost)"
                  << std::endl;
        return 0;
    }
    try {
        const auto execConfigFile = std::string(argv[1]);
        const auto sysConfigFile = std::string(argv[2]);
        const auto address = std::string(argv[3]);

        const auto reportEndpoint = address + ":51380";
        const auto infoEndpoint = address + ":51382";
        const auto execLoc = dsb::execution::Locator(
            address + ":51390",
            address + ":51391",
            address + ":51392",
            address + ":51393");

        auto context = std::make_shared<zmq::context_t>();
        auto domain = dsb::domain::Controller(context, reportEndpoint, infoEndpoint);

        auto controller = dsb::execution::SpawnExecution(context, execLoc.MasterEndpoint());
        const auto execConfig = ParseExecutionConfig(execConfigFile);
        ParseSystemConfig(sysConfigFile, controller);

        std::cout << "Press ENTER to run simulation, 'R' to refresh slave list, or enter slave type UUID to start a slave" << std::endl;
        dsb::model::SlaveID slaveID = 0;
        for (;;) {
            std::string command;
            std::getline(std::cin, command);
            if (command.empty()) break;
            else if (command == "R" || command == "r") {
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
            } else {
                std::cout << "'" << command << "' interpreted as UUID, starting slave" << std::endl;
                try {
                    domain.InstantiateSlave(command, execLoc, ++slaveID);
                } catch (const std::exception& e) {
                    std::cerr << "Instantiation failed: " << e.what() << std::endl;
                }
            }
        }

        // =========================================================================
        // TEMPORARY DEMO CODE
        // =========================================================================
        /*
        enum { SPRING1 = 1, MASS1 = 2, SPRING2 = 3, MASS2 = 4 };

        controller.AddSlave(SPRING1);
        controller.AddSlave(MASS1);
        dsb::types::Variable spring1Vars[2] = {
            { 4, 2.0 }, // length
            { 1, 1.0 }  // position B
        };
        controller.SetVariables(SPRING1, dsb::sequence::ElementsOf(spring1Vars, 2));
        dsb::types::Variable vMass1Pos = { 2, 1.0 };
        controller.SetVariables(MASS1, dsb::sequence::Only(vMass1Pos));
        dsb::types::VariableConnection cMass1Spring1Pos = { 1, MASS1, 2 };
        controller.ConnectVariables(SPRING1, dsb::sequence::Only(cMass1Spring1Pos));
        dsb::types::VariableConnection cSpring1Mass1Force = { 0, SPRING1, 3 };
        controller.ConnectVariables(MASS1, dsb::sequence::Only(cSpring1Mass1Force));

        controller.AddSlave(SPRING2);
        controller.AddSlave(MASS2);
        dsb::types::Variable spring2Vars[3] = {
            { 4, 2.0 }, // length
            { 0, 1.0 }, // position A
            { 1, 3.0 }  // position B
        };
        controller.SetVariables(SPRING2, dsb::sequence::ElementsOf(spring2Vars, 3));
        dsb::types::Variable vMass2Pos = { 2, 3.0 };
        controller.SetVariables(MASS2, dsb::sequence::Only(vMass2Pos));
        dsb::types::VariableConnection cMass2Spring2Pos = { 1, MASS2, 2 };
        controller.ConnectVariables(SPRING2, dsb::sequence::Only(cMass2Spring2Pos));
        dsb::types::VariableConnection cSpring2Mass2Force = { 0, SPRING2, 3 };
        controller.ConnectVariables(MASS2, dsb::sequence::Only(cSpring2Mass2Force));

        dsb::types::VariableConnection cMass1Spring2Pos = { 0, MASS1, 2 };
        controller.ConnectVariables(SPRING2, dsb::sequence::Only(cMass1Spring2Pos));
        dsb::types::VariableConnection cSpring2Mass1Force = { 1, SPRING2, 2 };
        controller.ConnectVariables(MASS1, dsb::sequence::Only(cSpring2Mass1Force));

        dsb::types::VariableConnection cMass1Spring2Pos = { 0, MASS1, 2 };
        controller.ConnectVariables(SPRING2, dsb::sequence::Only(cMass1Spring2Pos));
        dsb::types::VariableConnection cSpring2Mass1Force = { 1, SPRING2, 2 };
        controller.ConnectVariables(MASS1, dsb::sequence::Only(cSpring2Mass1Force));
        */
        // =========================================================================

        // This is to work around "slow joiner syndrome".  It lets slaves'
        // subscriptions take effect before we start the simulation.
        std::cout <<
            "Slaves may now be connected.\n"
            "Once all slaves are up and running, press ENTER to start simulation."
            << std::endl;
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
        std::cout << "Press ENTER to terminate slaves." << std::endl;
        std::cin.ignore();
        controller.Terminate();

        // Give ZMQ time to send all TERMINATE messages
        std::cout << "Terminated. Press ENTER to quit." << std::endl;
        std::cin.ignore();
    } catch (const std::runtime_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}
