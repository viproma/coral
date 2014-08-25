#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
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


/**
\brief  A class which contains the state of the slave and takes care of
        responding to requests from the master node in an appropriate manner.
*/
class Slave
{
public:
    /**
    \brief  Constructs a new Slave.

    \param [in] id              The slave ID.
    \param [in] dataSub         A SUB socket to be used for receiving variables.
    \param [in] dataPub         A PUB socket to be used for sending variables.
    \param [in] slaveInstance   (Temporary) A pointer to the object which
                                contains the slave's mathematical model.
    \param [in] otherSlaveId    (Temporary) The ID of the slave which this
                                slave should be connected to.
    */
    Slave(
        uint16_t id,
        zmq::socket_t dataSub,
        zmq::socket_t dataPub,
        std::unique_ptr<ISlaveInstance> slaveInstance,
        uint16_t otherSlaveId
        );

    /**
    \brief  Prepares the first message (HELLO) which is to be sent to the master
            and stores it in `msg`.
    */
    void Start(std::deque<zmq::message_t>& msg);

    /**
    \brief  Responds to a message from the master.
    
    On input, `msg` must be the message received from master, and on output,
    it will contain the slave's reply.  Internally, the function forwards to
    the handler function that corresponds to the slave's current state.
    */
    void RequestReply(std::deque<zmq::message_t>& msg);

private:
    // Each of these functions correspond to one of the slave's possible states.
    // On input, `msg` is a message from the master node, and when the function
    // returns, `msg` must contain the reply.  If the message triggers a state
    // change, the handler function must update m_stateHandler to point to the
    // function for the new state.
    void ConnectingHandler(std::deque<zmq::message_t>& msg);
    void InitHandler(std::deque<zmq::message_t>& msg);
    void ReadyHandler(std::deque<zmq::message_t>& msg);
    void PublishedHandler(std::deque<zmq::message_t>& msg);
    void StepFailedHandler(std::deque<zmq::message_t>& msg);

    // Performs the time step for ReadyHandler()
    bool Step(const dsbproto::control::StepData& stepData);

    // A pointer to the handler function for the current state.
    void (Slave::* m_stateHandler)(std::deque<zmq::message_t>&);

    zmq::socket_t m_dataSub;
    zmq::socket_t m_dataPub;
    std::unique_ptr<ISlaveInstance> m_slaveInstance;
    double m_currentTime;
    double m_lastStepSize;

    // -------------------------------------------------------------------------
    // Temporary
    static const uint16_t IN_VAR_REF = 0;
    static const uint16_t OUT_VAR_REF = 1;
    static const size_t DATA_HEADER_SIZE = 4;

    char otherHeader[DATA_HEADER_SIZE];
    char myHeader[DATA_HEADER_SIZE];
};


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

    auto context = zmq::context_t();
    auto control = zmq::socket_t(context, ZMQ_REQ);
    control.setsockopt(ZMQ_IDENTITY, id.data(), id.size());
    control.connect(controlEndpoint.c_str());
    auto dataPub = zmq::socket_t(context, ZMQ_PUB);
    dataPub.connect(dataPubEndpoint.c_str());
    auto dataSub = zmq::socket_t(context, ZMQ_SUB);
    dataSub.connect(dataSubEndpoint.c_str());

    Slave slave(boost::lexical_cast<uint16_t>(id),
                std::move(dataSub),
                std::move(dataPub),
                NewSlave(slaveType),
                otherSlaveId);
    std::deque<zmq::message_t> msg;
    slave.Start(msg);
    for (;;) {
        dsb::comm::Send(control, msg);
        dsb::comm::Receive(control, msg);
        slave.RequestReply(msg);
    }
} catch (const Shutdown& e) {
    std::cerr << "Shutdown: " << e.what() << std::endl;
} catch (const dsb::control::RemoteErrorException& e) {
    std::cerr << "Received ERROR message: " << e.what() << std::endl;
}
}


// =============================================================================
// Slave class function implementations
// =============================================================================

Slave::Slave(
        uint16_t id,
        zmq::socket_t dataSub,
        zmq::socket_t dataPub,
        std::unique_ptr<ISlaveInstance> slaveInstance,
        uint16_t otherSlaveId)
    : m_dataSub(std::move(dataSub)),
      m_dataPub(std::move(dataPub)),
      m_slaveInstance(std::move(slaveInstance)),
      m_currentTime(std::numeric_limits<double>::signaling_NaN()),
      m_lastStepSize(std::numeric_limits<double>::signaling_NaN())
{

    // -------------------------------------------------------------------------
    // Temporary

    // Build a header to use for subscribing to the other slave's output.
    dsb::util::EncodeUint16(otherSlaveId, otherHeader);
    dsb::util::EncodeUint16(OUT_VAR_REF, otherHeader + 2);
    m_dataSub.setsockopt(ZMQ_SUBSCRIBE, otherHeader, DATA_HEADER_SIZE);

    // Build a header to use for our own output.
    dsb::util::EncodeUint16(id, myHeader);
    dsb::util::EncodeUint16(OUT_VAR_REF, myHeader + 2);
    // -------------------------------------------------------------------------
}


