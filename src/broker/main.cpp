#include "zmq.hpp"

#include <algorithm>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <map>
#include <utility>

#include "zmq.hpp"

#include "dsb/config.h"
#include "dsb/comm.hpp"
#include "dsb/proxy.hpp"
#include "dsb/util.hpp"
#include "p2p_proxy.hpp"


namespace
{
    std::string BindToEphemeralPort(
        zmq::socket_t& socket,
        const std::string& networkInterface = "*")
    {
        const auto endpoint = "tcp://" + networkInterface + ":*";
        socket.bind(endpoint.c_str());
        return dsb::comm::LastEndpoint(socket);
    }

    int EndpointPort(const std::string& endpoint)
    {
        const size_t colonPos = endpoint.rfind(':');
        if (colonPos > endpoint.size() - 1) {
            throw std::invalid_argument("Invalid endpoint specification: " + endpoint);
        }
        return std::stoi(endpoint.substr(colonPos + 1));
    }

    dsb::proxy::Proxy EphemeralProxy(
        std::shared_ptr<zmq::context_t> context,
        int frontendType,
        int backendType,
        int& frontendPort,
        int& backendPort)
    {
        auto fe = zmq::socket_t(*context, frontendType);
        auto be = zmq::socket_t(*context, backendType);
        const auto fep = EndpointPort(BindToEphemeralPort(fe));
        const auto bep = EndpointPort(BindToEphemeralPort(be));
        auto p = dsb::proxy::SpawnProxy(context, std::move(fe), std::move(be));
        //----- No exceptions may be thrown below this line -----
        frontendPort = fep;
        backendPort = bep;
        return p;
    }

    class ExecutionBroker
    {
    public:
        ExecutionBroker(std::shared_ptr<zmq::context_t> context)
            : m_masterControlPort(-1), m_slaveControlPort(-1),
              m_dataSubPort(-1), m_dataPubPort(-1),
              m_control(EphemeralProxy(context, ZMQ_DEALER, ZMQ_ROUTER, m_masterControlPort, m_slaveControlPort)),
              m_data(EphemeralProxy(context, ZMQ_XSUB, ZMQ_XPUB, m_dataSubPort, m_dataPubPort))
        {
        }

        ExecutionBroker(ExecutionBroker&& other)
            : m_masterControlPort(std::move(other.m_masterControlPort)),
              m_slaveControlPort(std::move(other.m_slaveControlPort)),
              m_dataPubPort(std::move(other.m_dataPubPort)),
              m_dataSubPort(std::move(other.m_dataSubPort)),
              m_control(std::move(other.m_control)),
              m_data(std::move(other.m_data))
        {
        }

        std::pair<int, int> ControlPorts() const
        {
            return std::make_pair(m_masterControlPort, m_slaveControlPort);
        }

        std::pair<int, int> DataPorts() const
        {
            return std::make_pair(m_dataSubPort, m_dataPubPort);
        }

        void Stop()
        {
            m_control.Stop();
            m_data.Stop();
        }
    private:
        int m_masterControlPort;
        int m_slaveControlPort;
        int m_dataSubPort;
        int m_dataPubPort;

        dsb::proxy::Proxy m_control;
        dsb::proxy::Proxy m_data;
    };
}


int main(int argc, const char** argv)
{
    const long long basePort = argc > 1 ? std::atol(argv[1]) : 51380;
    const std::string baseEndpoint = "tcp://*:";
    const auto reportMasterEndpoint = baseEndpoint + std::to_string(basePort);
    const auto reportSlavePEndpoint = baseEndpoint + std::to_string(basePort+1);
    const auto infoMasterEndpoint   = baseEndpoint + std::to_string(basePort+2);
    const auto infoSlavePEndpoint   = baseEndpoint + std::to_string(basePort+3);
    const auto execReqEndpoint      = baseEndpoint + std::to_string(basePort+4);
    std::cout << "Domain broker binding to the following endpoints: \n"
              << "  report         (masters): " << reportMasterEndpoint << '\n'
              << "  report (slave providers): " << reportSlavePEndpoint << '\n'
              << "  info           (masters): " << infoMasterEndpoint << '\n'
              << "  info   (slave providers): " << infoSlavePEndpoint << '\n'
              << "  exec. request  (masters): " << execReqEndpoint << std::endl;

    auto context = std::make_shared<zmq::context_t>();
    auto report = dsb::proxy::SpawnProxy(
        context,
        ZMQ_XPUB, reportMasterEndpoint,
        ZMQ_XSUB, reportSlavePEndpoint);
    auto info = dsb::domain_broker::SpawnP2PProxy(
        context,
        infoMasterEndpoint,
        infoSlavePEndpoint);

    auto executionRequest = zmq::socket_t(*context, ZMQ_REP);
    executionRequest.bind(execReqEndpoint.c_str());
    std::map<std::string, ExecutionBroker> executionBrokers;
    for (;;) {
        std::deque<zmq::message_t> msg;
        dsb::comm::Receive(executionRequest, msg);
        const auto command = dsb::comm::ToString(msg.front());
        if (command == "SPAWN_EXECUTION" && msg.size() > 1) {
            try {
                const auto execName = dsb::comm::ToString(msg[1]);
                if (executionBrokers.count(execName)) {
                    throw std::runtime_error("Execution name already in use: " + execName);
                }
#ifdef DSB_USE_MSVC_EMPLACE_WORKAROUND
                auto b = executionBrokers.emplace(std::make_pair(execName, ExecutionBroker(context))).first;
#else
                auto b = executionBrokers.emplace(execName, ExecutionBroker(context)).first;
#endif
                msg.clear();
                msg.push_back(dsb::comm::ToFrame("SPAWN_EXECUTION_OK"));
                msg.push_back(dsb::comm::ToFrame(std::to_string((long long) b->second.ControlPorts().first)));
                msg.push_back(dsb::comm::ToFrame(std::to_string((long long) b->second.ControlPorts().second)));
                msg.push_back(dsb::comm::ToFrame(std::to_string((long long) b->second.DataPorts().first)));
                msg.push_back(dsb::comm::ToFrame(std::to_string((long long) b->second.DataPorts().second)));

                // TODO: Remove later
                std::clog << "Started broker for execution \"" << execName << "\" using the following ports:\n"
                          << "  control (master): " << b->second.ControlPorts().first << '\n'
                          << "  control (slaves): " << b->second.ControlPorts().second << '\n'
                          << "  data (subscribe): " << b->second.DataPorts().first << '\n'
                          << "  data   (publish): " << b->second.DataPorts().second << std::endl;
            } catch (const std::runtime_error& e) {
                msg.clear();
                msg.push_back(dsb::comm::ToFrame("SPAWN_EXECUTION_FAIL"));
                msg.push_back(dsb::comm::ToFrame(e.what()));
            }
            dsb::comm::Send(executionRequest, msg);
        } else if (command == "TERMINATE_EXECUTION" && msg.size() > 1) {
            const auto execID = dsb::comm::ToString(msg[1]);
            if (executionBrokers.count(execID)) {
                executionBrokers.at(execID).Stop();
                executionBrokers.erase(execID);
                std::clog << "Broker for execution \"" << execID << "\" stopped" << std::endl;
            }
            executionRequest.send("", 0);
        }
    }
}
