#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

#include "boost/chrono.hpp"
#include "boost/thread.hpp"

#include "dsb/comm.hpp"
#include "dsb/control.hpp"
#include "dsb/error.hpp"
#include "dsb/util.hpp"

#include "control.pb.h"

#include "mock_slaves.hpp"


#if defined(_MSC_VER) && _MSC_VER <= 1800
#   define noexcept
#endif

class Shutdown : std::exception
{
public:
    const char* what() const noexcept override { return "Normal shutdown requested by master"; }
};


uint16_t NormalMessageType(const std::deque<zmq::message_t>& msg)
{
    const auto mt = dsb::control::NonErrorMessageType(msg);
    if (mt == dsbproto::control::MSG_TERMINATE) throw Shutdown();
    return mt;
}


void EnforceMessageType(
    const std::deque<zmq::message_t>& msg,
    dsbproto::control::MessageType expectedType)
{
    if (NormalMessageType(msg) != expectedType) {
        throw dsb::error::ProtocolViolationException(
            "Invalid reply from master");
    }
}


int main(int argc, const char** argv)
{
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <id> <address> <slave type>\n"
                  << "  id      = a number in the range 0 - 65535\n"
                  << "  address = DSB server endpoint (e.g. tcp://myhost:5432)"
                  << std::endl;
        return 0;
    }
    const auto id = std::string(argv[1]);
    const auto endpoint = std::string(argv[2]);
    const auto slaveType = std::string(argv[3]);

    auto instance = NewSlave(slaveType);

    auto context = zmq::context_t();
    auto control = zmq::socket_t(context, ZMQ_REQ);
    control.setsockopt(ZMQ_IDENTITY, id.data(), id.size());
    control.connect(endpoint.c_str());

    // Send HELLO
    std::deque<zmq::message_t> msg;
    dsb::control::CreateHelloMessage(msg, 0);
    dsb::comm::Send(control, msg);

    // Receive HELLO
    dsb::comm::Receive(control, msg);
    if (dsb::control::ParseProtocolVersion(msg.front()) != 0) {
        throw std::runtime_error("Master required unsupported protocol");
    }

    // Send MSG_INIT_READY
    dsb::control::CreateMessage(msg, dsbproto::control::MSG_INIT_READY);
    dsb::comm::Send(control, msg);

    // Receive MSG_INIT_DONE
    dsb::comm::Receive(control, msg);
    EnforceMessageType(msg, dsbproto::control::MSG_INIT_DONE);

    // MSG_READY loop
    for (;;) {
        dsb::control::CreateMessage(msg, dsbproto::control::MSG_READY);
        dsb::comm::Send(control, msg);

        dsb::comm::Receive(control, msg);
        const auto msgType = NormalMessageType(msg);
        switch (msgType) {
            case dsbproto::control::MSG_STEP:
                std::cout << "Performing time step" << std::endl;
                boost::this_thread::sleep_for(boost::chrono::milliseconds(10));
                break;
            default:
                throw dsb::error::ProtocolViolationException(
                    "Invalid reply from master");
        }
    }
}
