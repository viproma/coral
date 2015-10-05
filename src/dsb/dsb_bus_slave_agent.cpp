#include "dsb/bus/slave_agent.hpp"

#include <cassert>
#include <iostream> // TEMPORARY
#include <limits>
#include <set>
#include <utility>

#include "boost/foreach.hpp"
#include "boost/numeric/conversion/cast.hpp"

#include "dsb/comm/messaging.hpp"
#include "dsb/comm/util.hpp"
#include "dsb/compat_helpers.hpp"
#include "dsb/execution/slave.hpp"
#include "dsb/error.hpp"
#include "dsb/protobuf.hpp"
#include "dsb/protocol/execution.hpp"
#include "dsb/util.hpp"


namespace
{
    uint16_t NormalMessageType(const std::deque<zmq::message_t>& msg)
    {
        const auto mt = dsb::protocol::execution::NonErrorMessageType(msg);
        if (mt == dsbproto::execution::MSG_TERMINATE) throw dsb::bus::Shutdown();
        return mt;
    }

    void InvalidReplyFromMaster()
    {
        throw dsb::error::ProtocolViolationException("Invalid reply from master");
    }

    void EnforceMessageType(
        const std::deque<zmq::message_t>& msg,
        dsbproto::execution::MessageType expectedType)
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
    dsb::comm::Reactor& reactor,
    dsb::execution::ISlaveInstance& slaveInstance,
    const dsb::comm::P2PEndpoint& bindpoint,
    boost::chrono::milliseconds commTimeout)
    : m_stateHandler(&SlaveAgent::NotConnectedHandler),
      m_slaveInstance(slaveInstance),
      m_commTimeout(commTimeout),
      m_control(),
      m_dataSub(),
      m_dataPub(),
      m_id(dsb::model::INVALID_SLAVE_ID),
      m_currentTime(std::numeric_limits<double>::signaling_NaN()),
      m_lastStepSize(std::numeric_limits<double>::signaling_NaN())
{
    m_control.Bind(bindpoint);
    reactor.AddSocket(
        m_control.Socket(),
        [this](dsb::comm::Reactor& r, zmq::socket_t& s) {
            assert(&s == &m_control.Socket());
            std::deque<zmq::message_t> msg;
            if (!dsb::comm::Receive(m_control, msg, m_commTimeout)) {
                throw dsb::execution::TimeoutException(
                    boost::chrono::duration_cast<boost::chrono::seconds>(m_commTimeout));
            }
            try {
                RequestReply(msg);
            } catch (const dsb::bus::Shutdown&) {
                // TODO: Handle slave shutdown via other means?
                r.Stop();
                return;
            }
            m_control.Send(msg);
        });
}


const dsb::comm::P2PEndpoint& SlaveAgent::BoundEndpoint() const
{
    return m_control.BoundEndpoint();
}


void SlaveAgent::RequestReply(std::deque<zmq::message_t>& msg)
{
    (this->*m_stateHandler)(msg);
}


void SlaveAgent::NotConnectedHandler(std::deque<zmq::message_t>& msg)
{
    if (dsb::protocol::execution::ParseHelloMessage(msg) != 0) {
        throw std::runtime_error("Master required unsupported protocol");
    }
    dsb::protocol::execution::CreateHelloMessage(msg, 0);
    m_stateHandler = &SlaveAgent::ConnectedHandler;
}


void SlaveAgent::ConnectedHandler(std::deque<zmq::message_t>& msg)
{
    EnforceMessageType(msg, dsbproto::execution::MSG_SETUP);
    if (msg.size() != 2) InvalidReplyFromMaster();
    dsbproto::execution::SetupData data;
    dsb::protobuf::ParseFromFrame(msg[1], data);
    m_id = data.slave_id();
    m_slaveInstance.Setup(
        data.start_time(),
        data.has_stop_time() ? data.stop_time() : std::numeric_limits<double>::infinity());
    m_dataPub = std::make_unique<zmq::socket_t>(dsb::comm::GlobalContext(), ZMQ_PUB);
    m_dataPub->connect(data.variable_pub_endpoint().c_str());
    m_dataSub = std::make_unique<zmq::socket_t>(dsb::comm::GlobalContext(), ZMQ_SUB);
    m_dataSub->connect(data.variable_sub_endpoint().c_str());
    std::clog << "Simulating from t = " << data.start_time()
              << " to " << (data.has_stop_time() ? data.stop_time() : std::numeric_limits<double>::infinity())
              << std::endl;
    dsb::protocol::execution::CreateMessage(msg, dsbproto::execution::MSG_READY);
    m_stateHandler = &SlaveAgent::ReadyHandler;
}


