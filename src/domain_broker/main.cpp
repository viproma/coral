#include "zmq.hpp"

#include <algorithm>
#include <cstdlib>
#include <deque>
#include <iostream>

#include "zmq.hpp"

#include "dsb/comm.hpp"
#include "dsb/proxy.hpp"
#include "p2p_proxy.hpp"


namespace
{
}


int main(int argc, const char** argv)
{
    const long long basePort = argc > 1 ? std::atol(argv[1]) : 51380;
    const std::string baseEndpoint = "tcp://*:";
    const auto reportMasterEndpoint = baseEndpoint + std::to_string(basePort);
    const auto reportSlavePEndpoint = baseEndpoint + std::to_string(basePort+1);
    const auto infoMasterEndpoint   = baseEndpoint + std::to_string(basePort+2);
    const auto infoSlavePEndpoint   = baseEndpoint + std::to_string(basePort+3);
    std::cout << "Domain broker binding to the following endpoints: \n"
              << "  report         (masters): " << reportMasterEndpoint << '\n'
              << "  report (slave providers): " << reportSlavePEndpoint << '\n'
              << "  info           (masters): " << infoMasterEndpoint << '\n'
              << "  info   (slave providers): " << infoSlavePEndpoint << std::endl;
        
    auto context = std::make_shared<zmq::context_t>();
    auto report = dsb::proxy::SpawnProxy(
        context,
        ZMQ_XPUB, reportMasterEndpoint,
        ZMQ_XSUB, reportSlavePEndpoint);
    auto info = dsb::domain_broker::SpawnP2PProxy(
        context,
        infoMasterEndpoint,
        infoSlavePEndpoint);

    std::cout << "Press ENTER to quit" << std::endl;
    std::cin.ignore();
    info.send("", 0);
    report.Stop();
}
