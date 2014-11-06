#include "dsb/execution/controller.hpp"

#include <deque>
#include <iostream> //TODO: Only for debugging; remove later.
#include <utility>

#include "boost/thread.hpp"

#include "dsb/bus/execution_agent.hpp"
#include "dsb/comm.hpp"
#include "dsb/inproc_rpc.hpp"
#include "dsb/util.hpp"


namespace
{
    void ControllerLoop(
        std::shared_ptr<zmq::context_t> context,
        std::shared_ptr<std::string> rpcEndpoint,
        std::shared_ptr<std::string> asyncInfoEndpoint,
        std::shared_ptr<std::string> slaveControlEndpoint)
    {
        auto rpcSocket = zmq::socket_t(*context, ZMQ_PAIR);
        rpcSocket.connect(rpcEndpoint->c_str());

        // TODO: This socket is currently not used for anything!
        auto asyncInfoSocket = zmq::socket_t(*context, ZMQ_PAIR);
        asyncInfoSocket.connect(asyncInfoEndpoint->c_str());

        auto slaveControlSocket = zmq::socket_t(*context, ZMQ_ROUTER);
        slaveControlSocket.connect(slaveControlEndpoint->c_str());

        // Main messaging loop
        dsb::bus::ExecutionAgent exec(rpcSocket, slaveControlSocket);
        zmq::pollitem_t pollItems[2] = {
            { rpcSocket,          0, ZMQ_POLLIN, 0 },
            { slaveControlSocket, 0, ZMQ_POLLIN, 0 }
        };
        while (!exec.HasShutDown()) {
            // Poll for incoming messages on both sockets.
            zmq::poll(pollItems, 2);

            if (pollItems[0].revents & ZMQ_POLLIN) {
                std::deque<zmq::message_t> msg;
                dsb::comm::Receive(rpcSocket, msg);
//                std::cout << "Received from rpcSocket: " << dsb::comm::ToString(msg.front()) << std::endl;
                exec.UserMessage(msg, rpcSocket, slaveControlSocket);
            }
            if (pollItems[1].revents & ZMQ_POLLIN) {
                std::deque<zmq::message_t> msg;
                dsb::comm::Receive(slaveControlSocket, msg);
                exec.SlaveMessage(msg, rpcSocket, slaveControlSocket);
            }
        }
    }
}


dsb::execution::Controller::Controller(
    zmq::socket_t rpcSocket,
    zmq::socket_t asyncInfoSocket)
    : m_rpcSocket(std::move(rpcSocket)),
      m_asyncInfoSocket(std::move(asyncInfoSocket))
{
}


dsb::execution::Controller::Controller(Controller&& other)
    : m_rpcSocket(std::move(other.m_rpcSocket)),
      m_asyncInfoSocket(std::move(other.m_asyncInfoSocket))
{
}


dsb::execution::Controller& dsb::execution::Controller::operator=(Controller&& other)
{
    m_rpcSocket = std::move(other.m_rpcSocket);
    m_asyncInfoSocket = std::move(other.m_asyncInfoSocket);
    return *this;
}


void dsb::execution::Controller::SetSimulationTime(
    dsb::model::TimePoint startTime,
    dsb::model::TimePoint stopTime)
{
    dsb::inproc_rpc::CallSetSimulationTime(m_rpcSocket, startTime, stopTime);
}


void dsb::execution::Controller::AddSlave(uint16_t slaveId)
{
    dsb::inproc_rpc::CallAddSlave(m_rpcSocket, slaveId);
}


void dsb::execution::Controller::SetVariables(
    dsb::model::SlaveID slaveId,
    dsb::sequence::Sequence<dsb::model::VariableValue> variables)
{
    dsb::inproc_rpc::CallSetVariables(m_rpcSocket, slaveId, variables);
}


void dsb::execution::Controller::ConnectVariables(
    dsb::model::SlaveID slaveId,
    dsb::sequence::Sequence<dsb::model::VariableConnection> connections)
{
    dsb::inproc_rpc::CallConnectVariables(m_rpcSocket, slaveId, connections);
}


void dsb::execution::Controller::WaitForReady()
{
    dsb::inproc_rpc::CallWaitForReady(m_rpcSocket);
}


void dsb::execution::Controller::Step(
    dsb::model::TimePoint t,
    dsb::model::TimeDuration dt)
{
    WaitForReady();
    dsb::inproc_rpc::CallStep(m_rpcSocket, t, dt);
}


void dsb::execution::Controller::Terminate()
{
    dsb::inproc_rpc::CallTerminate(m_rpcSocket);
}


dsb::execution::Controller dsb::execution::SpawnExecution(
    std::shared_ptr<zmq::context_t> context,
    const std::string& endpoint)
{
    auto rpcEndpoint =
        std::make_shared<std::string>("inproc://" + dsb::util::RandomUUID());
    auto asyncInfoEndpoint =
        std::make_shared<std::string>("inproc://" + dsb::util::RandomUUID());
    auto slaveControlEndpoint = std::make_shared<std::string>(endpoint);

    auto userSocket = zmq::socket_t(*context, ZMQ_PAIR);
    userSocket.bind(rpcEndpoint->c_str());
    auto asyncInfoSocket = zmq::socket_t(*context, ZMQ_PAIR);
    asyncInfoSocket.bind(asyncInfoEndpoint->c_str());

    auto thread = boost::thread(ControllerLoop,
        context, rpcEndpoint, asyncInfoEndpoint, slaveControlEndpoint);
    return Controller(std::move(userSocket), std::move(asyncInfoSocket));
}
