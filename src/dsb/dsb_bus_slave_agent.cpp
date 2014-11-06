#include "dsb/bus/slave_agent.hpp"

#include <iostream> // TEMPORARY
#include <limits>
#include <set>
#include <utility>

#include "boost/foreach.hpp"

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

    void InvalidReplyFromMaster()
    {
        throw dsb::error::ProtocolViolationException("Invalid reply from master");
    }

    void EnforceMessageType(
        const std::deque<zmq::message_t>& msg,
        dsbproto::control::MessageType expectedType)
    {
        if (NormalMessageType(msg) != expectedType) InvalidReplyFromMaster();
    }

    const size_t DATA_HEADER_SIZE = 4;
}


namespace dsb
{
namespace bus
{


SlaveAgent::SlaveAgent(
        uint16_t id,
        zmq::socket_t dataSub,
        zmq::socket_t dataPub,
        dsb::execution::ISlaveInstance& slaveInstance)
    : m_id(id),
      m_dataSub(std::move(dataSub)),
      m_dataPub(std::move(dataPub)),
      m_slaveInstance(slaveInstance),
      m_currentTime(std::numeric_limits<double>::signaling_NaN()),
      m_lastStepSize(std::numeric_limits<double>::signaling_NaN())
{
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
    if (dsb::control::ParseHelloMessage(msg) != 0) {
        throw std::runtime_error("Master required unsupported protocol");
    }
    dsb::control::CreateMessage(msg, dsbproto::control::MSG_SUBMIT);
    m_stateHandler = &SlaveAgent::ConnectedHandler;
}


void SlaveAgent::ConnectedHandler(std::deque<zmq::message_t>& msg)
{
    EnforceMessageType(msg, dsbproto::control::MSG_SETUP);
    if (msg.size() != 2) InvalidReplyFromMaster();
    dsbproto::control::SetupData data;
    dsb::protobuf::ParseFromFrame(msg[1], data);
    m_slaveInstance.Setup(
        data.start_time(),
        data.has_stop_time() ? data.stop_time() : std::numeric_limits<double>::infinity());
    std::clog << "Simulating from t = " << data.start_time()
              << " to " << (data.has_stop_time() ? data.stop_time() : std::numeric_limits<double>::infinity())
              << std::endl;
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
                // TODO: Remove this line when we implement proper handling of
                // failed steps.
                std::clog << "Step failed: t = " << stepData.timepoint()
                          << ", dt = " << stepData.stepsize() << std::endl;
                dsb::control::CreateMessage(msg, dsbproto::control::MSG_STEP_FAILED);
                m_stateHandler = &SlaveAgent::StepFailedHandler;
            }
            break; }
        case dsbproto::control::MSG_SET_VARS:
            HandleSetVars(msg);
            break;
        case dsbproto::control::MSG_CONNECT_VARS:
            HandleConnectVars(msg);
            break;
        default:
            InvalidReplyFromMaster();
    }
}


namespace
{
    void SetVariable(
        dsb::execution::ISlaveInstance& slaveInstance,
        uint16_t varRef,
        const dsbproto::variable::ScalarValue& value)
    {
        if (value.has_real_value()) {
            slaveInstance.SetRealVariable(varRef, value.real_value());
        } else if (value.has_integer_value()) {
            slaveInstance.SetIntegerVariable(varRef, value.integer_value());
        } else if (value.has_boolean_value()) {
            slaveInstance.SetBooleanVariable(varRef, value.boolean_value());
        } else if (value.has_string_value()) {
            slaveInstance.SetStringVariable(varRef, value.string_value());
        } else {
            assert (!"Corrupt or empty variable value received");
        }
    }
}


