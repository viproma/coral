/*
Copyright 2013-present, SINTEF Ocean.
This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#include <coral/bus/slave_agent.hpp>

#include <cassert>
#include <limits>
#include <utility>

#include <coral/error.hpp>
#include <coral/log.hpp>
#include <coral/net/zmqx.hpp>
#include <coral/protobuf.hpp>
#include <coral/protocol/execution.hpp>
#include <coral/protocol/glue.hpp>
#include <coral/slave/exception.hpp>
#include <coral/util.hpp>


namespace
{
    uint16_t NormalMessageType(const std::vector<zmq::message_t>& msg)
    {
        const auto mt = coral::protocol::execution::NonErrorMessageType(msg);
        CORAL_LOG_TRACE(boost::format("Received %s")
            % coralproto::execution::MessageType_Name(
                static_cast<coralproto::execution::MessageType>(mt)));
        if (mt == coralproto::execution::MSG_TERMINATE) throw coral::bus::Shutdown();
        return mt;
    }

    void InvalidReplyFromMaster()
    {
        throw coral::error::ProtocolViolationException("Invalid reply from master");
    }

    void EnforceMessageType(
        const std::vector<zmq::message_t>& msg,
        coralproto::execution::MessageType expectedType)
    {
        if (NormalMessageType(msg) != expectedType) InvalidReplyFromMaster();
    }

    const size_t DATA_HEADER_SIZE = 4;
}


namespace coral
{
namespace bus
{


SlaveAgent::SlaveAgent(
    coral::net::Reactor& reactor,
    coral::slave::Instance& slaveInstance,
    const coral::net::Endpoint& controlEndpoint,
    const coral::net::Endpoint& dataPubEndpoint,
    std::chrono::milliseconds masterInactivityTimeout)
    : m_stateHandler(&SlaveAgent::NotConnectedHandler),
      m_slaveInstance(slaveInstance),
      m_masterInactivityTimeout(reactor, masterInactivityTimeout),
      m_variableRecvTimeout(std::chrono::seconds(1)),
      m_id(coral::model::INVALID_SLAVE_ID),
      m_currentStepID(coral::model::INVALID_STEP_ID)
{
    m_control.Bind(controlEndpoint);
    CORAL_LOG_TRACE("Slave bound to control endpoint: " + BoundControlEndpoint().URL());

    m_publisher.Bind(dataPubEndpoint);
    CORAL_LOG_TRACE("Slave bound to data publisher endpoint: " + BoundDataPubEndpoint().URL());

    reactor.AddSocket(
        m_control.Socket(),
        [this](coral::net::Reactor& r, zmq::socket_t& s) {
            assert(&s == &m_control.Socket());
            m_masterInactivityTimeout.Reset();
            std::vector<zmq::message_t> msg;
            try {
                m_control.Receive(msg);
                RequestReply(msg);
            } catch (const coral::bus::Shutdown&) {
                r.Stop();
                return;
            } catch (const zmq::error_t&) {
                throw; // Not much we can do about this...
            } catch (const std::runtime_error& e) {
                coral::protocol::execution::CreateFatalErrorMessage(
                    msg,
                    coralproto::execution::ErrorInfo::UNSPECIFIED_ERROR,
                    e.what());
                m_control.Send(msg);
                throw;
            }
#ifdef CORAL_LOG_TRACE_ENABLED
            const auto replyType = static_cast<coralproto::execution::MessageType>(
                coral::protocol::execution::ParseMessageType(msg.front()));
#endif
            m_control.Send(msg);
            CORAL_LOG_TRACE(boost::format("Sent %s")
                % coralproto::execution::MessageType_Name(replyType));
        });
}


coral::net::Endpoint SlaveAgent::BoundControlEndpoint() const
{
    return coral::net::Endpoint{m_control.BoundEndpoint().URL()};
}


coral::net::Endpoint SlaveAgent::BoundDataPubEndpoint() const
{
    return m_publisher.BoundEndpoint();
}


void SlaveAgent::RequestReply(std::vector<zmq::message_t>& msg)
{
    (this->*m_stateHandler)(msg);
}


void SlaveAgent::NotConnectedHandler(std::vector<zmq::message_t>& msg)
{
    CORAL_LOG_TRACE("NOT CONNECTED state: incoming message");
    if (coral::protocol::execution::ParseHelloMessage(msg) != 0) {
        throw std::runtime_error("Master required unsupported protocol");
    }
    CORAL_LOG_TRACE("Received HELLO");
    coral::protocol::execution::CreateHelloMessage(msg, 0);
    m_stateHandler = &SlaveAgent::ConnectedHandler;
}


void SlaveAgent::ConnectedHandler(std::vector<zmq::message_t>& msg)
{
    CORAL_LOG_TRACE("CONNECTED state: incoming message");
    EnforceMessageType(msg, coralproto::execution::MSG_SETUP);
    if (msg.size() != 2) InvalidReplyFromMaster();

    coralproto::execution::SetupData data;
    coral::protobuf::ParseFromFrame(msg[1], data);
    CORAL_LOG_DEBUG(boost::format("Slave name (ID): %s (%d)")
        % data.slave_name() % data.slave_id());
    CORAL_LOG_DEBUG(boost::format("Simulation time frame: %g to %g")
        % data.start_time()
        % (data.has_stop_time() ? data.stop_time() : std::numeric_limits<double>::infinity()));
    m_id = data.slave_id();
    m_slaveInstance.Setup(
        data.slave_name(),
        data.execution_name(),
        data.start_time(),
        data.has_stop_time() ? data.stop_time() : std::numeric_limits<double>::infinity(),
        false,
        1.0 /* not used */);

    if (data.has_variable_recv_timeout_ms()) {
        m_variableRecvTimeout =
            std::chrono::milliseconds(data.variable_recv_timeout_ms());
    }

    coral::protocol::execution::CreateMessage(msg, coralproto::execution::MSG_READY);
    m_stateHandler = &SlaveAgent::ReadyHandler;
}


