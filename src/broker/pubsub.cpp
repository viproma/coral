#include <iostream>
#include <string>
#include "boost/thread.hpp"
#include "zmq.hpp"


const long SECONDS = 1000;
const long MSECS = 1;


void slave(const char* name)
{
    const auto text = std::string("Hello from ") + name;

    auto ctx = zmq::context_t();
    auto slaveOutput = zmq::socket_t(ctx, ZMQ_PUB);
    slaveOutput.connect("ipc://broker_slaveOutputs");

    auto slaveInput = zmq::socket_t(ctx, ZMQ_SUB);
    slaveInput.setsockopt(ZMQ_SUBSCRIBE, "", 0);
    slaveInput.connect("ipc://broker_slaveInputs");

    while (true) {
        zmq::pollitem_t item = { slaveInput, 0, ZMQ_POLLIN, 0 };
        zmq::poll(&item, 1, 1*SECONDS);
        if (item.revents & ZMQ_POLLIN) {
            auto msg = zmq::message_t();
            slaveInput.recv(&msg);
            std::cout << name << ": Received \""
                      << std::string(reinterpret_cast<char*>(msg.data()), msg.size())
                      << '"' << std::endl;
        } else {
            slaveOutput.send(text.data(), text.size());
        }
    }
}

void spawn(void (*func)(const char*), const char* name)
{
    auto t = boost::thread(func, name);
    t.detach();
}

int main(int argc, char** argv)
{
    auto ctx = zmq::context_t();

    auto slaveOutputs = zmq::socket_t(ctx, ZMQ_XSUB);
    slaveOutputs.bind("ipc://broker_slaveOutputs");

    auto slaveInputs = zmq::socket_t(ctx, ZMQ_XPUB);
    slaveInputs.bind("ipc://broker_slaveInputs");

    spawn(slave, "Slave A");
    spawn(slave, "Slave B");
    zmq::proxy(slaveOutputs, slaveInputs, nullptr);
}
