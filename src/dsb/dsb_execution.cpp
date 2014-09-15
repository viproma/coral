#include "dsb/execution.hpp"

#include <iostream>
#include <utility>

#include "boost/thread.hpp"

#include "dsb/bus/execution_agent.hpp"
#include "dsb/bus/slave_tracker.hpp"
#include "dsb/comm.hpp"
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

        // Make a list of expected slaves, hardcoded for the time being.
        dsb::bus::ExecutionAgent exec(rpcSocket, slaveControlSocket);

        // Main messaging loop
        zmq::pollitem_t pollItems[2] = {
            { rpcSocket,          0, ZMQ_POLLIN, 0 },
            { slaveControlSocket, 0, ZMQ_POLLIN, 0 }
        };
        for (;;) {
            // Poll for incoming messages on both sockets.
            zmq::poll(pollItems, 2);

            if (pollItems[0].revents & ZMQ_POLLIN) {
                std::deque<zmq::message_t> msg;
                dsb::comm::Receive(rpcSocket, msg);
                std::cout << "Received from rpcSocket: " << dsb::comm::ToString(msg.front()) << std::endl;
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


namespace
{
    // Performs a remote procedure call (RPC).
    // This function will send `msg` on `socket` and wait for a reply.
    // If the reply is FAILED, an exception with the accompanying error
    // description is thrown.  Otherwise the function succeeds vacuously.
    // Either way, `msg` will be empty when the function returns/throws.
    void RPC(zmq::socket_t& socket, std::deque<zmq::message_t>& msg)
    {
        dsb::comm::Send(socket, msg);
        dsb::comm::Receive(socket, msg);
        const auto reply = dsb::comm::ToString(msg.at(0));
        if (reply == "FAILED") {
            assert (msg.size() == 2);
            msg.clear();
            throw std::runtime_error(dsb::comm::ToString(msg.at(1)));
        } else {
            assert (msg.size() == 1 && reply == "OK");
            msg.clear();
        }
    }

    // Convenience function which creates a 1-frame message out of `str` and
    // forwards it to RPC().
    void RPC(zmq::socket_t& socket, const std::string& str)
    {
        std::deque<zmq::message_t> msg;
        msg.push_back(dsb::comm::ToFrame(str));
        RPC(socket, msg);
    }
}


void dsb::execution::Controller::AddSlave(uint16_t slaveId)
{
    std::deque<zmq::message_t> msg;
    msg.push_back(dsb::comm::ToFrame("ADD_SLAVE"));
    msg.push_back(dsb::comm::EncodeRawDataFrame(slaveId));
    RPC(m_rpcSocket, msg);
}


void dsb::execution::Controller::SetVariables(
    uint16_t slaveId,
    dsb::sequence::Sequence<Variable&> variables)
{
    std::deque<zmq::message_t> msg;
    msg.push_back(dsb::comm::ToFrame("SET_VARS"));
    msg.push_back(dsb::comm::EncodeRawDataFrame(slaveId));
    while (!variables.Empty()) {
        const auto v = variables.Next();
        msg.push_back(dsb::comm::EncodeRawDataFrame(v.id));
        msg.push_back(dsb::comm::EncodeRawDataFrame(v.value));
    }
    RPC(m_rpcSocket, msg);
}


void dsb::execution::Controller::ConnectVariables(
    uint16_t slaveId,
    dsb::sequence::Sequence<VariableConnection&> variables)
{
    std::deque<zmq::message_t> msg;
    msg.push_back(dsb::comm::ToFrame("CONNECT_VARS"));
    msg.push_back(dsb::comm::EncodeRawDataFrame(slaveId));
    while (!variables.Empty()) {
        const auto v = variables.Next();
        msg.push_back(dsb::comm::EncodeRawDataFrame(v.inputId));
        msg.push_back(dsb::comm::EncodeRawDataFrame(v.otherSlaveId));
        msg.push_back(dsb::comm::EncodeRawDataFrame(v.otherOutputId));
    }
    RPC(m_rpcSocket, msg);
}


void dsb::execution::Controller::WaitForReady()
{
    RPC(m_rpcSocket, "WAIT_FOR_READY");
}


void dsb::execution::Controller::Step(double t, double dt)
{
    WaitForReady();
    std::deque<zmq::message_t> msg;
    msg.push_back(dsb::comm::ToFrame("STEP"));
    msg.push_back(dsb::comm::EncodeRawDataFrame(t));
    msg.push_back(dsb::comm::EncodeRawDataFrame(dt));
    RPC(m_rpcSocket, msg);
}


void dsb::execution::Controller::Terminate()
{
    RPC(m_rpcSocket, "TERMINATE");
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
