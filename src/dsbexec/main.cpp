#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#   define NOMINMAX
#endif
#include "dsb/comm.hpp"
#include "dsb/control.hpp"
#include "dsb/util.hpp"
#include "control.pb.h"


namespace
{
    enum SlaveState
    {
        SLAVE_UNKNOWN       = 1,
        SLAVE_CONNECTED     = 1 << 1,
        SLAVE_INITIALIZING  = 1 << 2,
        SLAVE_READY         = 1 << 3,
    };

    struct SlaveTracker
    {
        SlaveTracker()
            : protocol(0xFFFF), state(SLAVE_UNKNOWN) { }

        SlaveTracker(uint16_t protocol_)
            : protocol(protocol_), state(SLAVE_CONNECTED) { }

        uint16_t protocol;
        SlaveState state;
    };

    bool UpdateSlaveState(
        std::map<std::string, SlaveTracker>& slaves,
        const std::string& slaveId,
        int oldStates,
        SlaveState newState)
    {
        auto slaveIt = slaves.find(slaveId);
        if (slaveIt != slaves.end() && slaveIt->second.state & oldStates) {
            slaveIt->second.state = newState;
            return true;
        } else {
            std::clog << "Warning: Slave not found or in wrong state" << std::endl;
            return false;
        }
    }

    void SendInvalidRequest(
        zmq::socket_t& socket,
        std::deque<zmq::message_t>& slaveEnvelope)
    {
        std::deque<zmq::message_t> msg;
        dsb::control::CreateErrorMessage(
            msg,
            dsbproto::control::ErrorInfo::INVALID_REQUEST,
            "Slave ID not seen before, or slave was expected to be in different state");
        dsb::comm::AddressedSend(socket, slaveEnvelope, msg);
    }

    void SendEmptyMessage(
        zmq::socket_t& socket,
        std::deque<zmq::message_t>& slaveEnvelope,
        dsbproto::control::MessageType type)
    {
        std::deque<zmq::message_t> msg;
        dsb::control::CreateMessage(msg, type);
        dsb::comm::AddressedSend(socket, slaveEnvelope, msg);
    }

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
    control.connect(endpoint.c_str());

    std::map<std::string, SlaveTracker> slaves;
    for (;;) {
        std::deque<zmq::message_t> msg;
        dsb::comm::Receive(control, msg);

        std::deque<zmq::message_t> envelope;
        dsb::comm::PopMessageEnvelope(msg, &envelope);
        const auto slaveId = dsb::comm::ToString(envelope.back());
        std::clog << "Received message from slave '" << slaveId << "': ";

        switch (dsb::control::ParseMessageType(msg.front())) {
            case dsbproto::control::MSG_HELLO: {
                std::clog << "MSG_HELLO" << std::endl;
                const auto slaveProtocol = dsb::control::ParseProtocolVersion(msg.front());
                if (slaveProtocol > 0) {
                    std::clog << "Warning: Slave requested newer protocol version"
                              << std::endl;
                    break;
                }
                slaves[slaveId] = SlaveTracker(slaveProtocol);
                dsb::control::CreateHelloMessage(
                    msg,
                    std::min(MAX_PROTOCOL, slaveProtocol));
                dsb::comm::AddressedSend(control, envelope, msg);
                break;
            }
            case dsbproto::control::MSG_INIT_READY:
                std::clog << "MSG_INIT_READY" << std::endl;
                if (UpdateSlaveState(slaves, slaveId, SLAVE_CONNECTED | SLAVE_INITIALIZING, SLAVE_INITIALIZING)) {
                    SendEmptyMessage(control, envelope, dsbproto::control::MSG_INIT_DONE);
                } else {
                    SendInvalidRequest(control, envelope);
                }
                break;

            case dsbproto::control::MSG_READY:
                std::clog << "MSG_READY" << std::endl;
                if (UpdateSlaveState(slaves, slaveId, SLAVE_INITIALIZING | SLAVE_READY, SLAVE_READY)) {
                    SendEmptyMessage(control, envelope, dsbproto::control::MSG_STEP);
                } else {
                    SendInvalidRequest(control, envelope);
                }
                break;

            default:
                std::clog << "Warning: Invalid message received from client: "
                          << slaveId << std::endl;
                SendInvalidRequest(control, envelope);
        }
    }
}
