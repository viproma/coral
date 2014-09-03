#include "controller.hpp"

#include <cassert>
#include <deque>
#include <iostream>
#include <map>
#include <string>

#include "boost/foreach.hpp"
#include "boost/thread.hpp"
#include "zmq.hpp"

#include "dsb/comm.hpp"
#include "dsb/compat_helpers.hpp"
#include "dsb/control.hpp"
#include "dsb/util.hpp"
#include "control.pb.h"

#include "slave_handler.hpp"


class Execution;
class ExecutionInitializing;
class ExecutionReady;
class ExecutionStepping;
class ExecutionPublished;
class ExecutionTerminating;


class IExecutionStateHandler
{
public:
    virtual void StateEntered(
        Execution& self,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket) = 0;

    virtual void UserMessage(
        Execution& self,
        std::deque<zmq::message_t>& msg,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket) = 0;

    virtual void SlaveWaiting(
        Execution& self,
        SlaveHandler& slaveHandler,
        std::deque<zmq::message_t>& msg,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket) = 0;
};


class Execution
{
public:
    Execution(
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket)
    {
        ChangeState<ExecutionInitializing>(userSocket, slaveSocket);
    }

    void UserMessage(
        std::deque<zmq::message_t>& msg,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket)
    {
        m_handler->UserMessage(*this, msg, userSocket, slaveSocket);
    }

    void SlaveMessage(
        std::deque<zmq::message_t>& msg,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket)
    {
        std::deque<zmq::message_t> envelope;
        dsb::comm::PopMessageEnvelope(msg, &envelope);
        const auto slaveId = dsb::comm::ToString(envelope.back());
        std::clog << "Received message from slave '" << slaveId << "': ";

        // Pass on the message to the appropriate slave handler, send the
        // reply immediately if necessary.
        auto& slaveHandler = slaves.at(slaveId);
        if (!slaveHandler.RequestReply(slaveSocket, envelope, msg)) {
            m_handler->SlaveWaiting(*this, slaveHandler, msg, userSocket, slaveSocket);
        }
    }

    template<typename T>
    void ChangeState(
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket)
    {
        m_handler = std::make_unique<T>();
        m_handler->StateEntered(*this, userSocket, slaveSocket);
    }

    std::unique_ptr<IExecutionStateHandler> m_handler;
    std::map<std::string, SlaveHandler> slaves;
};


class ExecutionInitializing : public IExecutionStateHandler
{
    void StateEntered(
        Execution& self,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket) override
    {
    }

    void UserMessage(
        Execution& self,
        std::deque<zmq::message_t>& msg,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket) override
    {
        //  if message is SET_VARS
        //      store variable values/connections
    }

    void SlaveWaiting(
        Execution& self,
        SlaveHandler& slaveHandler,
        std::deque<zmq::message_t>& msg,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket) override
    {
        // Check whether all slaves are Ready, and if so, switch to Ready state.
        bool allReady = true;
        BOOST_FOREACH (const auto& slave, self.slaves) {
            if (slave.second.State() != SLAVE_READY) allReady = false;
        }
        if (allReady) self.ChangeState<ExecutionReady>(userSocket, slaveSocket);
    }
};


class ExecutionReady : public IExecutionStateHandler
{
    void StateEntered(
        Execution& self,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket) override
    {
        // send ALL_READY to user
    }

    void UserMessage(
        Execution& self,
        std::deque<zmq::message_t>& msg,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket) override
    {
        //  if message is STEP
        {
            const auto time = 0.0;  //TODO: Get from user!
            const auto stepSize = 0.1; // TODO: GET FROM USER GODDAMMIT!

            // Create the STEP message body
            dsbproto::control::StepData stepData;
            stepData.set_timepoint(time);
            stepData.set_stepsize(stepSize);
            // Create the multipart STEP message
            std::deque<zmq::message_t> stepMsg;
            dsb::control::CreateMessage(stepMsg, dsbproto::control::MSG_STEP, stepData);

            // For each slave, make a copy of the STEP message and send it.
            BOOST_FOREACH(auto& slave, self.slaves) {
                std::deque<zmq::message_t> stepMsgCopy;
                dsb::comm::CopyMessage(stepMsg, stepMsgCopy);
                slave.second.SendStep(slaveSocket, stepMsgCopy);
            }

            self.ChangeState<ExecutionStepping>(userSocket, slaveSocket);
        }
        //  else if message is SET_VARS
        //      send SET_VARS to appropriate slave
        //      go back to Init state
    }

    void SlaveWaiting(
        Execution& self,
        SlaveHandler& slaveHandler,
        std::deque<zmq::message_t>& msg,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket) override
    {
    }
};

class ExecutionStepping : public IExecutionStateHandler
{
    void StateEntered(
        Execution& self,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket) override
    {
    }

    void UserMessage(
        Execution& self,
        std::deque<zmq::message_t>& msg,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket) override
    {
    }

    void SlaveWaiting(
        Execution& self,
        SlaveHandler& slaveHandler,
        std::deque<zmq::message_t>& msg,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket) override
    {
        bool allPublished = true;
        BOOST_FOREACH (const auto& slave, self.slaves) {
            if (slave.second.IsSimulating() && slave.second.State() != SLAVE_PUBLISHED) {
                allPublished = false;
            }
        }
        if (allPublished) {
            self.ChangeState<ExecutionPublished>(userSocket, slaveSocket);
        }
   }
};

