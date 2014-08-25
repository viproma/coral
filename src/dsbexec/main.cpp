#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

#include "boost/foreach.hpp"

#ifdef _WIN32
#   define NOMINMAX
#endif
#include "dsb/comm.hpp"
#include "dsb/control.hpp"
#include "dsb/util.hpp"
#include "control.pb.h"
#include "variable.pb.h"


namespace
{
    enum SlaveState
    {
        SLAVE_UNKNOWN       = 1,
        SLAVE_CONNECTING    = 1 << 1,
        SLAVE_INITIALIZING  = 1 << 2,
        SLAVE_READY         = 1 << 3,
        SLAVE_STEPPING      = 1 << 4,
        SLAVE_PUBLISHED     = 1 << 5,
        SLAVE_RECEIVING     = 1 << 6,
        SLAVE_STEP_FAILED   = 1 << 7,
    };

    struct SlaveTracker
    {
        static const uint16_t UNKNOWN_PROTOCOL = 0xFFFF;

        SlaveTracker()
            : protocol(UNKNOWN_PROTOCOL), state(SLAVE_UNKNOWN), isSimulating(false) { }

        SlaveTracker(uint16_t protocol_)
            : protocol(protocol_), state(SLAVE_CONNECTING), isSimulating(false) { }

        SlaveTracker(SlaveTracker& other) {
            protocol = other.protocol;
            state = other.state;
            isSimulating = other.isSimulating;
            dsb::comm::CopyMessage(other.envelope, envelope);
        }

        SlaveTracker& operator=(SlaveTracker& other) {
            protocol = other.protocol;
            state = other.state;
            isSimulating = other.isSimulating;
            dsb::comm::CopyMessage(other.envelope, envelope);
            return *this;
        }

        uint16_t protocol;
        SlaveState state;
        bool isSimulating;
        std::deque<zmq::message_t> envelope;
    };