void SlaveAgent::ReadyHandler(std::vector<zmq::message_t>& msg)
{
    CORAL_LOG_TRACE("READY state: incoming message");
    switch (NormalMessageType(msg)) {
        case coralproto::execution::MSG_STEP: {
            if (msg.size() != 2) {
                throw coral::error::ProtocolViolationException(
                    "Wrong number of frames in STEP message");
            }
            coralproto::execution::StepData stepData;
            coral::protobuf::ParseFromFrame(msg[1], stepData);
            if (Step(stepData)) {
                coral::protocol::execution::CreateMessage(msg, coralproto::execution::MSG_STEP_OK);
                m_stateHandler = &SlaveAgent::PublishedHandler;
            } else {
                coral::protocol::execution::CreateMessage(msg, coralproto::execution::MSG_STEP_FAILED);
                m_stateHandler = &SlaveAgent::StepFailedHandler;
            }
            break; }
        case coralproto::execution::MSG_SET_VARS:
            HandleSetVars(msg);
            break;
        case coralproto::execution::MSG_SET_PEERS:
            HandleSetPeers(msg);
            break;
        case coralproto::execution::MSG_DESCRIBE:
            HandleDescribe(msg);
            break;
        case coralproto::execution::MSG_RESEND_VARS:
            HandleResendVars(msg);
            break;
        default:
            InvalidReplyFromMaster();
    }
}


void SlaveAgent::PublishedHandler(std::vector<zmq::message_t>& msg)
{
    CORAL_LOG_TRACE("STEP OK state: incoming message");
    EnforceMessageType(msg, coralproto::execution::MSG_ACCEPT_STEP);
    // TODO: Use a different timeout here?
    if (!m_connections.Update(m_slaveInstance, m_currentStepID, m_variableRecvTimeout)) {
        throw std::runtime_error("Timeout waiting for variable values from other slaves");
    }
    coral::protocol::execution::CreateMessage(msg, coralproto::execution::MSG_READY);
    m_stateHandler = &SlaveAgent::ReadyHandler;
}


void SlaveAgent::StepFailedHandler(std::vector<zmq::message_t>& msg)
{
    CORAL_LOG_TRACE("STEP FAILED state: incoming message");
    EnforceMessageType(msg, coralproto::execution::MSG_TERMINATE);
    // We never get here, because EnforceMessageType() always throws either
    // Shutdown or ProtocolViolationException.
    assert (false);
}


void SlaveAgent::HandleDescribe(std::vector<zmq::message_t>& msg)
{
    coralproto::execution::SlaveDescription sd;
    *sd.mutable_type_description() =
        coral::protocol::ToProto(m_slaveInstance.TypeDescription());
    coral::protocol::execution::CreateMessage(
        msg, coralproto::execution::MSG_READY, sd);
}


