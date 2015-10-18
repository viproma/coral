#include "dsb/bus/slave_control_messenger_v0.hpp"

#include <cassert>
#include <utility>

#include "dsb/compat_helpers.hpp"
#include "dsb/error.hpp"
#include "dsb/protocol/execution.hpp"
#include "dsb/protocol/glue.hpp"
#include "dsb/util.hpp"
#include "execution.pb.h"


namespace dsb
{
namespace bus
{

namespace
{
    // We use these as default values member variables.
    const int NO_COMMAND_ACTIVE = -1;
    const int NO_TIMER_ACTIVE = -1;
}


SlaveControlMessengerV0::SlaveControlMessengerV0(
    dsb::comm::Reactor& reactor,
    dsb::comm::P2PReqSocket socket,
    dsb::model::SlaveID slaveID,
    const SlaveSetup& setup,
    boost::chrono::milliseconds timeout,
    MakeSlaveControlMessengerHandler onComplete)
    : m_reactor(reactor),
      m_socket(std::move(socket)),
      m_state(SLAVE_CONNECTED),
      m_attachedToReactor(false),
      m_currentCommand(NO_COMMAND_ACTIVE),
      m_onComplete(),
      m_replyTimeoutTimerId(NO_TIMER_ACTIVE)
{
    reactor.AddSocket(m_socket.Socket(), [=](dsb::comm::Reactor& r, zmq::socket_t& s) {
        assert (&s == &m_socket.Socket());
        OnReply();
    });
    m_attachedToReactor = true;
    Setup(slaveID, setup, timeout, std::move(onComplete));

    assert(State() == SLAVE_BUSY);
    CheckInvariant();
}


SlaveControlMessengerV0::~SlaveControlMessengerV0() DSB_NOEXCEPT
{
    CheckInvariant();
    if (m_attachedToReactor) {
        m_reactor.RemoveSocket(m_socket.Socket());
    }
    if (m_replyTimeoutTimerId != NO_TIMER_ACTIVE) {
        UnregisterTimeout();
    }
}


SlaveState SlaveControlMessengerV0::State() const DSB_NOEXCEPT
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
        onComplete(make_error_code(dsb::error::generic_error::aborted));
    } else if (m_state != SLAVE_NOT_CONNECTED) {
        Reset();
    }
}


void SlaveControlMessengerV0::SetVariables(
    const std::vector<dsb::model::VariableSetting>& settings,
    boost::chrono::milliseconds timeout,
    SetVariablesHandler onComplete)
{
    DSB_PRECONDITION_CHECK(State() == SLAVE_READY);
    DSB_INPUT_CHECK(timeout > boost::chrono::milliseconds(0));
    DSB_INPUT_CHECK(onComplete);
    CheckInvariant();

    dsbproto::execution::SetVarsData data;
    for (auto it = begin(settings); it != end(settings); ++it) {
        auto v = data.add_variable();
        v->set_variable_id(it->Variable());
        if (it->HasValue()) {
            dsb::protocol::ConvertToProto(it->Value(), *v->mutable_value());
        }
        if (it->IsConnected()) {
            dsb::protocol::ConvertToProto(it->ConnectedOutput(), *v->mutable_connected_output());
        }
    }
    SendCommand(dsbproto::execution::MSG_SET_VARS, &data, timeout, std::move(onComplete));
    assert(State() == SLAVE_BUSY);
}


void SlaveControlMessengerV0::Step(
    dsb::model::TimePoint currentT,
    dsb::model::TimeDuration deltaT,
    boost::chrono::milliseconds timeout,
    StepHandler onComplete)
{
    DSB_PRECONDITION_CHECK(State() == SLAVE_READY);
    DSB_INPUT_CHECK(timeout > boost::chrono::milliseconds(0));
    DSB_INPUT_CHECK(onComplete);
    CheckInvariant();

    dsbproto::execution::StepData data;
    data.set_timepoint(currentT);
    data.set_stepsize(deltaT);

    SendCommand(dsbproto::execution::MSG_STEP, &data, timeout, std::move(onComplete));
    assert(State() == SLAVE_BUSY);
}


