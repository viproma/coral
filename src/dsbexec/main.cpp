#include <iostream>
#include <memory>
#include <string>

#include "boost/chrono.hpp"
#include "zmq.hpp"

#include "dsb/execution.hpp"


int main(int argc, const char** argv)
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <address>\n"
                  << "  address = the DSB server endpoint (e.g. tcp://localhost:5432)"
                  << std::endl;
        return 0;
    }
    const auto endpoint = std::string(argv[1]);

    auto context = std::make_shared<zmq::context_t>();
    auto controller = dsb::execution::SpawnExecution(context, endpoint);

    // =========================================================================
    // TEMPORARY DEMO CODE
    // =========================================================================
    enum { SPRING1 = 1, MASS1 = 2, SPRING2 = 3, MASS2 = 4 };

    controller.AddSlave(SPRING1);
    controller.AddSlave(MASS1);
    dsb::execution::Variable spring1Vars[2] = {
        { 4, 2.0 }, // length
        { 1, 1.0 }  // position B
    };
    controller.SetVariables(SPRING1, dsb::sequence::ArraySequence(spring1Vars, 2));
    dsb::execution::Variable vMass1Pos = { 2, 1.0 };
    controller.SetVariables(MASS1, dsb::sequence::ArraySequence(&vMass1Pos, 1));
    dsb::execution::VariableConnection cMass1Spring1Pos = { 1, MASS1, 2 };
    controller.ConnectVariables(SPRING1, dsb::sequence::ArraySequence(&cMass1Spring1Pos, 1));
    dsb::execution::VariableConnection cSpring1Mass1Force = { 0, SPRING1, 3 };
    controller.ConnectVariables(MASS1, dsb::sequence::ArraySequence(&cSpring1Mass1Force, 1));

    controller.AddSlave(SPRING2);
    controller.AddSlave(MASS2);
    dsb::execution::Variable spring2Vars[3] = {
        { 4, 2.0 }, // length
        { 0, 1.0 }, // position A
        { 1, 3.0 }  // position B
    };
    controller.SetVariables(SPRING2, dsb::sequence::ArraySequence(spring2Vars, 3));
    dsb::execution::Variable vMass2Pos = { 2, 3.0 };
    controller.SetVariables(MASS2, dsb::sequence::ArraySequence(&vMass2Pos, 1));
    dsb::execution::VariableConnection cMass2Spring2Pos = { 1, MASS2, 2 };
    controller.ConnectVariables(SPRING2, dsb::sequence::ArraySequence(&cMass2Spring2Pos, 1));
    dsb::execution::VariableConnection cSpring2Mass2Force = { 0, SPRING2, 3 };
    controller.ConnectVariables(MASS2, dsb::sequence::ArraySequence(&cSpring2Mass2Force, 1));

    dsb::execution::VariableConnection cMass1Spring2Pos = { 0, MASS1, 2 };
    controller.ConnectVariables(SPRING2, dsb::sequence::ArraySequence(&cMass1Spring2Pos, 1));
    dsb::execution::VariableConnection cSpring2Mass1Force = { 1, SPRING2, 2 };
    controller.ConnectVariables(MASS1, dsb::sequence::ArraySequence(&cSpring2Mass1Force, 1));
    // =========================================================================

    // This is to work around "slow joiner syndrome".  It lets slaves'
    // subscriptions take effect before we start the simulation.
    std::cout << "Press ENTER to start simulation." << std::endl;
    std::cin.ignore();
    const auto t0 = boost::chrono::high_resolution_clock::now();

    // Super advanced master algorithm.
    const double maxTime = 10.0;
    const double stepSize = 1.0/100.0;
    for (double time = 0.0; time < maxTime-stepSize; time += stepSize) {
        controller.Step(time, stepSize);
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
}
