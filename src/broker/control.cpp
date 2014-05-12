#include <cassert>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "boost/chrono.hpp"
#include "boost/thread.hpp"

#include "zmq.hpp"
#include "control.pb.h"


const long SECONDS = 1000;
const long MSECS = 1;


size_t send(zmq::socket_t& socket, const std::string& data, bool more = false)
{
    return socket.send(data.data(), data.size(), more ? ZMQ_SNDMORE : 0);
}

template<typename T>
void sendPB(zmq::socket_t& socket, const T& pb, bool more = false)
{
    const auto size = pb.ByteSize();
    auto msg = zmq::message_t(size);
    pb.SerializeToArray(msg.data(), size);
    socket.send(msg, more ? ZMQ_SNDMORE : 0);
}

std::string toString(const zmq::message_t& msg)
{
    return std::string(reinterpret_cast<const char*>(msg.data()), msg.size());
}

std::string recvString(zmq::socket_t& socket)
{
    auto msg = zmq::message_t();
    socket.recv(&msg, 0);
    return toString(msg);
}

void recvMulti(zmq::socket_t& socket, std::vector<zmq::message_t>& vec)
{
    assert(vec.empty());
    do {
#if defined(_MSC_VER) && _MSC_VER <= 1600
        vec.emplace_back(zmq::message_t());
#else
        vec.emplace_back();
#endif
        socket.recv(&vec.back(), 0);
    } while (vec.back().more());
}

template<typename T>
void readPB(const zmq::message_t& msg, T& pb)
{
    pb.ParseFromArray(msg.data(), msg.size());
}

void slave(const char* namez)
{
    const auto name = std::string(namez);
    auto context = zmq::context_t();

    auto control = zmq::socket_t(context, ZMQ_REQ);
    control.setsockopt(ZMQ_IDENTITY, name.data(), name.size());
    control.connect("ipc://broker_slaveControl");

    dsbproto::control::VarInfo varInfo;
    varInfo.set_id(123);
    varInfo.set_name("myvar");
    varInfo.set_type(dsbproto::variable::INTEGER);
    varInfo.set_causality(dsbproto::variable::OUTPUT);
    send(control, "HELLO", true);
    sendPB(control, varInfo);

    std::vector<zmq::message_t> connect;
    recvMulti(control, connect);
    if (connect.size() < 2 || toString(connect.front()) != "CONNECT") {
        std::cerr << name
                  << ": Killing myself due to invalid command from master: "
                  << toString(connect.front()) << std::endl;
        return;
    }
    std::cout << name << ": Master told me to connect to "
              << toString(connect[1]) << std::endl;

    // Simulate connections being made.
    boost::this_thread::sleep_for(boost::chrono::milliseconds(50));

    send(control, "CONNECTED");
    recvString(control);
    std::cout << name << ": Moving on to next phase, i.e. dying." << std::endl;
}

void master(const size_t slaveCount)
{
    auto context = zmq::context_t();

    auto control = zmq::socket_t(context, ZMQ_DEALER);
    control.connect("ipc://broker_masterControl");

    std::set<std::string> slavesSeen;
    while (slavesSeen.size() < slaveCount) {
        std::vector<zmq::message_t> msg;
        recvMulti(control, msg);
        if (msg.size() < 2 || msg[1].size() != 0) {
            std::cerr << "Master: Invalid message received. Ignoring." << std::endl;
            continue;
        }

        const auto slaveId = toString(msg.front());
        if (msg.size() < 3) {
            std::cerr << "Master: Invalid message received from "
                      << slaveId << ". Ignoring." << std::endl;
            continue;
        }

        const auto cmd = toString(msg[2]);
        if (cmd == "HELLO") {
            std::cout << "Master: Slave connected: " << slaveId << std::endl;
            dsbproto::control::VarInfo varInfo;
            readPB(msg[3], varInfo);
            std::cout << " -- " << varInfo.id() << " " << varInfo.name() << " "
                      << varInfo.type() << " " << varInfo.causality() << std::endl;
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

    boost::thread(master, 2).detach();
    boost::thread(slave, "foo").detach();
    boost::thread(slave, "bar").detach();

    zmq::proxy(slaveControl, masterControl, nullptr);
}
