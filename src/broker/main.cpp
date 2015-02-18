#include <algorithm>
#include <cassert>
#include <cstdint>
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
    dsb::proxy::Proxy EphemeralProxy(
        std::shared_ptr<zmq::context_t> context,
        int frontendType,
        int backendType,
        std::uint16_t& frontendPort,
        std::uint16_t& backendPort,
        boost::chrono::milliseconds silenceTimeout)
    {
        auto fe = zmq::socket_t(*context, frontendType);
        auto be = zmq::socket_t(*context, backendType);
        const auto fep = dsb::domain_broker::BindToEphemeralPort(fe);
        const auto bep = dsb::domain_broker::BindToEphemeralPort(be);
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
            : m_masterControlPort(0), m_slaveControlPort(0),
              m_dataPubPort(0), m_dataSubPort(0),
              m_control(EphemeralProxy(context, ZMQ_DEALER, ZMQ_ROUTER,
                                       m_masterControlPort, m_slaveControlPort,
                                       commTimeout)),
              m_data(EphemeralProxy(context, ZMQ_XSUB, ZMQ_XPUB,
                                    m_dataSubPort, m_dataPubPort, commTimeout))
        {
            assert (m_masterControlPort > 0);
            assert (m_slaveControlPort > 0);
            assert (m_dataPubPort > 0);
            assert (m_dataSubPort > 0);
        }

        ExecutionBroker(ExecutionBroker&& other) DSB_NOEXCEPT
            : m_masterControlPort(std::move(other.m_masterControlPort)),
              m_slaveControlPort(std::move(other.m_slaveControlPort)),
              m_dataPubPort(std::move(other.m_dataPubPort)),
              m_dataSubPort(std::move(other.m_dataSubPort)),
              m_control(std::move(other.m_control)),
              m_data(std::move(other.m_data))
        {
        }

        std::pair<std::uint16_t, std::uint16_t> ControlPorts() const
        {
            return std::make_pair(m_masterControlPort, m_slaveControlPort);
        }

        std::pair<std::uint16_t, std::uint16_t> DataPorts() const
        {
            return std::make_pair(m_dataSubPort, m_dataPubPort);
        }

        void Stop()
        {
            m_control.Stop();
            m_data.Stop();
        }
    private:
        std::uint16_t m_masterControlPort;
        std::uint16_t m_slaveControlPort;
        std::uint16_t m_dataPubPort;
        std::uint16_t m_dataSubPort;

        dsb::proxy::Proxy m_control;
        dsb::proxy::Proxy m_data;
    };
}


int main(int argc, const char** argv)
{
    const long long basePort = argc > 1 ? std::atol(argv[1]) : 10243;

    auto context = std::make_shared<zmq::context_t>();
    const auto execReqEndpoint = "tcp://*:" + std::to_string(basePort);
    auto executionRequest = zmq::socket_t(*context, ZMQ_REP);
    executionRequest.bind(execReqEndpoint.c_str());

    auto reportMasterSocket         = zmq::socket_t(*context, ZMQ_XPUB);
    const auto reportMasterPort     = dsb::domain_broker::BindToEphemeralPort(reportMasterSocket);
    const auto reportMasterEndpoint = "tcp://*:" + std::to_string(reportMasterPort);

    auto reportSlavePSocket         = zmq::socket_t(*context, ZMQ_XSUB);
    const auto reportSlavePPort     = dsb::domain_broker::BindToEphemeralPort(reportSlavePSocket);
    const auto reportSlavePEndpoint = "tcp://*:" + std::to_string(reportSlavePPort);

    auto report = dsb::proxy::SpawnProxy(
        context,
        std::move(reportMasterSocket),
        std::move(reportSlavePSocket));

    std::uint16_t infoPort = 0;
    auto info = dsb::domain_broker::SpawnP2PProxy(context, "*", infoPort);
    const auto infoEndpoint = "tcp://*:" + std::to_string(infoPort);

    std::cout << "Domain broker bound to the following endpoints: \n"
              << "  report         (masters): " << reportMasterEndpoint << '\n'
              << "  report (slave providers): " << reportSlavePEndpoint << '\n'
              << "  info          (everyone): " << infoEndpoint << '\n'
              << "  exec. request  (masters): " << execReqEndpoint << std::endl;

    std::map<std::string, ExecutionBroker> executionBrokers;
    for (;;) {
        std::deque<zmq::message_t> msg;
        dsb::comm::Receive(executionRequest, msg);
        const auto command = dsb::comm::ToString(msg.front());
        if (command == "GET_PROXY_PORTS") {
            msg.clear();
            msg.push_back(dsb::comm::ToFrame("PROXY_PORTS"));
            msg.push_back(dsb::comm::ToFrame(std::to_string(reportMasterPort)));
            msg.push_back(dsb::comm::ToFrame(std::to_string(reportSlavePPort)));
            msg.push_back(dsb::comm::ToFrame(std::to_string(infoPort)));
            msg.push_back(dsb::comm::ToFrame(std::to_string(infoPort)));
            dsb::comm::Send(executionRequest, msg);
        } else if (command == "SPAWN_EXECUTION" && msg.size() > 1) {
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