void SlaveControlMessengerV0::AcceptStep(
    boost::chrono::milliseconds timeout,
    AcceptStepHandler onComplete)
{
    DSB_PRECONDITION_CHECK(m_state == SLAVE_STEP_OK);
    DSB_INPUT_CHECK(timeout > boost::chrono::milliseconds(0));
    DSB_INPUT_CHECK(onComplete);
    CheckInvariant();

    SendCommand(dsbproto::execution::MSG_ACCEPT_STEP, nullptr, timeout, std::move(onComplete));
    assert(State() == SLAVE_BUSY);
}


void SlaveControlMessengerV0::Terminate()
{
    DSB_PRECONDITION_CHECK(m_state != SLAVE_NOT_CONNECTED);
    CheckInvariant();

    const auto onComplete = std::move(m_onComplete);
    std::deque<zmq::message_t> msg;
    dsb::protocol::execution::CreateMessage(msg, dsbproto::execution::MSG_TERMINATE);
    m_socket.Send(msg, dsb::comm::SEND_OUT_OF_ORDER);
    // ---
    // Workaround for ZeroMQ issue 1264, https://github.com/zeromq/libzmq/issues/1264
    char temp;
    m_socket.Socket().recv(&temp, 1, ZMQ_DONTWAIT);
    // ---
    Close();
    if (onComplete) {
        // This is only if m_state was SLAVE_BUSY when we entered the function.
        onComplete(make_error_code(std::errc::operation_canceled));
    }
}


void SlaveControlMessengerV0::Setup(
    dsb::model::SlaveID slaveID,
    const SlaveSetup& setup,
    boost::chrono::milliseconds timeout,
    VoidHandler onComplete)
{
    assert(State() == SLAVE_CONNECTED);
    CheckInvariant();

    dsbproto::execution::SetupData data;
    data.set_slave_id(slaveID);
    data.set_start_time(setup.startTime);
    if (setup.stopTime != dsb::model::ETERNITY) {
        data.set_stop_time(setup.stopTime);
    }
    data.set_variable_pub_endpoint(setup.variablePubEndpoint);
    data.set_variable_sub_endpoint(setup.variableSubEndpoint);

    SendCommand(dsbproto::execution::MSG_SETUP, &data, timeout, std::move(onComplete));
    assert(State() == SLAVE_BUSY);
}


void SlaveControlMessengerV0::Reset()
{
    assert(m_attachedToReactor);
    assert(m_currentCommand == NO_COMMAND_ACTIVE);
    assert(!m_onComplete);
    assert(m_replyTimeoutTimerId == NO_TIMER_ACTIVE);
    m_reactor.RemoveSocket(m_socket.Socket());
    m_socket.Close();
    m_state = SLAVE_NOT_CONNECTED;
    m_attachedToReactor = false;
}


void SlaveControlMessengerV0::SendCommand(
    int command,
    const google::protobuf::MessageLite* data,
    boost::chrono::milliseconds timeout,
    VoidHandler onComplete)
{
    std::deque<zmq::message_t> msg;
    const auto msgType = static_cast<dsbproto::execution::MessageType>(command);
    if (data) dsb::protocol::execution::CreateMessage(msg, msgType, *data);
    else      dsb::protocol::execution::CreateMessage(msg, msgType);
    m_socket.Send(msg);
    PostSendCommand(command, timeout, std::move(onComplete));
}


void SlaveControlMessengerV0::PostSendCommand(
    int command,
    boost::chrono::milliseconds timeout,
    VoidHandler onComplete)
{
    RegisterTimeout(timeout);
    m_state = SLAVE_BUSY;
    m_currentCommand = command;
    m_onComplete = std::move(onComplete);
}


