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
#include "dsb/compat_helpers.hpp"
#include "dsb/protobuf.hpp"
#include "dsb/proxy.hpp"
#include "dsb/util.hpp"
#include "p2p_proxy.hpp"

#include "broker.pb.h"


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
        int& backendPort,
        boost::chrono::milliseconds silenceTimeout)
    {
        auto fe = zmq::socket_t(*context, frontendType);
        auto be = zmq::socket_t(*context, backendType);
        const auto fep = EndpointPort(BindToEphemeralPort(fe));
        const auto bep = EndpointPort(BindToEphemeralPort(be));
        auto p = dsb::proxy::SpawnProxy(context, std::move(fe), std::move(be), silenceTimeout);
        //----- No exceptions may be thrown below this line -----
        frontendPort = fep;
        backendPort = bep;
        return p;
    }

    class ExecutionBroker
    {
    public:
        ExecutionBroker(
            std::shared_ptr<zmq::context_t> context,
            boost::chrono::seconds commTimeout)
            : m_masterControlPort(-1), m_slaveControlPort(-1),
              m_dataPubPort(-1), m_dataSubPort(-1),
              m_control(EphemeralProxy(context, ZMQ_DEALER, ZMQ_ROUTER,
                                       m_masterControlPort, m_slaveControlPort,
                                       commTimeout)),
              m_data(EphemeralProxy(context, ZMQ_XSUB, ZMQ_XPUB,
                                    m_dataSubPort, m_dataPubPort, commTimeout))
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
        int m_dataPubPort;
        int m_dataSubPort;

        dsb::proxy::Proxy m_control;
        dsb::proxy::Proxy m_data;
    };
}


int main(int argc, const char** argv)
{
    const int basePort = argc > 1 ? std::atoi(argv[1]) : 51380;
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
                dsbproto::broker::SpawnExecutionData seData;
                dsb::protobuf::ParseFromFrame(msg[1], seData);
                const auto execName = seData.execution_name();
                const auto commTimeout = boost::chrono::seconds(seData.comm_timeout_seconds());
                if (executionBrokers.count(execName)) {
                    throw std::runtime_error("Execution name already in use: " + execName);
                }
                auto b = executionBrokers.insert(
                    std::make_pair(execName, ExecutionBroker(context, commTimeout))
                    ).first;
                msg.clear();
                msg.push_back(dsb::comm::ToFrame("SPAWN_EXECUTION_OK"));
                msg.push_back(zmq::message_t());
                dsbproto::broker::SpawnExecutionOkData seOkData;
                seOkData.set_master_port(b->second.ControlPorts().first);
                seOkData.set_slave_port(b->second.ControlPorts().second);
                seOkData.set_variable_pub_port(b->second.DataPorts().first);
                seOkData.set_variable_sub_port(b->second.DataPorts().second);
                dsb::protobuf::SerializeToFrame(seOkData, msg[1]);

                // TODO: Remove later
                std::clog << "Started broker for execution \"" << execName << "\" using the following ports:\n"
                          << "  control (master): " << b->second.ControlPorts().first << '\n'
                          << "  control (slaves): " << b->second.ControlPorts().second << '\n'
                          << "  data (subscribe): " << b->second.DataPorts().first << '\n'
                          << "  data   (publish): " << b->second.DataPorts().second << std::endl;
            } catch (const std::runtime_error& e) {
                msg.clear();
                msg.push_back(dsb::comm::ToFrame("SPAWN_EXECUTION_FAIL"));
                msg.push_back(zmq::message_t());
                dsbproto::broker::SpawnExecutionFailData seFailData;
                seFailData.set_reason(e.what());
                dsb::protobuf::SerializeToFrame(seFailData, msg[1]);
            }
            dsb::comm::Send(executionRequest, msg);
        } else if (command == "TERMINATE_EXECUTION" && msg.size() > 1) {
            dsbproto::broker::TerminateExecutionData teData;
            dsb::protobuf::ParseFromFrame(msg[1], teData);
            const auto execID = teData.execution_name();
            if (executionBrokers.count(execID)) {
                executionBrokers.at(execID).Stop();
                executionBrokers.erase(execID);
            }
            executionRequest.send("", 0);
        }
    }
}