void Slave::Start(std::deque<zmq::message_t>& msg)
{
    dsb::control::CreateHelloMessage(msg, 0);
    m_stateHandler = &Slave::ConnectingHandler;
}


void Slave::RequestReply(std::deque<zmq::message_t>& msg)
{
    (this->*m_stateHandler)(msg);
}


void Slave::ConnectingHandler(std::deque<zmq::message_t>& msg)
{
    if (dsb::control::ParseProtocolVersion(msg.front()) != 0) {
        throw std::runtime_error("Master required unsupported protocol");
    }
    dsb::control::CreateMessage(msg, dsbproto::control::MSG_INIT_READY);
    m_stateHandler = &Slave::InitHandler;
}


void Slave::InitHandler(std::deque<zmq::message_t>& msg)
{
    EnforceMessageType(msg, dsbproto::control::MSG_INIT_DONE);
    dsb::control::CreateMessage(msg, dsbproto::control::MSG_READY);
    m_stateHandler = &Slave::ReadyHandler;
}


void Slave::ReadyHandler(std::deque<zmq::message_t>& msg)
{
    switch (NormalMessageType(msg)) {
        case dsbproto::control::MSG_STEP: {
            if (msg.size() != 2) {
                throw dsb::error::ProtocolViolationException(
                    "Wrong number of frames in STEP message");
            }
            dsbproto::control::StepData stepData;
            dsb::protobuf::ParseFromFrame(msg[1], stepData);
            if (Step(stepData)) {
                dsb::control::CreateMessage(msg, dsbproto::control::MSG_STEP_OK);
                m_stateHandler = &Slave::PublishedHandler;
            } else {
                dsb::control::CreateMessage(msg, dsbproto::control::MSG_STEP_FAILED);
                m_stateHandler = &Slave::StepFailedHandler;
            }
            break; }
        default:
            throw dsb::error::ProtocolViolationException(
                "Invalid reply from master");
    }
}


void Slave::PublishedHandler(std::deque<zmq::message_t>& msg)
{
    EnforceMessageType(msg, dsbproto::control::MSG_RECV_VARS);

    // Receive message from other and store the body in inVar.
    const auto allowedTimeError = m_lastStepSize * 1e-6;
    std::deque<zmq::message_t> dataMsg;
    dsbproto::variable::TimestampedValue inVar;
    do {
        dsb::comm::Receive(m_dataSub, dataMsg);
        dsb::protobuf::ParseFromFrame(dataMsg.back(), inVar);
        assert (inVar.timestamp() < m_currentTime + allowedTimeError
                && "Data received from the future");
        // If the message has been queued up from a previous time
        // step, which could happen if we have joined the simulation
        // while it's in progress, discard it and retry.
    } while (inVar.timestamp() < m_currentTime - allowedTimeError);

    // Set our input variable.
    m_slaveInstance->SetVariable(IN_VAR_REF, inVar.value().real_value());

    // Send READY message and change state again.
    dsb::control::CreateMessage(msg, dsbproto::control::MSG_READY);
    m_stateHandler = &Slave::ReadyHandler;
}


void Slave::StepFailedHandler(std::deque<zmq::message_t>& msg)
{
    EnforceMessageType(msg, dsbproto::control::MSG_TERMINATE);
    // We never get here, because EnforceMessageType() always throws either
    // Shutdown or ProtocolViolationException.
    assert (false);
}


bool Slave::Step(const dsbproto::control::StepData& stepInfo)
{
    // Perform time step
    if (!m_slaveInstance->DoStep(stepInfo.timepoint(), stepInfo.stepsize())) {
        return false;
    }
    // Pretend to work really hard
    boost::this_thread::sleep_for(boost::chrono::milliseconds(10));
    m_currentTime = stepInfo.timepoint() + stepInfo.stepsize();
    m_lastStepSize = stepInfo.stepsize();
    std::cout << m_currentTime << " " << m_slaveInstance->GetVariable(OUT_VAR_REF) << std::endl;

    // Get value of output variable
    dsbproto::variable::TimestampedValue outVar;
    outVar.set_timestamp(m_currentTime);
    outVar.mutable_value()->set_real_value(m_slaveInstance->GetVariable(OUT_VAR_REF));

    // Build data message to be published
    std::deque<zmq::message_t> dataMsg;
    // Header
    dataMsg.push_back(zmq::message_t(DATA_HEADER_SIZE));
    std::copy(myHeader, myHeader+DATA_HEADER_SIZE,
                static_cast<char*>(dataMsg.back().data()));
    // Body
    dataMsg.push_back(zmq::message_t());
    dsb::protobuf::SerializeToFrame(outVar, dataMsg.back());
    // Send it
    dsb::comm::Send(m_dataPub, dataMsg);
    return true;
}
