#define BOOST_CHRONO_DONT_PROVIDES_DEPRECATED_IO_SINCE_V2_0_0
#include "dsb/execution/controller.hpp"

#include <deque>
#include <iostream> //TODO: Only for debugging; remove later.
#include <sstream>
#include <utility>

#include "boost/chrono.hpp"

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
        std::shared_ptr<dsb::execution::Locator> execLocator)
    {
        auto rpcSocket = zmq::socket_t(*context, ZMQ_PAIR);
        rpcSocket.connect(rpcEndpoint->c_str());

        // TODO: This socket is currently not used for anything!
        auto asyncInfoSocket = zmq::socket_t(*context, ZMQ_PAIR);
        asyncInfoSocket.connect(asyncInfoEndpoint->c_str());

        auto slaveControlSocket = zmq::socket_t(*context, ZMQ_ROUTER);
        slaveControlSocket.connect(execLocator->MasterEndpoint().c_str());

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

        auto execTerminationSocket = zmq::socket_t(*context, ZMQ_REQ);
        execTerminationSocket.connect(execLocator->ExecTerminationEndpoint().c_str());
        std::deque<zmq::message_t> termMsg;
        termMsg.push_back(dsb::comm::ToFrame("TERMINATE_EXECUTION"));
        termMsg.push_back(dsb::comm::ToFrame(execLocator->ExecName()));
        dsb::comm::Send(execTerminationSocket, termMsg);
        // TODO: The following receive was just added to force ZMQ to send the
        // message. We don't really care about the reply.  We've tried closing
        // the socket and even the context manually, but then the message just
        // appears to be dropped altoghether.  This sucks, and we need to figure
        // out what is going on at some point.
        char temp;
        execTerminationSocket.recv(&temp, 1);
    }
}


dsb::execution::Controller::Controller(const dsb::execution::Locator& locator)
    : m_context(std::make_shared<zmq::context_t>()),
      m_rpcSocket(*m_context, ZMQ_PAIR),
      m_asyncInfoSocket(*m_context, ZMQ_PAIR),
      m_active(true)
{
    auto rpcEndpoint =
        std::make_shared<std::string>("inproc://" + dsb::util::RandomUUID());
    auto asyncInfoEndpoint =
        std::make_shared<std::string>("inproc://" + dsb::util::RandomUUID());
    m_rpcSocket.bind(rpcEndpoint->c_str());
    m_asyncInfoSocket.bind(asyncInfoEndpoint->c_str());
    m_thread = boost::thread(ControllerLoop,
        m_context, rpcEndpoint, asyncInfoEndpoint,
        std::make_shared<dsb::execution::Locator>(locator));
}


dsb::execution::Controller::Controller(Controller&& other)
    : m_context(std::move(other.m_context)),
      m_rpcSocket(std::move(other.m_rpcSocket)),
      m_asyncInfoSocket(std::move(other.m_asyncInfoSocket)),
      m_active(dsb::util::SwapOut(other.m_active, false)),
      m_thread(std::move(other.m_thread))
{
}


dsb::execution::Controller& dsb::execution::Controller::operator=(Controller&& other)
{
    m_rpcSocket         = std::move(other.m_rpcSocket);
    m_asyncInfoSocket   = std::move(other.m_asyncInfoSocket);
    m_active            = dsb::util::SwapOut(other.m_active, false);
    m_thread            = std::move(other.m_thread);
    // Move the context last, in case it overwrites and destroys another
    // context that is used by the above sockets.
    m_context           = std::move(other.m_context);
    return *this;
}


dsb::execution::Controller::~Controller()
{
    if (m_active) try {
        Terminate();
    } catch (...) {
        assert (!"dsb::execution::Controller::~Controller(): CallTerminate() threw");
    }
    m_thread.join();
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
    assert (m_active);
    dsb::inproc_rpc::CallTerminate(m_rpcSocket);
    m_active = false;
}


namespace
{
    std::string Timestamp()
    {
        const auto t = boost::chrono::system_clock::now();
        std::ostringstream ss;
        ss << boost::chrono::time_fmt(boost::chrono::timezone::utc, "%Y%m%dT%H%M%SZ") << t;
        return ss.str();
    }
}


dsb::execution::Locator dsb::execution::SpawnExecution(
    const dsb::domain::Locator& domainLocator,
    const std::string& executionName)
{
    const auto actualExecName = executionName.empty()
        ? Timestamp()
        : executionName;

    zmq::context_t ctx;
    auto sck = zmq::socket_t(ctx, ZMQ_REQ);
    sck.connect(domainLocator.ExecReqEndpoint().c_str());
    std::deque<zmq::message_t> msg;
    msg.push_back(dsb::comm::ToFrame("SPAWN_EXECUTION"));
    msg.push_back(dsb::comm::ToFrame(actualExecName));
    dsb::comm::Send(sck, msg);

    if (!dsb::comm::Receive(sck, msg, boost::chrono::seconds(10))) {
        throw std::runtime_error("Failed to spawn execution (domain connection timed out)");
    }
    const auto reply = dsb::comm::ToString(msg.front());
    if (reply == "SPAWN_EXECUTION_OK" && msg.size() == 5) {
        const auto endpointBase = domainLocator.ExecReqEndpoint().substr(
            0,
            domainLocator.ExecReqEndpoint().rfind(':'));
        return dsb::execution::Locator(
            endpointBase + ':' + dsb::comm::ToString(msg[1]),
            endpointBase + ':' + dsb::comm::ToString(msg[2]),
            endpointBase + ':' + dsb::comm::ToString(msg[3]),
            endpointBase + ':' + dsb::comm::ToString(msg[4]),
            domainLocator.ExecReqEndpoint(),
            actualExecName);
    } else if (reply == "SPAWN_EXECUTION_FAIL" && msg.size() == 2) {
        throw std::runtime_error(
            "Failed to spawn execution (" + dsb::comm::ToString(msg[1]) + ')');
    } else {
        throw std::runtime_error("Failed to spawn execution (invalid response from domain)");
    }
}