void SlaveAgent::ReadyHandler(std::deque<zmq::message_t>& msg)
{
    switch (NormalMessageType(msg)) {
        case dsbproto::execution::MSG_STEP: {
            if (msg.size() != 2) {
                throw dsb::error::ProtocolViolationException(
                    "Wrong number of frames in STEP message");
            }
            dsbproto::execution::StepData stepData;
            dsb::protobuf::ParseFromFrame(msg[1], stepData);
            if (Step(stepData)) {
                dsb::protocol::execution::CreateMessage(msg, dsbproto::execution::MSG_STEP_OK);
                m_stateHandler = &SlaveAgent::PublishedHandler;
            } else {
                dsb::protocol::execution::CreateMessage(msg, dsbproto::execution::MSG_STEP_FAILED);
                m_stateHandler = &SlaveAgent::StepFailedHandler;
            }
            break; }
        case dsbproto::execution::MSG_SET_VARS:
            HandleSetVars(msg);
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
        const dsbproto::model::ScalarValue& value)
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
    EnforceMessageType(msg, dsbproto::execution::MSG_ACCEPT_STEP);

    const auto allowedTimeError = m_lastStepSize * 1e-6;

    // Receive messages until all input variables have been set.
    std::set<uint16_t> receivedVars;
    while (receivedVars.size() < m_connections.size()) {
        // Receive message from other and store the body in inVar.
        std::deque<zmq::message_t> dataMsg;
        dsb::comm::Receive(*m_dataSub, dataMsg);
        assert (dataMsg.size() == 2
                && "Wrong number of frames in data message");

        dsbproto::execution::TimestampedValue inVar;
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
    dsb::protocol::execution::CreateMessage(msg, dsbproto::execution::MSG_READY);
    m_stateHandler = &SlaveAgent::ReadyHandler;
}


void SlaveAgent::StepFailedHandler(std::deque<zmq::message_t>& msg)
{
    EnforceMessageType(msg, dsbproto::execution::MSG_TERMINATE);
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
    dsbproto::execution::SetVarsData data;
    dsb::protobuf::ParseFromFrame(msg[1], data);
    BOOST_FOREACH (const auto& var, data.variable()) {
        // TODO: Remove debug output
        if (var.has_value()) {
            std::clog << "Setting value of var " << var.variable_id() << std::endl;
            SetVariable(m_slaveInstance, var.variable_id(), var.value());
        }
        if (var.has_connected_output()) {
            std::clog << "Connecting var " << var.variable_id() << " to slave "
                << var.connected_output().slave_id() << ", var "
                << var.connected_output().variable_id() << std::endl;
            // Look for any existing connection to the input variable, and
            // unsubscribe and remove it.
            for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
                if (it->second == var.variable_id()) {
                    char oldHeader[DATA_HEADER_SIZE];
                    dsb::util::EncodeUint16(it->first.slave, oldHeader);
                    dsb::util::EncodeUint16(it->first.var,   oldHeader + 2);
                    m_dataSub->setsockopt(ZMQ_UNSUBSCRIBE, oldHeader, DATA_HEADER_SIZE);
                    m_connections.erase(it);
                    break;
                }
            }
            // Make the new connection and subscription.
            RemoteVariable rv = {
                boost::numeric_cast<uint16_t>(var.connected_output().slave_id()),
                boost::numeric_cast<uint16_t>(var.connected_output().variable_id())
            };
            char newHeader[DATA_HEADER_SIZE];
            dsb::util::EncodeUint16(rv.slave, newHeader);
            dsb::util::EncodeUint16(rv.var,   newHeader + 2);
            m_dataSub->setsockopt(ZMQ_SUBSCRIBE, newHeader, DATA_HEADER_SIZE);
            m_connections.insert(std::make_pair(rv, var.variable_id()));
        }
    }
    dsb::protocol::execution::CreateMessage(msg, dsbproto::execution::MSG_READY);
}


bool SlaveAgent::Step(const dsbproto::execution::StepData& stepInfo)
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
        dsbproto::execution::TimestampedValue outVar;
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
        dsb::comm::Send(*m_dataPub, dataMsg);
    }
    return true;
}


}} // namespace