void SlaveAgent::PublishedHandler(std::deque<zmq::message_t>& msg)
{
    EnforceMessageType(msg, dsbproto::control::MSG_RECV_VARS);

    const auto allowedTimeError = m_lastStepSize * 1e-6;

    // Receive messages until all input variables have been set.
    std::set<uint16_t> receivedVars;
    while (receivedVars.size() < m_connections.size()) {
        // Receive message from other and store the body in inVar.
        std::deque<zmq::message_t> dataMsg;
        dsb::comm::Receive(m_dataSub, dataMsg);
        assert (dataMsg.size() == 2
                && "Wrong number of frames in data message");

        dsbproto::variable::TimestampedValue inVar;
        dsb::protobuf::ParseFromFrame(dataMsg.back(), inVar);
        assert (inVar.timestamp() < m_currentTime + allowedTimeError
                && "Data received from the future");

        // If the message has been queued up from a previous time
        // step, which could happen if we have joined the simulation
        // while it's in progress, discard it and retry.
        if (inVar.timestamp() < m_currentTime - allowedTimeError) continue;

        // Decode header.
        RemoteVariable rv = {
            dsb::util::DecodeUint16(static_cast<const char*>(dataMsg[0].data())),
            dsb::util::DecodeUint16(static_cast<const char*>(dataMsg[0].data())+2)
        };
        const auto inVarRef = m_connections.at(rv);

        // Set our input variable.
        SetVariable(m_slaveInstance, inVarRef, inVar.value());
        receivedVars.insert(inVarRef);
    }

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


// TODO: Make this function signature more consistent with Step() (or the other
// way around).
void SlaveAgent::HandleSetVars(std::deque<zmq::message_t>& msg)
{
    if (msg.size() != 2) {
        throw dsb::error::ProtocolViolationException(
            "Wrong number of frames in SET_VARS message");
    }
    dsbproto::control::SetVarsData data;
    dsb::protobuf::ParseFromFrame(msg[1], data);
    BOOST_FOREACH (const auto& var, data.variable()) {
        SetVariable(m_slaveInstance, var.id(), var.value());
    }
    dsb::control::CreateMessage(msg, dsbproto::control::MSG_READY);
}


// TODO: Make this function signature more consistent with Step() (or the other
// way around).
void SlaveAgent::HandleConnectVars(std::deque<zmq::message_t>& msg)
{
    if (msg.size() != 2) {
        throw dsb::error::ProtocolViolationException(
            "Wrong number of frames in CONNECT_VARS message");
    }
    dsbproto::control::ConnectVarsData data;
    dsb::protobuf::ParseFromFrame(msg[1], data);
    BOOST_FOREACH (const auto& var, data.connection()) {
        // Look for any existing connection to the input variable, and
        // unsubscribe and remove it.
        for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
            if (it->second == var.input_var_id()) {
                char oldHeader[DATA_HEADER_SIZE];
                dsb::util::EncodeUint16(it->first.slave, oldHeader);
                dsb::util::EncodeUint16(it->first.var,   oldHeader + 2);
                m_dataSub.setsockopt(ZMQ_UNSUBSCRIBE, oldHeader, DATA_HEADER_SIZE);
                m_connections.erase(it);
                break;
            }
        }
        // Make the new connection and subscription.
        RemoteVariable rv = {
            var.output_var().slave_id(),
            var.output_var().var_id()
        };
        char newHeader[DATA_HEADER_SIZE];
        dsb::util::EncodeUint16(rv.slave, newHeader);
        dsb::util::EncodeUint16(rv.var,   newHeader + 2);
        m_dataSub.setsockopt(ZMQ_SUBSCRIBE, newHeader, DATA_HEADER_SIZE);
        m_connections.insert(std::make_pair(rv, var.input_var_id()));
    }
    dsb::control::CreateMessage(msg, dsbproto::control::MSG_READY);
}


bool SlaveAgent::Step(const dsbproto::control::StepData& stepInfo)
{
    // Perform time step
    if (!m_slaveInstance.DoStep(stepInfo.timepoint(), stepInfo.stepsize())) {
        return false;
    }
    m_currentTime = stepInfo.timepoint() + stepInfo.stepsize();
    m_lastStepSize = stepInfo.stepsize();

    for (size_t i = 0; i < m_slaveInstance.VariableCount(); ++i) {
        const auto varInfo = m_slaveInstance.Variable(i);
        if (varInfo.Causality() != dsb::model::OUTPUT_CAUSALITY) continue;

        // Get value of output variable
        dsbproto::variable::TimestampedValue outVar;
        outVar.set_timestamp(m_currentTime);
        switch (varInfo.DataType()) {
            case dsb::model::REAL_DATATYPE:
                outVar.mutable_value()->set_real_value(
                    m_slaveInstance.GetRealVariable(varInfo.ID()));
                break;
            case dsb::model::INTEGER_DATATYPE:
                outVar.mutable_value()->set_integer_value(
                    m_slaveInstance.GetIntegerVariable(varInfo.ID()));
                break;
            case dsb::model::BOOLEAN_DATATYPE:
                outVar.mutable_value()->set_boolean_value(
                    m_slaveInstance.GetBooleanVariable(varInfo.ID()));
                break;
            case dsb::model::STRING_DATATYPE:
                outVar.mutable_value()->set_string_value(
                    m_slaveInstance.GetStringVariable(varInfo.ID()));
                break;
            default:
                assert (false);
        }

        // Build data message to be published
        std::deque<zmq::message_t> dataMsg;
        // Header
        dataMsg.push_back(zmq::message_t(DATA_HEADER_SIZE));
        dsb::util::EncodeUint16(m_id, static_cast<char*>(dataMsg.front().data()));
        dsb::util::EncodeUint16(varInfo.ID(), static_cast<char*>(dataMsg.front().data()) + 2);
        // Body
        dataMsg.push_back(zmq::message_t());
        dsb::protobuf::SerializeToFrame(outVar, dataMsg.back());
        // Send it
        dsb::comm::Send(m_dataPub, dataMsg);
    }
    return true;
}


}} // namespace
