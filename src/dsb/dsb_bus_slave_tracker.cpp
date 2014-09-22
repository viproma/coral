#ifdef _WIN32
#   define NOMINMAX
#endif
#include "dsb/bus/slave_tracker.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>

#include "dsb/comm.hpp"
#include "dsb/control.hpp"
#include "control.pb.h"


namespace
{
    void CreateInvalidRequest(std::deque<zmq::message_t>& targetMsg)
    {
        dsb::control::CreateErrorMessage(
            targetMsg,
            dsbproto::control::ErrorInfo::INVALID_REQUEST,
            "Slave ID not seen before, or slave was expected to be in different state");
    }

    const uint16_t MAX_PROTOCOL = 0;
}


namespace dsb
{
namespace bus
{


SlaveTracker::SlaveTracker(double startTime, double stopTime)
    : m_startTime(startTime),
      m_stopTime(stopTime),
      m_protocol(UNKNOWN_PROTOCOL),
      m_state(SLAVE_UNKNOWN),
      m_isSimulating(false)
{
}


SlaveTracker::SlaveTracker(const SlaveTracker& other)
{
    operator=(other);
}


SlaveTracker& SlaveTracker::operator=(const SlaveTracker& other) {
    m_startTime = other.m_startTime;
    m_stopTime = other.m_stopTime;
    m_protocol = other.m_protocol;
    m_state = other.m_state;
    m_isSimulating = other.m_isSimulating;
    dsb::comm::CopyMessage(other.m_envelope, m_envelope);
    m_pendingSetVars = other.m_pendingSetVars;
    m_pendingConnectVars = other.m_pendingConnectVars;
    return *this;
}


bool SlaveTracker::RequestReply(
    zmq::socket_t& socket,
    std::deque<zmq::message_t>& envelope,
    std::deque<zmq::message_t>& msg)
{
    assert (!envelope.empty());
    assert (!msg.empty());
    bool sendImmediately = false;
    switch (dsb::control::ParseMessageType(msg.front())) {
        case dsbproto::control::MSG_HELLO:
            std::clog << "MSG_HELLO" << std::endl;
            sendImmediately = HelloHandler(msg);
            break;
        case dsbproto::control::MSG_SUBMIT:
            std::clog << "MSG_SUBMIT" << std::endl;
            sendImmediately = SubmitHandler(msg);
            break;
        case dsbproto::control::MSG_READY:
            std::clog << "MSG_READY" << std::endl;
            sendImmediately = ReadyHandler(msg);
            break;
        case dsbproto::control::MSG_STEP_OK:
            std::clog << "MSG_STEP_OK" << std::endl;
            sendImmediately = StepOkHandler(msg);
            break;
        case dsbproto::control::MSG_STEP_FAILED:
            std::clog << "MSG_STEP_FAILED" << std::endl;
            sendImmediately = StepFailedHandler(msg);
            break;
        default:
            std::clog << "Warning: Invalid message received from client: "
                        /*<< slaveId*/ << std::endl;
            CreateInvalidRequest(msg);
            sendImmediately = true;
            break;
    }
    if (sendImmediately) {
        dsb::comm::AddressedSend(socket, envelope, msg);
    } else {
        m_envelope.swap(envelope);  // Store envelope, thus clearing it.
        msg.clear();                // Clear msg too, for consistency.
    }
    assert (envelope.empty());
    assert (msg.empty());
    return sendImmediately;
}


void SlaveTracker::EnqueueSetVars(
    zmq::socket_t& socket,
    const dsbproto::control::SetVarsData& data)
{
    if (State() == SLAVE_READY) {
        assert (m_pendingSetVars.empty());
        std::deque<zmq::message_t> msg;
        dsb::control::CreateMessage(
            msg, dsbproto::control::MSG_SET_VARS, data);
        SendSynchronousMsg(socket, msg, SLAVE_READY, SLAVE_BUSY);
    } else {
        m_pendingSetVars.push(data);
    }
}


void SlaveTracker::EnqueueConnectVars(
    zmq::socket_t& socket,
    const dsbproto::control::ConnectVarsData& data)
{
    if (State() == SLAVE_READY) {
        assert (m_pendingConnectVars.empty());
        std::deque<zmq::message_t> msg;
        dsb::control::CreateMessage(
            msg, dsbproto::control::MSG_CONNECT_VARS, data);
        SendSynchronousMsg(socket, msg, SLAVE_READY, SLAVE_BUSY);
    } else {
        m_pendingConnectVars.push(data);
    }
}


void SlaveTracker::SendStep(
    zmq::socket_t& socket,
    const dsbproto::control::StepData& data)
{
    std::deque<zmq::message_t> msg;
    dsb::control::CreateMessage(msg, dsbproto::control::MSG_STEP, data);
    SendSynchronousMsg(socket, msg, SLAVE_READY, SLAVE_STEPPING);
    m_isSimulating = true;
}


void SlaveTracker::SendTerminate(zmq::socket_t& socket)
{
    std::deque<zmq::message_t> msg;
    dsb::control::CreateMessage(msg, dsbproto::control::MSG_TERMINATE);
    SendSynchronousMsg(socket, msg, TERMINATABLE_STATES, SLAVE_TERMINATED);
    m_isSimulating = false;
}


void SlaveTracker::SendRecvVars(zmq::socket_t& socket)
{
    assert (m_isSimulating);
    std::deque<zmq::message_t> msg;
    dsb::control::CreateMessage(msg, dsbproto::control::MSG_RECV_VARS);
    SendSynchronousMsg(socket, msg, SLAVE_PUBLISHED, SLAVE_RECEIVING);
}


void SlaveTracker::SendSynchronousMsg(
    zmq::socket_t& socket,
    std::deque<zmq::message_t>& msg,
    int allowedOldStates,
    SlaveState newState)
{
    assert (!m_envelope.empty());
    assert (!msg.empty());
    const auto rc = UpdateSlaveState(allowedOldStates, newState);
    assert (rc);
    dsb::comm::AddressedSend(socket, m_envelope, msg);
}


SlaveState SlaveTracker::State() const
{
    return m_state;
}


bool SlaveTracker::IsSimulating() const
{
    return m_isSimulating;
}


bool SlaveTracker::HelloHandler(std::deque<zmq::message_t>& msg)
{
    if (UpdateSlaveState(SLAVE_UNKNOWN, SLAVE_CONNECTING)) {
        const auto slaveProtocol = dsb::control::ParseHelloMessage(msg);
        if (slaveProtocol > 0) {
            std::clog << "Warning: Slave requested newer protocol version ("
                        << slaveProtocol << ")" << std::endl;
        }
        m_protocol = std::min(MAX_PROTOCOL, slaveProtocol);
        dsb::control::CreateHelloMessage(msg, m_protocol);
    } else {
        dsb::control::CreateDeniedMessage(msg, "Slave already connected");
    }
    return true;
}


bool SlaveTracker::SubmitHandler(std::deque<zmq::message_t>& msg)
{
    if (UpdateSlaveState(SLAVE_CONNECTING, SLAVE_CONNECTED)) {
        dsbproto::control::SetupData data;
        data.set_start_time(m_startTime);
        if (m_stopTime < std::numeric_limits<double>::infinity()) {
            data.set_stop_time(m_stopTime);
        }
        dsb::control::CreateMessage(msg, dsbproto::control::MSG_SETUP, data);
    } else {
        CreateInvalidRequest(msg);
    }
    return true;
}


bool SlaveTracker::ReadyHandler(std::deque<zmq::message_t>& msg)
{
    if (UpdateSlaveState(SLAVE_CONNECTED | SLAVE_BUSY | SLAVE_RECEIVING, SLAVE_READY)) {
        if (!m_pendingSetVars.empty()) {
            dsb::control::CreateMessage(
                msg, dsbproto::control::MSG_SET_VARS, m_pendingSetVars.front());
            m_pendingSetVars.pop();
            UpdateSlaveState(SLAVE_READY, SLAVE_BUSY);
            return true;
        } else if (!m_pendingConnectVars.empty()) {
            dsb::control::CreateMessage(
                msg, dsbproto::control::MSG_CONNECT_VARS, m_pendingConnectVars.front());
            m_pendingConnectVars.pop();
            UpdateSlaveState(SLAVE_READY, SLAVE_BUSY);
            return true;
        } else {
            return false;
        }
    } else {
        CreateInvalidRequest(msg);
        return true;
    }
}


bool SlaveTracker::StepFailedHandler(std::deque<zmq::message_t>& msg)
{
    if (UpdateSlaveState(SLAVE_STEPPING, SLAVE_STEP_FAILED)) {
        dsb::control::CreateMessage(msg, dsbproto::control::MSG_TERMINATE);
    } else {
        CreateInvalidRequest(msg);
    }
    return true;
}


bool SlaveTracker::StepOkHandler(std::deque<zmq::message_t>& msg)
{
    if (UpdateSlaveState(SLAVE_STEPPING, SLAVE_PUBLISHED)) {
        return false;
    } else {
        CreateInvalidRequest(msg);
        return true;
    }
}


bool SlaveTracker::UpdateSlaveState(int oldStates, SlaveState newState)
{
    if (m_state & oldStates) {
        m_state = newState;
        return true;
    } else {
        std::clog << "Warning: Slave in wrong state (" << m_state << ")" << std::endl;
        return false;
    }
}


}} // namespace
