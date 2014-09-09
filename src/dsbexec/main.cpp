#include <iostream>
#include <memory>
#include <string>
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
    auto controller = dsb::execution::SpawnController(context, endpoint);

    std::cout << "Press ENTER to start simulation." << std::endl;
    std::cin.ignore();

    // Super advanced master algorithm.
    const double maxTime = 10.0;
    const double stepSize = 1.0/100.0;

    for (double time = 0.0; time < maxTime-stepSize; time += stepSize) {
        controller.Step(time, stepSize);
    }
    controller.Terminate();

    // Give ZMQ time to send all TERMINATE messages
    std::cout << "Terminated. Press ENTER to quit." << std::endl;
    std::cin.ignore();
}
