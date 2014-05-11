#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

#include "dsb/comm.hpp"
#include "dsb/control.hpp"
#include "dsb/util.hpp"

#include "control.pb.h"


namespace
{
    enum SlaveState
    {
        SlaveState_unknown,
        SlaveState_connected,
        SlaveState_introduced,
        SlaveState_initialized,
        SlaveState_ready,
    };

    struct Slave
    {
        Slave()
            : protocol(0xFFFF), state(SlaveState_unknown) { }

        Slave(uint16_t protocol_)
            : protocol(protocol_), state(SlaveState_connected) { }

        uint16_t protocol;
        SlaveState state;
    };

    const uint16_t MAX_PROTOCOL = 0;
}


int main(int argc, const char** argv)
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <address>\n"
                  << "  address = the DSB server endpoint (e.g. tcp://localhost:5432)"
                  << std::endl;
        return 0;
    }
    const auto endpoint = std::string(argv[1]);

    auto context = zmq::context_t();
    auto control = zmq::socket_t(context, ZMQ_ROUTER);
    control.bind(endpoint.c_str());

    std::map<std::string, Slave> slaves;
    for (;;) {
        std::deque<zmq::message_t> msg;
        dsb::comm::Receive(control, msg);

        std::deque<zmq::message_t> envelope;
        dsb::comm::PopMessageEnvelope(msg, &envelope);
        const auto slaveId = dsb::comm::ToString(envelope.front());
        std::clog << "Received message from slave '" << slaveId << "':";

        switch (dsb::control::ParseMessageType(msg.front())) {
            case dsbproto::control::MessageType::HELLO: {
                std::clog << "HELLO" << std::endl;
                const auto slaveProtocol = dsb::control::ParseProtocolVersion(msg.front());
                if (slaveProtocol > 0) {
                    std::clog << "Warning: Slave requested newer protocol version"
                              << std::endl;
                    break;
                }
                slaves[slaveId] = Slave(slaveProtocol);
                dsb::control::CreateHelloMessage(0, msg);
                dsb::comm::AddressedSend(control, slaveId, msg);
                break;
            }
            case dsbproto::control::MessageType::DESCRIBE: {
                std::clog << "DESCRIBE" << std::endl;
                auto slaveIt = slaves.find(slaveId);
                if (slaveIt != slaves.end() && slaveIt->second.state == SlaveState_connected) {
                    slaveIt->second.state = SlaveState_introduced;
                } else {
                    std::clog << "Warning: Slave not found or in wrong state"
                              << std::endl;
                }
                break;
            }
            default:
                std::clog << "Warning: Invalid message received from client: "
                          << slaveId << std::endl;
        }
    }
/*
    dsb::control::CreateHelloMessage(0, msg);
    dsb::comm::Send(control, msg);

    // Receive HELLO
    dsb::comm::Receive(control, msg);
    if (dsb::control::ParseProtocolVersion(msg.front()) != 0) {
        throw std::runtime_error("Master required unsupported protocol");
    }
*/
}
