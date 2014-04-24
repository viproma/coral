#include <cassert>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "boost/chrono.hpp"
#include "boost/thread.hpp"

#include "zmq.hpp"


const long SECONDS = 1000;
const long MSECS = 1;


size_t send(zmq::socket_t& socket, const std::string& data, bool more = false)
{
    return socket.send(data.data(), data.size(), more ? ZMQ_SNDMORE : 0);
}

std::string recv(zmq::socket_t& socket)
{
    auto msg = zmq::message_t();
    socket.recv(&msg);
    return std::string(reinterpret_cast<char*>(msg.data()), msg.size());
}

std::vector<std::string> recvMulti(zmq::socket_t& socket)
{
    zmq::message_t msg;
    std::vector<std::string> vec;
    do {
        msg.rebuild();
        socket.recv(&msg);
        vec.push_back(std::string(reinterpret_cast<char*>(msg.data()), msg.size()));
    } while (msg.more());
    return vec;
}

void slave(const char* namez)
{
    const auto name = std::string(namez);
    auto context = zmq::context_t();

    auto control = zmq::socket_t(context, ZMQ_REQ);
    control.setsockopt(ZMQ_IDENTITY, name.data(), name.size());
    control.connect("ipc://broker_slaveControl");

    send(control, "HELLO");

    const auto connect = recvMulti(control);
    if (connect.size() < 2 || connect.front() != "CONNECT") {
        std::cerr << name
                  << ": Killing myself due to invalid command from master: "
                  << connect.front() << std::endl;
        return;
    }
    std::cout << name << ": Master told me to connect to " << connect[1] << std::endl;

    // Simulate connections being made.
    boost::this_thread::sleep_for(boost::chrono::milliseconds(50));

    send(control, "CONNECTED");
    recv(control);
    std::cout << name << ": Moving on to next phase, i.e. dying." << std::endl;
}

void master(const size_t slaveCount)
{
    auto context = zmq::context_t();

    auto control = zmq::socket_t(context, ZMQ_DEALER);
    control.connect("ipc://broker_masterControl");

    std::set<std::string> slavesSeen;
    while (slavesSeen.size() < slaveCount) {
        const auto msg = recvMulti(control);
        if (msg.size() < 2 || !msg[1].empty()) {
            std::cerr << "Master: Invalid message received. Ignoring." << std::endl;
            continue;
        }

        const auto slaveId = msg.front();
        if (msg.size() < 3) {
            std::cerr << "Master: Invalid message received from "
                      << slaveId << ". Ignoring." << std::endl;
            continue;
        }

        const auto cmd = msg[2];
        if (cmd == "HELLO") {
            std::cout << "Master: Slave connected: " << slaveId << std::endl;
            send(control, slaveId, true);
            send(control, "", true);
            send(control, "CONNECT", true);
            send(control, "someone");
        } else if (cmd == "CONNECTED") {
            slavesSeen.insert(slaveId);
            std::cout << "Master: Slave ready: " << slaveId
                      << ". " << slaveCount - slavesSeen.size() << " left."
                      << std::endl;
        }
    }

    std::cout << "Master: All slaves online, let's go!" << std::endl;
    for (auto it = slavesSeen.begin(); it != slavesSeen.end(); ++it) {
        send(control, *it, true);
        send(control, "", true);
        send(control, "INIT");
    }
}

int main(int argc, char** argv)
{
    auto context = zmq::context_t();

    auto slaveControl = zmq::socket_t(context, ZMQ_ROUTER);
    slaveControl.bind("ipc://broker_slaveControl");

    auto masterControl = zmq::socket_t(context, ZMQ_DEALER);
    masterControl.bind("ipc://broker_masterControl");

    boost::thread(master, 3).detach();
    boost::thread(slave, "foo").detach();
    boost::thread(slave, "bar").detach();

    zmq::proxy(slaveControl, masterControl, nullptr);
}
