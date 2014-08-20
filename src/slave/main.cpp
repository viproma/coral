#include <algorithm>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

#include "boost/chrono.hpp"
#include "boost/lexical_cast.hpp"
#include "boost/thread.hpp"

#include "dsb/comm.hpp"
#include "dsb/control.hpp"
#include "dsb/error.hpp"
#include "dsb/protobuf.hpp"
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
try {
    if (argc < 7) {
        std::cerr << "Usage: " << argv[0] << " <id> <control> <data pub> <data sub> <slave type> <other slave>\n"
                  << "  id          = a number in the range 0 - 65535\n"
                  << "  control     = Control socket endpoint (e.g. tcp://myhost:5432)\n"
                  << "  data pub    = Publisher socket endpoint\n"
                  << "  data sub    = Subscriber socket endpoint\n"
                  << "  slave type  = mass_1d or spring_1d\n"
                  << "  other slave = ID of other slave"
                  << std::endl;
        return 0;
    }
    const auto id = std::string(argv[1]);
    const auto controlEndpoint = std::string(argv[2]);
    const auto dataPubEndpoint = std::string(argv[3]);
    const auto dataSubEndpoint = std::string(argv[4]);
    const auto slaveType = std::string(argv[5]);
    const auto otherSlaveId = boost::lexical_cast<uint16_t>(argv[6]);

    auto slaveInstance = NewSlave(slaveType);

    auto context = zmq::context_t();
    auto control = zmq::socket_t(context, ZMQ_REQ);
    control.setsockopt(ZMQ_IDENTITY, id.data(), id.size());
    control.connect(controlEndpoint.c_str());
    auto dataPub = zmq::socket_t(context, ZMQ_PUB);
    dataPub.connect(dataPubEndpoint.c_str());
    auto dataSub = zmq::socket_t(context, ZMQ_SUB);
    dataSub.connect(dataSubEndpoint.c_str());

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

    // -------------------------------------------------------------------------
    // Temporary faked code
    const uint16_t inVarRef = 0;
    const uint16_t outVarRef = 1;
    const size_t dataHeaderSize = 4;

    // Build a header to use for subscribing to the other slave's output.
    char otherHeader[dataHeaderSize];
    dsb::util::EncodeUint16(otherSlaveId, otherHeader);
    dsb::util::EncodeUint16(outVarRef, otherHeader + 2);
    dataSub.setsockopt(ZMQ_SUBSCRIBE, otherHeader, dataHeaderSize);

    // Build a header to use for our own output.
    char myHeader[dataHeaderSize];
    dsb::util::EncodeUint16(boost::lexical_cast<uint16_t>(id), myHeader);
    dsb::util::EncodeUint16(outVarRef, myHeader + 2);
    // -------------------------------------------------------------------------

    // MSG_READY loop
    for (;;) {
        dsb::control::CreateMessage(msg, dsbproto::control::MSG_READY);
        dsb::comm::Send(control, msg);

        dsb::comm::Receive(control, msg);
        const auto msgType = NormalMessageType(msg);
        switch (msgType) {
            case dsbproto::control::MSG_STEP: {
                // Extract DoStep message from second (body) frame.
                assert (msg.size() == 2);
                dsbproto::control::StepData stepInfo;
                dsb::protobuf::ParseFromFrame(msg[1], stepInfo);

                // Perform time step
                slaveInstance->DoStep(stepInfo.timepoint(), stepInfo.stepsize());
                // Pretend to work really hard
                boost::this_thread::sleep_for(boost::chrono::milliseconds(10));
                const auto newTime = stepInfo.timepoint() + stepInfo.stepsize();
                std::cout << newTime << " " << slaveInstance->GetVariable(outVarRef) << std::endl;

                // Get value of output variable
                dsbproto::variable::TimestampedValue outVar;
                outVar.set_timestamp(newTime);
                outVar.mutable_value()->set_real_value(slaveInstance->GetVariable(outVarRef));

                // Build data message to be published
                std::deque<zmq::message_t> dataMsg;
                // Header
                dataMsg.push_back(zmq::message_t(dataHeaderSize));
                std::copy(myHeader, myHeader+dataHeaderSize,
                          static_cast<char*>(dataMsg.back().data()));
                // Body
                dataMsg.push_back(zmq::message_t());
                dsb::protobuf::SerializeToFrame(outVar, dataMsg.back());
                // Send it
                dsb::comm::Send(dataPub, dataMsg);

                // Send STEP_OK message
                std::deque<zmq::message_t> stepOkMsg;
                dsb::control::CreateMessage(stepOkMsg, dsbproto::control::MSG_STEP_OK);
                dsb::comm::Send(control, stepOkMsg);

                // Wait for RECV_VARS command
                std::deque<zmq::message_t> recvVarsMsg;
                dsb::comm::Receive(control, recvVarsMsg);
                EnforceMessageType(recvVarsMsg, dsbproto::control::MSG_RECV_VARS);

                // Receive message from other and store the body in inVar.
                dsb::comm::Receive(dataSub, dataMsg);
                dsbproto::variable::TimestampedValue inVar;
                dsb::protobuf::ParseFromFrame(dataMsg.back(), inVar);
                assert (inVar.timestamp() == newTime);

                // Set our input variable.
                slaveInstance->SetVariable(inVarRef, inVar.value().real_value());
                break; }
            default:
                throw dsb::error::ProtocolViolationException(
                    "Invalid reply from master");
        }
    }
} catch (const Shutdown& e) {
    std::cerr << "Shutdown: " << e.what() << std::endl;
}
}