class ExecutionPublished : public IExecutionStateHandler
{
    void StateEntered(
        Execution& self,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket) override
    {
        // Create RECV_VARS message
        std::deque<zmq::message_t> recvVarsMsg;
        dsb::control::CreateMessage(recvVarsMsg, dsbproto::control::MSG_RECV_VARS);
        // Send this to all slaves which are in simulation
        BOOST_FOREACH (auto& slave, self.slaves) {
            if (slave.second.IsSimulating()) {
                // Make a copy of the message and send it
                std::deque<zmq::message_t> recvVarsMsgCopy;
                dsb::comm::CopyMessage(recvVarsMsg, recvVarsMsgCopy);
                slave.second.SendRecvVars(slaveSocket, recvVarsMsgCopy);
            }
        }
    }

    void UserMessage(
        Execution& self,
        std::deque<zmq::message_t>& msg,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket) override
    {
        //  if message is TERMINATE
        //      proceed to Terminating
    }

    void SlaveWaiting(
        Execution& self,
        SlaveHandler& slaveHandler,
        std::deque<zmq::message_t>& msg,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket) override
    {
        // Check whether all slaves are Ready, and if so, switch to Ready state.
        bool allReady = true;
        BOOST_FOREACH (const auto& slave, self.slaves) {
            if (slave.second.State() != SLAVE_READY) allReady = false;
        }
        if (allReady) self.ChangeState<ExecutionReady>(userSocket, slaveSocket);
    }
};


class ExecutionTerminating : public IExecutionStateHandler
{
public:
    ExecutionTerminating()
    {
        dsb::control::CreateMessage(m_termMsg, dsbproto::control::MSG_TERMINATE);
    }

    void StateEntered(
        Execution& self,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket)
    {
        BOOST_FOREACH (auto& slave, self.slaves) {
            if (slave.second.State() & TERMINATABLE_STATES) {
                std::deque<zmq::message_t> termMsgCopy;
                dsb::comm::CopyMessage(m_termMsg, termMsgCopy);
                slave.second.SendTerminate(slaveSocket, termMsgCopy);
            }
        }
    }

    void UserMessage(
        Execution& self,
        std::deque<zmq::message_t>& msg,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket)
    {
        // error
    }

    void SlaveWaiting(
        Execution& self,
        SlaveHandler& slaveHandler,
        std::deque<zmq::message_t>& msg,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket)
    {
        assert (slaveHandler.State() & TERMINATABLE_STATES);
            
        std::deque<zmq::message_t> termMsgCopy;
        dsb::comm::CopyMessage(m_termMsg, termMsgCopy);
        slaveHandler.SendTerminate(slaveSocket, termMsgCopy);
    }

private:
    std::deque<zmq::message_t> m_termMsg;
};


namespace
{
    void ControllerLoop(
        std::shared_ptr<zmq::context_t> context,
        std::shared_ptr<std::string> userEndpoint,
        std::shared_ptr<std::string> slaveControlEndpoint)
    {
        auto user = zmq::socket_t(*context, ZMQ_PAIR);
        user.connect(userEndpoint->c_str());

        auto slaveControl = zmq::socket_t(*context, ZMQ_ROUTER);
        slaveControl.connect(slaveControlEndpoint->c_str());

        bool terminate = false;

        // Make a list of expected slaves, hardcoded for the time being.
        Execution exec(user, slaveControl);
        exec.slaves["1"] = SlaveHandler();
        exec.slaves["2"] = SlaveHandler();
    //    exec.slaves["3"] = SlaveHandler();

        // Main messaging loop
        zmq::pollitem_t pollItems[2] = {
            { user,         0, ZMQ_POLLIN, 0 },
            { slaveControl, 0, ZMQ_POLLIN, 0 }
        };
        for (;;) {
            // Poll for incoming messages on both sockets.
            zmq::poll(pollItems, 2);

            if (pollItems[0].revents & ZMQ_POLLIN) {
                std::deque<zmq::message_t> msg;
                dsb::comm::Receive(user, msg);
                std::cout << "Received from user: " << dsb::comm::ToString(msg.front()) << std::endl;
                exec.UserMessage(msg, user, slaveControl);
            }
            if (pollItems[1].revents & ZMQ_POLLIN) {
                std::deque<zmq::message_t> msg;
                dsb::comm::Receive(slaveControl, msg);
                exec.SlaveMessage(msg, user, slaveControl);
            }
        }
    }
}


dsb::execution::Controller::Controller(zmq::socket_t socket)
    : m_socket(std::move(socket))
{
}


dsb::execution::Controller::Controller(Controller&& other)
    : m_socket(std::move(other.m_socket))
{
}


dsb::execution::Controller& dsb::execution::Controller::operator=(Controller&& other)
{
    m_socket = std::move(other.m_socket);
    return *this;
}


void dsb::execution::Controller::Step(double t, double dt)
{
    auto m = dsb::comm::ToFrame("Hello World!");
    m_socket.send(m);
}


dsb::execution::Controller dsb::execution::SpawnController(
    std::shared_ptr<zmq::context_t> context,
    const std::string& endpoint)
{
    auto userEndpoint = std::make_shared<std::string>("inproc://" + dsb::util::RandomUUID());
    auto slaveControlEndpoint = std::make_shared<std::string>(endpoint);
    auto userSocket = zmq::socket_t(*context, ZMQ_PAIR);
    userSocket.bind(userEndpoint->c_str());

    auto thread = boost::thread(ControllerLoop, context, userEndpoint, slaveControlEndpoint);
    return Controller(std::move(userSocket));
}