    bool UpdateSlaveState(
        std::map<std::string, SlaveTracker>& slaves,
        const std::string& slaveId,
        int oldStates,
        SlaveState newState)
    {
        auto slaveIt = slaves.find(slaveId);
        if (slaveIt != slaves.end() && (slaveIt->second.state & oldStates)) {
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

    double time = 0.0;
    const double maxTime = 10.0;
    const double stepSize = 1.0/100.0;
    bool terminate = false;
    const auto slaveCount = 2;

    // Define a variable to keep track of the state of the slaves in situations
    // where we want them all to be in the same state.
    enum AllSlavesState
    {
        ALL_SLAVES_OTHER,       // Neither
        ALL_SLAVES_READY,       // All slaves are in the READY state
        ALL_SLAVES_PUBLISHED,   // All slaves are in the PUBLISHED state
    };
    AllSlavesState allSlavesState = ALL_SLAVES_OTHER;

    // Main messaging loop
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
                if (UpdateSlaveState(slaves, slaveId, SLAVE_CONNECTING | SLAVE_INITIALIZING, SLAVE_INITIALIZING)) {
                    SendEmptyMessage(control, envelope, dsbproto::control::MSG_INIT_DONE);
                } else {
                    SendInvalidRequest(control, envelope);
                }
                break;

            case dsbproto::control::MSG_READY:
                std::clog << "MSG_READY" << std::endl;
                if (UpdateSlaveState(slaves, slaveId, SLAVE_INITIALIZING | SLAVE_READY | SLAVE_RECEIVING, SLAVE_READY)) {
                    // At least one slave is now in the READY state.  We
                    // assume they all are, and try to prove otherwise.
                    bool allReady = slaves.size() >= slaveCount;
                    if (allReady) {
                        BOOST_FOREACH (const auto& slave, slaves) {
                            if (slave.second.state != SLAVE_READY) {
                                allReady = false;
                                break;
                            }
                        }
                    }
                    if (allReady) allSlavesState = ALL_SLAVES_READY;
                    // Store the return envelope for later.
                    slaves[slaveId].envelope.swap(envelope);
                } else {
                    SendInvalidRequest(control, envelope);
                }
                break;

            case dsbproto::control::MSG_STEP_OK:
                std::clog << "MSG_STEP_OK" << std::endl;
                if (UpdateSlaveState(slaves, slaveId, SLAVE_STEPPING, SLAVE_PUBLISHED)) {
                    // At least one slave is now in the PUBLISHED state.  We
                    // assume that they all are, and try to prove otherwise.
                    bool allPublished = slaves.size() >= slaveCount;
                    if (allPublished) {
                        BOOST_FOREACH (const auto& slave, slaves) {
                            if (slave.second.isSimulating
                                && slave.second.state != SLAVE_PUBLISHED) {
                                allPublished = false;
                            }
                        }
                    }
                    if (allPublished) allSlavesState = ALL_SLAVES_PUBLISHED;
                    // Store the return envelope for later.
                    slaves[slaveId].envelope.swap(envelope);
                } else {
                    SendInvalidRequest(control, envelope);
                }
                break;

            case dsbproto::control::MSG_STEP_FAILED:
                std::clog << "MSG_STEP_FAILED" << std::endl;
                if (UpdateSlaveState(slaves, slaveId, SLAVE_STEPPING, SLAVE_STEP_FAILED)) {
                    dsb::control::CreateMessage(msg, dsbproto::control::MSG_TERMINATE);
                    dsb::comm::AddressedSend(control, envelope, msg);
                } else {
                    SendInvalidRequest(control, envelope);
                }
                break;

            default:
                std::clog << "Warning: Invalid message received from client: "
                          << slaveId << std::endl;
                SendInvalidRequest(control, envelope);
        }
        if (allSlavesState == ALL_SLAVES_READY) {
            if (terminate) {
                // Create a TERMINATE message
                std::deque<zmq::message_t> termMsg;
                dsb::control::CreateMessage(termMsg,  dsbproto::control::MSG_TERMINATE);

                // For each slave, make a copy of the TERMINATE message and send it.
                BOOST_FOREACH(auto& slave, slaves) {
                    std::deque<zmq::message_t> termMsgCopy;
                    dsb::comm::CopyMessage(termMsg, termMsgCopy);

                    // Send it on the "control" socket to the slave identified by "envelope"
                    assert (!slave.second.envelope.empty());
                    dsb::comm::AddressedSend(control, slave.second.envelope, termMsgCopy);
                }
                break;
            } else {
                // Create the STEP message body
                dsbproto::control::StepData stepData;
                stepData.set_timepoint(time);
                stepData.set_stepsize(stepSize);
                // Create the multipart STEP message
                std::deque<zmq::message_t> stepMsg;
                dsb::control::CreateMessage(stepMsg, dsbproto::control::MSG_STEP, stepData);

                // For each slave, make a copy of the STEP message and send it.
                BOOST_FOREACH(auto& slave, slaves) {
                    std::deque<zmq::message_t> stepMsgCopy;
                    dsb::comm::CopyMessage(stepMsg, stepMsgCopy);

                    // Send it on the "control" socket to the slave identified by "envelope"
                    assert (!slave.second.envelope.empty());
                    dsb::comm::AddressedSend(control, slave.second.envelope, stepMsgCopy);
#ifndef NDEBUG
                    auto rc =
#endif
                    UpdateSlaveState(slaves, slave.first, SLAVE_READY, SLAVE_STEPPING);
                    assert (rc);
                    slave.second.isSimulating = true;
                }
                time += stepSize;
                allSlavesState = ALL_SLAVES_OTHER;
                terminate = time >= maxTime; // temporary
            }
        } else if (allSlavesState == ALL_SLAVES_PUBLISHED) {
            // Create RECV_VARS message
            std::deque<zmq::message_t> recvVarsMsg;
            dsb::control::CreateMessage(recvVarsMsg, dsbproto::control::MSG_RECV_VARS);
            // Send this to all slaves which are in simulation
            BOOST_FOREACH (auto& slave, slaves) {
                if (slave.second.isSimulating) {
                    // Make a copy of the message and send it
                    std::deque<zmq::message_t> recvVarsMsgCopy;
                    dsb::comm::CopyMessage(recvVarsMsg, recvVarsMsgCopy);
                    assert (!slave.second.envelope.empty());
                    dsb::comm::AddressedSend(control, slave.second.envelope, recvVarsMsgCopy);
#ifndef NDEBUG
                    auto rc =
#endif
                    UpdateSlaveState(slaves, slave.first, SLAVE_PUBLISHED, SLAVE_RECEIVING);
                    assert (rc);
                }
            }
            allSlavesState = ALL_SLAVES_OTHER;
        }
    }

    // Give ZMQ time to send all TERMINATE messages
    std::cout << "Terminated. Press ENTER to quit." << std::endl;
    std::cin.ignore();
}