void SlaveControlMessengerV0::RegisterTimeout(boost::chrono::milliseconds timeout)
{
    assert (m_replyTimeoutTimerId == NO_TIMER_ACTIVE);
    m_replyTimeoutTimerId = m_reactor.AddTimer(timeout, 1,
        [=](dsb::comm::Reactor& r, int i) {
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
                 "Unless you are a DSB developer, you should not see this. "
                 "If you do, please let us know.");
        return;
    }

    // Clean up before any callbacks are called, in case they throw or initiate
    // a new command.  We don't touch m_state, though; that must be done inside
    // the reply handlers, based on the actual reply.
    const auto currentCommand = dsb::util::MoveAndReplace(m_currentCommand, NO_COMMAND_ACTIVE);
    const auto onComplete = std::move(m_onComplete);
    UnregisterTimeout();

    // Delegate different replies to different functions.
    std::deque<zmq::message_t> msg;
    m_socket.Receive(msg);
    switch (currentCommand) {
        case dsbproto::execution::MSG_SETUP:        SetupReplyReceived(msg, std::move(onComplete)); break;
        case dsbproto::execution::MSG_SET_VARS:     SetVarsReplyReceived(msg, std::move(onComplete)); break;
        case dsbproto::execution::MSG_STEP:         StepReplyReceived(msg, std::move(onComplete)); break;
        case dsbproto::execution::MSG_ACCEPT_STEP:  AcceptStepReplyReceived(msg, std::move(onComplete)); break;
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

    onComplete(make_error_code(std::errc::timed_out));
}


void SlaveControlMessengerV0::SetupReplyReceived(
    const std::deque<zmq::message_t>& msg,
    VoidHandler onComplete)
{
    assert (m_state == SLAVE_BUSY);
    HandleExpectedReadyReply(msg, std::move(onComplete));
}


void SlaveControlMessengerV0::SetVarsReplyReceived(
    const std::deque<zmq::message_t>& msg,
    VoidHandler onComplete)
{
    assert (m_state == SLAVE_BUSY);
    HandleExpectedReadyReply(msg, std::move(onComplete));
}


void SlaveControlMessengerV0::StepReplyReceived(
    const std::deque<zmq::message_t>& msg,
    VoidHandler onComplete)
{
    assert (m_state = SLAVE_BUSY);
    const auto msgType = dsb::protocol::execution::ParseMessageType(msg.front());
    if (msgType == dsbproto::execution::MSG_STEP_OK) {
        m_state = SLAVE_STEP_OK;
        onComplete(std::error_code());
    } else if (msgType == dsbproto::execution::MSG_STEP_FAILED) {
        Reset();
        onComplete(dsb::error::sim_error::cannot_perform_timestep);
    } else {
        HandleErrorReply(msgType, std::move(onComplete));
    }
}


void SlaveControlMessengerV0::AcceptStepReplyReceived(
    const std::deque<zmq::message_t>& msg,
    VoidHandler onComplete)
{
    assert(m_state == SLAVE_BUSY);
    HandleExpectedReadyReply(msg, std::move(onComplete));
}


void SlaveControlMessengerV0::HandleExpectedReadyReply(
    const std::deque<zmq::message_t>& msg,
    VoidHandler onComplete)
{
    assert (m_state == SLAVE_BUSY);
    const auto reply = dsb::protocol::execution::ParseMessageType(msg.front());
    if (reply == dsbproto::execution::MSG_READY) {
        m_state = SLAVE_READY;
        onComplete(std::error_code());
    } else {
        HandleErrorReply(reply, std::move(onComplete));
    }
}


void SlaveControlMessengerV0::HandleErrorReply(int reply, VoidHandler onComplete)
{
    Reset();
    if (reply == dsbproto::execution::MSG_ERROR) {
        onComplete(make_error_code(dsb::error::generic_error::operation_failed));
    } else {
        onComplete(make_error_code(std::errc::bad_message));
    }
}


// This function does absolutely nothing when compiled in release mode, and
// it is expected that the compiler will simply optimise it away entirely.
void SlaveControlMessengerV0::CheckInvariant() const
{
    switch (m_state) {
        case SLAVE_NOT_CONNECTED:
            assert(!m_attachedToReactor);
            assert(m_currentCommand == NO_COMMAND_ACTIVE);
            assert(!m_onComplete);
            assert(m_replyTimeoutTimerId == NO_TIMER_ACTIVE);
            break;
        case SLAVE_CONNECTED:
        case SLAVE_READY:
        case SLAVE_STEP_OK:
            assert(m_attachedToReactor);
            assert(m_currentCommand == NO_COMMAND_ACTIVE);
            assert(!m_onComplete);
            assert(m_replyTimeoutTimerId == NO_TIMER_ACTIVE);
            break;
        case SLAVE_BUSY:
            assert(m_attachedToReactor);
            assert(m_currentCommand != NO_COMMAND_ACTIVE);
            assert(m_onComplete);
            assert(m_replyTimeoutTimerId != NO_TIMER_ACTIVE);
            break;
        default:
            assert(!"Invalid value of m_state");
    }
}


}} // namespace
