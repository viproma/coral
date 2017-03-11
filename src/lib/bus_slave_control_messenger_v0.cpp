/*
Copyright 2013-2017, SINTEF Ocean and the Coral contributors.
This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#include "coral/bus/slave_control_messenger_v0.hpp"

#include <cassert>
#include <utility>

#include <boost/numeric/conversion/cast.hpp>

#include "coral/error.hpp"
#include "coral/log.hpp"
#include "coral/protobuf.hpp"
#include "coral/protocol/execution.hpp"
#include "coral/protocol/glue.hpp"
#include "coral/util.hpp"
#include "execution.pb.h"


namespace coral
{
namespace bus
{

namespace
{
    // We use these as default values member variables.
    const int NO_COMMAND_ACTIVE = -1;
    const int NO_TIMER_ACTIVE = -1;

    // boost::variant visitor class for calling a completion handler with an
    // error code, regardless of operation/handler type.
    class CallWithError : public boost::static_visitor<>
    {
    public:
        CallWithError(const std::error_code& ec) : m_ec(ec) { }

        void operator()(const ISlaveControlMessenger::VoidHandler& c) const
        {
            c(m_ec);
        }

        void operator()(const ISlaveControlMessenger::GetDescriptionHandler& c) const
        {
            c(m_ec, coral::model::SlaveDescription());
        }

    private:
        std::error_code m_ec;
    };

    // boost::variant visitor class for checking whether a completion handler
    // object is empty (has no handler assigned to it).  This is only used in
    // assertions, so for now we only need to include it in debug mode.
#ifndef NDEBUG
    class IsEmpty : public boost::static_visitor<bool>
    {
    public:
        template<typename T>
        bool operator()(const T& x) const { return !x; }
    };
#endif
}


SlaveControlMessengerV0::SlaveControlMessengerV0(
    coral::net::Reactor& reactor,
    coral::net::zmqx::ReqSocket socket,
    coral::model::SlaveID slaveID,
    const std::string& slaveName,
    const SlaveSetup& setup,
    std::chrono::milliseconds timeout,
    MakeSlaveControlMessengerHandler onComplete)
    : m_reactor(reactor),
      m_socket(std::move(socket)),
      m_state(SLAVE_CONNECTED),
      m_attachedToReactor(false),
      m_currentCommand(NO_COMMAND_ACTIVE),
      m_onComplete(),
      m_replyTimeoutTimerId(NO_TIMER_ACTIVE)
{
    CORAL_LOG_TRACE(boost::format("SlaveControlMessengerV0 %x: connected to \"%s\" (ID = %d)")
        % this % slaveName % slaveID);
    reactor.AddSocket(m_socket.Socket(), [=](coral::net::Reactor& r, zmq::socket_t& s) {
        assert (&s == &m_socket.Socket());
        OnReply();
    });
    m_attachedToReactor = true;
    Setup(slaveID, slaveName, setup, timeout, std::move(onComplete));

    assert(State() == SLAVE_BUSY);
    CheckInvariant();
}


SlaveControlMessengerV0::~SlaveControlMessengerV0() CORAL_NOEXCEPT
{
    CheckInvariant();
    if (m_attachedToReactor) {
        m_reactor.RemoveSocket(m_socket.Socket());
    }
    if (m_replyTimeoutTimerId != NO_TIMER_ACTIVE) {
        UnregisterTimeout();
    }
}


SlaveState SlaveControlMessengerV0::State() const CORAL_NOEXCEPT
{
    return m_state;
}


void SlaveControlMessengerV0::Close()
{
    CheckInvariant();
    if (m_state == SLAVE_BUSY) {
        UnregisterTimeout();
        auto onComplete = std::move(m_onComplete);
        m_currentCommand = NO_COMMAND_ACTIVE;
        Reset();
        boost::apply_visitor(
            CallWithError(make_error_code(std::errc::operation_canceled)),
            onComplete);
    } else if (m_state != SLAVE_NOT_CONNECTED) {
        Reset();
    }
}


void SlaveControlMessengerV0::GetDescription(
    std::chrono::milliseconds timeout,
    GetDescriptionHandler onComplete)
{
    CORAL_PRECONDITION_CHECK(State() == SLAVE_READY);
    CORAL_INPUT_CHECK(timeout > std::chrono::milliseconds(0));
    CORAL_INPUT_CHECK(onComplete);
    CheckInvariant();

    SendCommand(
        coralproto::execution::MSG_DESCRIBE,
        nullptr,
        timeout,
        std::move(onComplete));
    assert(State() == SLAVE_BUSY);
}


void SlaveControlMessengerV0::SetVariables(
    const std::vector<coral::model::VariableSetting>& settings,
    std::chrono::milliseconds timeout,
    SetVariablesHandler onComplete)
{
    CORAL_PRECONDITION_CHECK(State() == SLAVE_READY);
    CORAL_INPUT_CHECK(timeout > std::chrono::milliseconds(0));
    CORAL_INPUT_CHECK(onComplete);
    CheckInvariant();

    coralproto::execution::SetVarsData data;
    for (auto it = begin(settings); it != end(settings); ++it) {
        auto v = data.add_variable();
        v->set_variable_id(it->Variable());
        if (it->HasValue()) {
            coral::protocol::ConvertToProto(it->Value(), *v->mutable_value());
        }
        if (it->IsConnected()) {
            coral::protocol::ConvertToProto(it->ConnectedOutput(), *v->mutable_connected_output());
        }
    }
    SendCommand(coralproto::execution::MSG_SET_VARS, &data, timeout, std::move(onComplete));
    assert(State() == SLAVE_BUSY);
}


void SlaveControlMessengerV0::SetPeers(
    const std::vector<coral::net::Endpoint>& peers,
    std::chrono::milliseconds timeout,
    SetPeersHandler onComplete)
{
    CORAL_PRECONDITION_CHECK(State() == SLAVE_READY);
    CORAL_INPUT_CHECK(timeout > std::chrono::milliseconds(0));
    CORAL_INPUT_CHECK(onComplete);
    CheckInvariant();

    coralproto::execution::SetPeersData data;
    for (const auto peer: peers) data.add_peer(peer.URL());
    SendCommand(coralproto::execution::MSG_SET_PEERS, &data, timeout, std::move(onComplete));
    assert(State() == SLAVE_BUSY);
}


void SlaveControlMessengerV0::ResendVars(
    std::chrono::milliseconds timeout,
    ResendVarsHandler onComplete)
{
    CORAL_PRECONDITION_CHECK(State() == SLAVE_READY);
    CORAL_INPUT_CHECK(timeout > std::chrono::milliseconds(0));
    CORAL_INPUT_CHECK(onComplete);
    CheckInvariant();

    SendCommand(coralproto::execution::MSG_RESEND_VARS, nullptr, timeout, std::move(onComplete));
    assert(State() == SLAVE_BUSY);
}


void SlaveControlMessengerV0::Step(
    coral::model::StepID stepID,
    coral::model::TimePoint currentT,
    coral::model::TimeDuration deltaT,
    std::chrono::milliseconds timeout,
    StepHandler onComplete)
{
    CORAL_PRECONDITION_CHECK(State() == SLAVE_READY);
    CORAL_INPUT_CHECK(timeout > std::chrono::milliseconds(0));
    CORAL_INPUT_CHECK(onComplete);
    CheckInvariant();

    coralproto::execution::StepData data;
    data.set_step_id(stepID);
    data.set_timepoint(currentT);
    data.set_stepsize(deltaT);

    SendCommand(coralproto::execution::MSG_STEP, &data, timeout, std::move(onComplete));
    assert(State() == SLAVE_BUSY);
}


void SlaveControlMessengerV0::AcceptStep(
    std::chrono::milliseconds timeout,
    AcceptStepHandler onComplete)
{
    CORAL_PRECONDITION_CHECK(m_state == SLAVE_STEP_OK);
    CORAL_INPUT_CHECK(timeout > std::chrono::milliseconds(0));
    CORAL_INPUT_CHECK(onComplete);
    CheckInvariant();

    SendCommand(coralproto::execution::MSG_ACCEPT_STEP, nullptr, timeout, std::move(onComplete));
    assert(State() == SLAVE_BUSY);
}


void SlaveControlMessengerV0::Terminate()
{
    CORAL_PRECONDITION_CHECK(m_state != SLAVE_NOT_CONNECTED);
    CheckInvariant();

    CORAL_LOG_TRACE(
        boost::format("SlaveControlMessengerV0 %x: Sending MSG_TERMINATE")
        % this);
    std::vector<zmq::message_t> msg;
    coral::protocol::execution::CreateMessage(msg, coralproto::execution::MSG_TERMINATE);
    m_socket.Send(msg);
    CORAL_LOG_TRACE(boost::format("SlaveControlMessengerV0 %x: Send complete") % this);
    Close();
}


void SlaveControlMessengerV0::Setup(
    coral::model::SlaveID slaveID,
    const std::string& slaveName,
    const SlaveSetup& setup,
    std::chrono::milliseconds timeout,
    VoidHandler onComplete)
{
    assert(State() == SLAVE_CONNECTED);
    CheckInvariant();

    coralproto::execution::SetupData data;
    data.set_slave_id(slaveID);
    data.set_start_time(setup.startTime);
    if (setup.stopTime != coral::model::ETERNITY) {
        data.set_stop_time(setup.stopTime);
    }
    data.set_execution_name(setup.executionName);
    data.set_slave_name(slaveName);
    data.set_variable_recv_timeout_ms(
        setup.variableRecvTimeout >= std::chrono::milliseconds(0)
            ? boost::numeric_cast<google::protobuf::int32>(setup.variableRecvTimeout.count())
            : -1);
    SendCommand(coralproto::execution::MSG_SETUP, &data, timeout, std::move(onComplete));
    assert(State() == SLAVE_BUSY);
}


void SlaveControlMessengerV0::Reset()
{
    assert(m_attachedToReactor);
    assert(m_currentCommand == NO_COMMAND_ACTIVE);
    assert(boost::apply_visitor(IsEmpty(), m_onComplete));
    assert(m_replyTimeoutTimerId == NO_TIMER_ACTIVE);
    m_reactor.RemoveSocket(m_socket.Socket());
    m_socket.Close();
    m_state = SLAVE_NOT_CONNECTED;
    m_attachedToReactor = false;
}


void SlaveControlMessengerV0::SendCommand(
    int command,
    const google::protobuf::MessageLite* data,
    std::chrono::milliseconds timeout,
    AnyHandler onComplete)
{
    std::vector<zmq::message_t> msg;
    const auto msgType = static_cast<coralproto::execution::MessageType>(command);
    CORAL_LOG_TRACE(boost::format("SlaveControlMessengerV0 %x: Sending %s")
        % this % coralproto::execution::MessageType_Name(msgType));
    if (data) coral::protocol::execution::CreateMessage(msg, msgType, *data);
    else      coral::protocol::execution::CreateMessage(msg, msgType);
    m_socket.Send(msg);
    CORAL_LOG_TRACE(boost::format("SlaveControlMessengerV0 %x: Send complete") % this);
    PostSendCommand(command, timeout, std::move(onComplete));
}


void SlaveControlMessengerV0::PostSendCommand(
    int command,
    std::chrono::milliseconds timeout,
    AnyHandler onComplete)
{
    RegisterTimeout(timeout);
    m_state = SLAVE_BUSY;
    m_currentCommand = command;
    m_onComplete = std::move(onComplete);
}


void SlaveControlMessengerV0::RegisterTimeout(std::chrono::milliseconds timeout)
{
    assert (m_replyTimeoutTimerId == NO_TIMER_ACTIVE);
    m_replyTimeoutTimerId = m_reactor.AddTimer(timeout, 1,
        [=](coral::net::Reactor& r, int i) {
            assert (i == m_replyTimeoutTimerId);
            OnReplyTimeout();
        });
    assert (m_replyTimeoutTimerId != NO_TIMER_ACTIVE);
}


void SlaveControlMessengerV0::UnregisterTimeout()
{
    assert (m_replyTimeoutTimerId != NO_TIMER_ACTIVE);
    m_reactor.RemoveTimer(m_replyTimeoutTimerId);
    m_replyTimeoutTimerId = NO_TIMER_ACTIVE;
}


void SlaveControlMessengerV0::OnReply()
{
    CheckInvariant();
    if (State() != SLAVE_BUSY) {
        // TODO:
        // The assert below should be removed later, or possibly replaced with
        // a logging statement.  We could end up in this situation if the reply
        // reaches us after the request timed out, in which case we should
        // simply ignore it (and let the slave timeout too, on its own).
        // The assert is simply here now, in the early stages of development,
        // to help us catch bugs in the master-slave communication.
        assert (!"Diagnostic: Unexpected message from slave. "
                 "Unless you are a Coral developer, you should not see this. "
                 "If you do, please let us know.");
        return;
    }

    // Clean up before any callbacks are called, in case they throw or initiate
    // a new command.  We don't touch m_state, though; that must be done inside
    // the reply handlers, based on the actual reply.
    const auto currentCommand = coral::util::MoveAndReplace(m_currentCommand, NO_COMMAND_ACTIVE);
    const auto onComplete = std::move(m_onComplete);
    UnregisterTimeout();

    // Delegate different replies to different functions.
    std::vector<zmq::message_t> msg;
    m_socket.Receive(msg);
    CORAL_LOG_TRACE(boost::format("SlaveControlMessengerV0 %x: Received %s")
        % this
        % coralproto::execution::MessageType_Name(
            static_cast<coralproto::execution::MessageType>(
                coral::protocol::execution::ParseMessageType(msg.front()))));
    switch (currentCommand) {
        case coralproto::execution::MSG_SETUP:
            SetupReplyReceived(
                msg,
                std::move(boost::get<VoidHandler>(onComplete)));
            break;
        case coralproto::execution::MSG_DESCRIBE:
            DescribeReplyReceived(
                msg,
                std::move(boost::get<GetDescriptionHandler>(onComplete)));
            break;
        case coralproto::execution::MSG_SET_VARS:
            SetVarsReplyReceived(
                msg,
                std::move(boost::get<VoidHandler>(onComplete)));
            break;
        case coralproto::execution::MSG_SET_PEERS:
            SetPeersReplyReceived(
                msg,
                std::move(boost::get<VoidHandler>(onComplete)));
            break;
        case coralproto::execution::MSG_RESEND_VARS:
            ResendVarsReplyReceived(
                msg,
                std::move(boost::get<VoidHandler>(onComplete)));
            break;
        case coralproto::execution::MSG_STEP:
            StepReplyReceived(
                msg,
                std::move(boost::get<VoidHandler>(onComplete)));
            break;
        case coralproto::execution::MSG_ACCEPT_STEP:
            AcceptStepReplyReceived(
                msg,
                std::move(boost::get<VoidHandler>(onComplete)));
            break;
        default: assert(!"Invalid currentCommand value");
    }
}


void SlaveControlMessengerV0::OnReplyTimeout()
{
    assert(m_state == SLAVE_BUSY);
    CheckInvariant();
    // Do all cleanup before calling the callback, in case it throws.
    m_currentCommand = NO_COMMAND_ACTIVE;
    const auto onComplete = std::move(m_onComplete);
    m_replyTimeoutTimerId = NO_TIMER_ACTIVE;
    Reset();

    boost::apply_visitor(
        CallWithError(make_error_code(std::errc::timed_out)),
        onComplete);
}


void SlaveControlMessengerV0::SetupReplyReceived(
    const std::vector<zmq::message_t>& msg,
    VoidHandler onComplete)
{
    assert (m_state == SLAVE_BUSY);
    HandleExpectedReadyReply(msg, std::move(onComplete));
}


void SlaveControlMessengerV0::DescribeReplyReceived(
    const std::vector<zmq::message_t>& msg,
    GetDescriptionHandler onComplete)
{
    assert (m_state == SLAVE_BUSY);
    const auto reply = coral::protocol::execution::ParseMessageType(msg.front());
    if (reply == coralproto::execution::MSG_READY && msg.size() > 1) {
        coralproto::execution::SlaveDescription slaveDescription;
        coral::protobuf::ParseFromFrame(msg[1], slaveDescription);
        m_state = SLAVE_READY;
        onComplete(
            std::error_code(),
            coral::model::SlaveDescription(
                coral::model::INVALID_SLAVE_ID,
                std::string(),
                coral::protocol::FromProto(slaveDescription.type_description())));
    } else {
        HandleErrorReply(reply, std::move(onComplete));
    }
}


void SlaveControlMessengerV0::SetVarsReplyReceived(
    const std::vector<zmq::message_t>& msg,
    VoidHandler onComplete)
{
    assert (m_state == SLAVE_BUSY);
    HandleExpectedReadyReply(msg, std::move(onComplete));
}


void SlaveControlMessengerV0::SetPeersReplyReceived(
    const std::vector<zmq::message_t>& msg,
    VoidHandler onComplete)
{
    assert (m_state == SLAVE_BUSY);
    HandleExpectedReadyReply(msg, std::move(onComplete));
}


void SlaveControlMessengerV0::ResendVarsReplyReceived(
    const std::vector<zmq::message_t>& msg,
    VoidHandler onComplete)
{
    assert (m_state == SLAVE_BUSY);
    const auto reply = coral::protocol::execution::ParseMessageType(msg.front());
    if (reply == coralproto::execution::MSG_READY) {
        m_state = SLAVE_READY;
        onComplete(std::error_code{});
    } else if (reply == coralproto::execution::MSG_ERROR && msg.size() > 1) {
        coralproto::execution::ErrorInfo errorInfo;
        coral::protobuf::ParseFromFrame(msg[1], errorInfo);
        if (errorInfo.code() == coralproto::execution::ErrorInfo::TIMED_OUT) {
            m_state = SLAVE_READY;
            onComplete(make_error_code(coral::error::sim_error::data_timeout));
        } else {
            HandleErrorReply(reply, std::move(onComplete));
        }
    } else {
        HandleErrorReply(reply, std::move(onComplete));
    }
}


void SlaveControlMessengerV0::StepReplyReceived(
    const std::vector<zmq::message_t>& msg,
    VoidHandler onComplete)
{
    assert (m_state = SLAVE_BUSY);
    const auto msgType = coral::protocol::execution::ParseMessageType(msg.front());
    if (msgType == coralproto::execution::MSG_STEP_OK) {
        m_state = SLAVE_STEP_OK;
        onComplete(std::error_code());
    } else if (msgType == coralproto::execution::MSG_STEP_FAILED) {
        m_state = SLAVE_STEP_FAILED;
        onComplete(coral::error::sim_error::cannot_perform_timestep);
    } else {
        HandleErrorReply(msgType, std::move(onComplete));
    }
}


void SlaveControlMessengerV0::AcceptStepReplyReceived(
    const std::vector<zmq::message_t>& msg,
    VoidHandler onComplete)
{
    assert(m_state == SLAVE_BUSY);
    HandleExpectedReadyReply(msg, std::move(onComplete));
}


void SlaveControlMessengerV0::HandleExpectedReadyReply(
    const std::vector<zmq::message_t>& msg,
    VoidHandler onComplete)
{
    assert (m_state == SLAVE_BUSY);
    const auto reply = coral::protocol::execution::ParseMessageType(msg.front());
    if (reply == coralproto::execution::MSG_READY) {
        m_state = SLAVE_READY;
        onComplete(std::error_code());
    } else {
        HandleErrorReply(reply, std::move(onComplete));
    }
}


void SlaveControlMessengerV0::HandleErrorReply(int reply, AnyHandler onComplete)
{
    Reset();
    std::error_code ec;
    if (reply == coralproto::execution::MSG_ERROR) {
        ec = make_error_code(coral::error::generic_error::operation_failed);
    } else if (reply == coralproto::execution::MSG_FATAL_ERROR) {
        ec = make_error_code(coral::error::generic_error::fatal);
    } else {
        ec = make_error_code(std::errc::bad_message);
    }
    assert(ec);
    boost::apply_visitor(CallWithError(ec), onComplete);
}


// This function does absolutely nothing when compiled in release mode, and
// it is expected that the compiler will simply optimise it away entirely.
void SlaveControlMessengerV0::CheckInvariant() const
{
    switch (m_state) {
        case SLAVE_NOT_CONNECTED:
            assert(!m_attachedToReactor);
            assert(m_currentCommand == NO_COMMAND_ACTIVE);
            assert(boost::apply_visitor(IsEmpty(), m_onComplete));
            assert(m_replyTimeoutTimerId == NO_TIMER_ACTIVE);
            break;
        case SLAVE_CONNECTED:
        case SLAVE_READY:
        case SLAVE_STEP_OK:
        case SLAVE_STEP_FAILED:
            assert(m_attachedToReactor);
            assert(m_currentCommand == NO_COMMAND_ACTIVE);
            assert(boost::apply_visitor(IsEmpty(), m_onComplete));
            assert(m_replyTimeoutTimerId == NO_TIMER_ACTIVE);
            break;
        case SLAVE_BUSY:
            assert(m_attachedToReactor);
            assert(m_currentCommand != NO_COMMAND_ACTIVE);
            assert(!boost::apply_visitor(IsEmpty(), m_onComplete));
            assert(m_replyTimeoutTimerId != NO_TIMER_ACTIVE);
            break;
        default:
            assert(!"Invalid value of m_state");
    }
}


}} // namespace
