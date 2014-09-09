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


SlaveTracker::SlaveTracker()
    : m_protocol(UNKNOWN_PROTOCOL),
      m_state(SLAVE_UNKNOWN),
      m_isSimulating(false)
{
}


SlaveTracker::SlaveTracker(const SlaveTracker& other)
{
    operator=(other);
}


SlaveTracker& SlaveTracker::operator=(const SlaveTracker& other) {
    m_protocol = other.m_protocol;
    m_state = other.m_state;
    m_isSimulating = other.m_isSimulating;
    dsb::comm::CopyMessage(other.m_envelope, m_envelope);
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
    const auto slaveProtocol = dsb::control::ParseHelloMessage(msg);
    if (slaveProtocol > 0) {
        std::clog << "Warning: Slave requested newer protocol version ("
                  << slaveProtocol << ")" << std::endl;
    }
    m_protocol = std::min(MAX_PROTOCOL, slaveProtocol);
    m_state = SLAVE_CONNECTING;
    dsb::control::CreateHelloMessage(msg, m_protocol);
    return true;
}


bool SlaveTracker::SubmitHandler(std::deque<zmq::message_t>& msg)
{
    if (UpdateSlaveState(SLAVE_CONNECTING | SLAVE_CONNECTED, SLAVE_CONNECTED)) {
        dsb::control::CreateMessage(msg, dsbproto::control::MSG_SETUP);
    } else {
        CreateInvalidRequest(msg);
    }
    return true;
}


bool SlaveTracker::ReadyHandler(std::deque<zmq::message_t>& msg)
{
    if (UpdateSlaveState(SLAVE_CONNECTED | SLAVE_READY | SLAVE_RECEIVING, SLAVE_READY)) {
        return false;
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
