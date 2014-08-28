#include <cassert>
#include <deque>
#include <iostream>
#include <map>
#include <string>

#include "boost/foreach.hpp"
#include "zmq.hpp"

#include "dsb/comm.hpp"
#include "dsb/control.hpp"
#include "control.pb.h"

#include "slave_handler.hpp"


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

    // Make a list of expected slaves, hardcoded for the time being.
    std::map<std::string, SlaveHandler> slaves;
    slaves["1"] = SlaveHandler();
    slaves["2"] = SlaveHandler();
//    slaves["3"] = SlaveHandler();

    // Main messaging loop
    for (;;) {
        // Read an incoming message, split out the return envelope from it,
        // and extract the slave ID from the envelope.
        std::deque<zmq::message_t> msg;
        dsb::comm::Receive(control, msg);
        std::deque<zmq::message_t> envelope;
        dsb::comm::PopMessageEnvelope(msg, &envelope);
        const auto slaveId = dsb::comm::ToString(envelope.back());
        std::clog << "Received message from slave '" << slaveId << "': ";

        // Pass on the message to the appropriate slave handler, send the
        // reply immediately if necessary.
        if (!slaves[slaveId].RequestReply(control, envelope, msg)) {
            // No immediate reply sent.  Instead, we check for a common state
            // (e.g. READY or PUBLISHED) to determine whether it is time to
            // send a STEP message or similar to all slaves.

            // TODO: Find a more elegant way to determine the common state,
            // because this kind of sucks.
            bool allReady = true;
            bool allPublished = true;
            bool anySimulating = false;
            BOOST_FOREACH (const auto& slave, slaves) {
                if (slave.second.State() != SLAVE_READY) {
                    allReady = false;
                }
                if (slave.second.IsSimulating()) {
                    anySimulating = true;
                }
                if (anySimulating && slave.second.State() != SLAVE_PUBLISHED) {
                    allPublished = false;
                }
            }
            allPublished = anySimulating && allPublished;
            assert (!(allReady && allPublished));

            if (allReady) {
                if (terminate) {
                    // Create a TERMINATE message
                    std::deque<zmq::message_t> termMsg;
                    dsb::control::CreateMessage(termMsg, dsbproto::control::MSG_TERMINATE);

                    // For each slave, make a copy of the TERMINATE message and send it.
                    BOOST_FOREACH(auto& slave, slaves) {
                        std::deque<zmq::message_t> termMsgCopy;
                        dsb::comm::CopyMessage(termMsg, termMsgCopy);
                        slave.second.SendTerminate(control, termMsgCopy);
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
                        slave.second.SendStep(control, stepMsgCopy);
                    }
                    time += stepSize;
                    terminate = time >= maxTime; // temporary
                }
            } else if (allPublished) {
                // Create RECV_VARS message
                std::deque<zmq::message_t> recvVarsMsg;
                dsb::control::CreateMessage(recvVarsMsg, dsbproto::control::MSG_RECV_VARS);
                // Send this to all slaves which are in simulation
                BOOST_FOREACH (auto& slave, slaves) {
                    if (slave.second.IsSimulating()) {
                        // Make a copy of the message and send it
                        std::deque<zmq::message_t> recvVarsMsgCopy;
                        dsb::comm::CopyMessage(recvVarsMsg, recvVarsMsgCopy);
                        slave.second.SendRecvVars(control, recvVarsMsgCopy);
                    }
                }
            }
        }
    }

    // Give ZMQ time to send all TERMINATE messages
    std::cout << "Terminated. Press ENTER to quit." << std::endl;
    std::cin.ignore();
}