namespace
{
    class SetVariable : public boost::static_visitor<bool>
    {
    public:
        SetVariable(
            coral::slave::Instance& slaveInstance,
            coral::model::VariableID varRef)
            : m_slaveInstance(slaveInstance), m_varRef(varRef) { }
        bool operator()(double value) const
        {
            return m_slaveInstance.SetRealVariable(m_varRef, value);
        }
        bool operator()(int value) const
        {
            return m_slaveInstance.SetIntegerVariable(m_varRef, value);
        }
        bool operator()(bool value) const
        {
            return m_slaveInstance.SetBooleanVariable(m_varRef, value);
        }
        bool operator()(const std::string& value) const
        {
            return m_slaveInstance.SetStringVariable(m_varRef, value);
        }
    private:
        coral::slave::Instance& m_slaveInstance;
        coral::model::VariableID m_varRef;
    };
}


// TODO: Make this function signature more consistent with Step() (or the other
// way around).
void SlaveAgent::HandleSetVars(std::vector<zmq::message_t>& msg)
{
    if (msg.size() != 2) {
        throw coral::error::ProtocolViolationException(
            "Wrong number of frames in SET_VARS message");
    }
    CORAL_LOG_DEBUG("Setting/connecting variables");
    coralproto::execution::SetVarsData data;
    coral::protobuf::ParseFromFrame(msg[1], data);

    bool allGood = true;
    for (const auto& varSetting : data.variable()) {
        // TODO: Catch and report errors
        if (varSetting.has_value()) {
            const auto val = coral::protocol::FromProto(varSetting.value());
            if (!boost::apply_visitor(
                    SetVariable(m_slaveInstance, varSetting.variable_id()),
                    val)) {
                allGood = false;
                CORAL_LOG_DEBUG(
                    boost::format("Failed to set value of variable with ID %d")
                    % varSetting.variable_id());
            }
        }
        if (varSetting.has_connected_output()) {
            m_connections.Couple(
                coral::protocol::FromProto(varSetting.connected_output()),
                varSetting.variable_id());
        }
    }
    CORAL_LOG_TRACE("Done setting/connecting variables");
    if (allGood) {
        coral::protocol::execution::CreateMessage(
            msg,
            coralproto::execution::MSG_READY);
    } else {
        coral::protocol::execution::CreateErrorMessage(
            msg,
            coralproto::execution::ErrorInfo::CANNOT_SET_VARIABLE,
            "Failed to set the value of one or more variables");
    }
}


void SlaveAgent::HandleSetPeers(std::vector<zmq::message_t>& msg)
{
    if (msg.size() != 2) {
        throw coral::error::ProtocolViolationException(
            "Wrong number of frames in SET_PEERS message");
    }
    CORAL_LOG_DEBUG("Reconnecting to peers");
    coralproto::execution::SetPeersData data;
    coral::protobuf::ParseFromFrame(msg[1], data);
    std::vector<coral::net::Endpoint> m_endpoints;
    for (const auto& peer : data.peer()) {
        m_endpoints.emplace_back(peer);
    }
    m_connections.Connect(m_endpoints.data(), m_endpoints.size());
    CORAL_LOG_TRACE("Done reconnecting to peers");
    coral::protocol::execution::CreateMessage(msg, coralproto::execution::MSG_READY);
}


void SlaveAgent::HandleResendVars(std::vector<zmq::message_t>& msg)
{
    // Publish all own variable values
    PublishAll();

    // Wait for all values from others
    CORAL_LOG_TRACE(
        boost::format("Waiting for variable values (timeout = %d ms)")
        % m_variableRecvTimeout.count());
    if (m_connections.Update(m_slaveInstance, m_currentStepID, m_variableRecvTimeout)) {
        coral::protocol::execution::CreateMessage(msg, coralproto::execution::MSG_READY);
    } else {
        CORAL_LOG_TRACE("RESEND_VARS timed out");
        coral::protocol::execution::CreateErrorMessage(
            msg,
            coralproto::execution::ErrorInfo::TIMED_OUT,
            "RESEND_VARS timed out");
    }
    assert(m_stateHandler == &SlaveAgent::ReadyHandler);
}


