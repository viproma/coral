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
        std::shared_ptr<std::string> userEndpoint,
        std::shared_ptr<std::string> slaveControlEndpoint)
    {
        auto user = zmq::socket_t(*context, ZMQ_PAIR);
        user.connect(userEndpoint->c_str());

        auto slaveControl = zmq::socket_t(*context, ZMQ_ROUTER);
        slaveControl.connect(slaveControlEndpoint->c_str());

        bool terminate = false;

        // Make a list of expected slaves, hardcoded for the time being.
        dsb::bus::ExecutionAgent exec(user, slaveControl);
        exec.slaves["1"] = dsb::bus::SlaveTracker();
        exec.slaves["2"] = dsb::bus::SlaveTracker();
    //    exec.slaves["3"] = SlaveTracker();

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
    zmq::message_t tmp;
    while (m_socket.recv(&tmp, ZMQ_DONTWAIT)) { }

    std::deque<zmq::message_t> msg;
    msg.push_back(dsb::comm::ToFrame("STEP"));
    msg.push_back(dsb::comm::EncodeRawDataFrame(t));
    msg.push_back(dsb::comm::EncodeRawDataFrame(dt));
    dsb::comm::Send(m_socket, msg);

    dsb::comm::Receive(m_socket, msg);
    assert (msg.size() == 1 && dsb::comm::ToString(msg.front()) == "ALL_READY");
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
