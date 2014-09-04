#include "dsb/bus/slave_agent.hpp"

#include <limits>
#include <iostream> // TEMPORARY
#include <utility>

#include "dsb/comm.hpp"
#include "dsb/control.hpp"
#include "dsb/error.hpp"
#include "dsb/protobuf.hpp"
#include "dsb/util.hpp"


namespace
{
    uint16_t NormalMessageType(const std::deque<zmq::message_t>& msg)
    {
        const auto mt = dsb::control::NonErrorMessageType(msg);
        if (mt == dsbproto::control::MSG_TERMINATE) throw dsb::bus::Shutdown();
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
}


namespace dsb
{
namespace bus
{


SlaveAgent::SlaveAgent(
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


void SlaveAgent::Start(std::deque<zmq::message_t>& msg)
{
    dsb::control::CreateHelloMessage(msg, 0);
    m_stateHandler = &SlaveAgent::ConnectingHandler;
}


void SlaveAgent::RequestReply(std::deque<zmq::message_t>& msg)
{
    (this->*m_stateHandler)(msg);
}


void SlaveAgent::ConnectingHandler(std::deque<zmq::message_t>& msg)
{
    if (dsb::control::ParseProtocolVersion(msg.front()) != 0) {
        throw std::runtime_error("Master required unsupported protocol");
    }
    dsb::control::CreateMessage(msg, dsbproto::control::MSG_INIT_READY);
    m_stateHandler = &SlaveAgent::InitHandler;
}


void SlaveAgent::InitHandler(std::deque<zmq::message_t>& msg)
{
    EnforceMessageType(msg, dsbproto::control::MSG_INIT_DONE);
    dsb::control::CreateMessage(msg, dsbproto::control::MSG_READY);
    m_stateHandler = &SlaveAgent::ReadyHandler;
}


void SlaveAgent::ReadyHandler(std::deque<zmq::message_t>& msg)
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
                m_stateHandler = &SlaveAgent::PublishedHandler;
            } else {
                dsb::control::CreateMessage(msg, dsbproto::control::MSG_STEP_FAILED);
                m_stateHandler = &SlaveAgent::StepFailedHandler;
            }
            break; }
        default:
            throw dsb::error::ProtocolViolationException(
                "Invalid reply from master");
    }
}


void SlaveAgent::PublishedHandler(std::deque<zmq::message_t>& msg)
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
    m_stateHandler = &SlaveAgent::ReadyHandler;
}


void SlaveAgent::StepFailedHandler(std::deque<zmq::message_t>& msg)
{
    EnforceMessageType(msg, dsbproto::control::MSG_TERMINATE);
    // We never get here, because EnforceMessageType() always throws either
    // Shutdown or ProtocolViolationException.
    assert (false);
}


bool SlaveAgent::Step(const dsbproto::control::StepData& stepInfo)
{
    // Perform time step
    if (!m_slaveInstance->DoStep(stepInfo.timepoint(), stepInfo.stepsize())) {
        return false;
    }
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


}} // namespace
