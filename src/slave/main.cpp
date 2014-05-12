#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

#include "dsb/comm.hpp"
#include "dsb/control.hpp"
#include "dsb/error.hpp"
#include "dsb/util.hpp"

#include "control.pb.h"


class Shutdown : std::exception
{
public:
    const char* what() const noexcept { return "Normal shutdown requested by master"; }
};


uint16_t NormalMessageType(const std::deque<zmq::message_t>& msg)
{
    const auto mt = dsb::control::NonErrorMessageType(msg);
    if (mt == dsbproto::control::TERMINATE) throw Shutdown();
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
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <id> <address>\n"
                  << "  id      = a number in the range 0 - 65535\n"
                  << "  address = DSB server endpoint (e.g. tcp://myhost:5432)"
                  << std::endl;
        return 0;
    }
    const auto id = std::string(argv[1]);
    const auto endpoint = std::string(argv[2]);

    auto context = zmq::context_t();
    auto control = zmq::socket_t(context, ZMQ_REQ);
    control.setsockopt(ZMQ_IDENTITY, id.data(), id.size());
    control.connect(endpoint.c_str());

    // Send HELLO
    std::deque<zmq::message_t> msg;
    dsb::control::CreateHelloMessage(0, msg);
    dsb::comm::Send(control, msg);

    // Receive HELLO
    dsb::comm::Receive(control, msg);
    if (dsb::control::ParseProtocolVersion(msg.front()) != 0) {
        throw std::runtime_error("Master required unsupported protocol");
    }

    // Send DESCRIBE
    dsb::control::CreateMessage(
        dsbproto::control::MessageType::DESCRIBE,
        msg);
    dsb::comm::Send(control, msg);

    // Receive INITIALIZE
    dsb::comm::Receive(control, msg);
    EnforceMessageType(msg, dsbproto::control::INITIALIZE);

    // Send INITIALIZED
    dsb::control::CreateMessage(
        dsbproto::control::MessageType::INITIALIZED,
        msg);
    dsb::comm::Send(control, msg);

    // Receive SUBSCRIBE
    dsb::comm::Receive(control, msg);
    EnforceMessageType(msg, dsbproto::control::SUBSCRIBE);

    // READY loop
    for (;;) {
        dsb::control::CreateMessage(
            dsbproto::control::MessageType::READY,
            msg);
        dsb::comm::Send(control, msg);

        dsb::comm::Receive(control, msg);
    }
}
