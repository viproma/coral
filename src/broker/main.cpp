#include "zmq.hpp"

#include <cstdlib>
#include <iostream>

#include "proxy.hpp"


int main(int argc, const char** argv)
{
    const long long basePort = argc > 1 ? std::atol(argv[1]) : 51390;
    const std::string baseEndpoint = "tcp://*:";
    const auto controlMasterEndpoint = baseEndpoint + std::to_string(basePort);
    const auto controlSlavesEndpoint = baseEndpoint + std::to_string(basePort+1);
    const auto dataSubEndpoint       = baseEndpoint + std::to_string(basePort+2);
    const auto dataPubEndpoint       = baseEndpoint + std::to_string(basePort+3);
    std::cout << "Binding to the following endpoints: \n"
              << "  control (master): " << controlMasterEndpoint << '\n'
              << "  control (slaves): " << controlSlavesEndpoint << '\n'
              << "  data (subscribe): " << dataSubEndpoint << '\n'
              << "  data   (publish): " << dataPubEndpoint << std::endl;
        
    auto context = std::make_shared<zmq::context_t>();
    auto control = dsb::broker::SpawnProxy(
        context,
        ZMQ_DEALER, controlMasterEndpoint,
        ZMQ_ROUTER, controlSlavesEndpoint);
    auto data = dsb::broker::SpawnProxy(
        context,
        ZMQ_XSUB, dataSubEndpoint,
        ZMQ_XPUB, dataPubEndpoint);

    std::cout << "Press ENTER to shut down" << std::endl;
    std::cin.ignore();
}