namespace
{
    coral::model::ScalarValue GetVariable(
        const coral::slave::Instance& slave,
        const coral::model::VariableDescription& variable)
    {
        switch (variable.DataType()) {
            case coral::model::REAL_DATATYPE:
                return slave.GetRealVariable(variable.ID());
            case coral::model::INTEGER_DATATYPE:
                return slave.GetIntegerVariable(variable.ID());
            case coral::model::BOOLEAN_DATATYPE:
                return slave.GetBooleanVariable(variable.ID());
            case coral::model::STRING_DATATYPE:
                return slave.GetStringVariable(variable.ID());
            default:
                assert (!"Variable has unknown data type");
                return coral::model::ScalarValue();
        }
    }
}


bool SlaveAgent::Step(const coralproto::execution::StepData& stepInfo)
{
    if (m_currentStepID == coral::model::INVALID_STEP_ID) {
        m_slaveInstance.StartSimulation();
    }
    m_currentStepID = stepInfo.step_id();
    if (!m_slaveInstance.DoStep(stepInfo.timepoint(), stepInfo.stepsize())) {
        return false;
    }
    PublishAll();
    return true;
}


void SlaveAgent::PublishAll()
{
    CORAL_LOG_TRACE("Publishing output variable values");
    const auto typeDescription = m_slaveInstance.TypeDescription();
    for (const auto& varInfo : typeDescription.Variables()) {
        if (varInfo.Causality() != coral::model::OUTPUT_CAUSALITY) continue;
        m_publisher.Publish(
            m_currentStepID,
            m_id,
            varInfo.ID(),
            GetVariable(m_slaveInstance, varInfo));
    }
}


// =============================================================================
// class SlaveAgent::Timeout
// =============================================================================
// TODO: This seems generally useful, so should probably be moved elsewhere.

SlaveAgent::Timeout::Timeout(
    coral::net::Reactor& reactor,
    std::chrono::milliseconds timeout)
    : m_reactor{reactor}
    , m_timerID{coral::net::Reactor::invalidTimerID}
{
    SetTimeout(timeout);
}


SlaveAgent::Timeout::~Timeout() noexcept
{
    SetTimeout(std::chrono::milliseconds(-1));
}


void SlaveAgent::Timeout::Reset()
{
    if (m_timerID != coral::net::Reactor::invalidTimerID) {
        m_reactor.RestartTimerInterval(m_timerID);
    }
}


void SlaveAgent::Timeout::SetTimeout(std::chrono::milliseconds timeout)
{
    if (m_timerID != coral::net::Reactor::invalidTimerID) {
        m_reactor.RemoveTimer(m_timerID);
        m_timerID = coral::net::Reactor::invalidTimerID;
    }
    if (timeout >= std::chrono::milliseconds(0)) {
        m_timerID = m_reactor.AddTimer(
            timeout,
            1,
            [timeout, this] (coral::net::Reactor&, int)
            {
                m_timerID = coral::net::Reactor::invalidTimerID;
                throw coral::slave::TimeoutException(
                    "Timed out due to lack of communication with master",
                    timeout);
            });
    }
}


// =============================================================================
// class SlaveAgent::Connections
// =============================================================================

void SlaveAgent::Connections::Connect(
    const coral::net::Endpoint* endpoints,
    std::size_t endpointsSize)
{
    m_subscriber.Connect(endpoints, endpointsSize);
}


void SlaveAgent::Connections::Couple(
    coral::model::Variable remoteOutput,
    coral::model::VariableID localInput)
{
    Decouple(localInput);
    if (!remoteOutput.Empty()) {
        m_subscriber.Subscribe(remoteOutput);
        m_connections.insert(ConnectionBimap::value_type(remoteOutput, localInput));
    }
}


bool SlaveAgent::Connections::Update(
    coral::slave::Instance& slaveInstance,
    coral::model::StepID stepID,
    std::chrono::milliseconds timeout)
{
    if (!m_subscriber.Update(stepID, timeout)) return false;
    for (const auto& conn : m_connections.left) {
        boost::apply_visitor(
            SetVariable(slaveInstance, conn.second),
            m_subscriber.Value(conn.first));
    }
    return true;
}


void SlaveAgent::Connections::Decouple(coral::model::VariableID localInput)
{
    const auto conn = m_connections.right.find(localInput);
    if (conn == m_connections.right.end()) return;
    const auto remoteOutput = conn->second;
    m_connections.right.erase(conn);
    if (m_connections.left.count(remoteOutput) == 0) {
        m_subscriber.Unsubscribe(remoteOutput);
    }
    assert(m_connections.right.count(localInput) == 0);
}


}} // namespace
