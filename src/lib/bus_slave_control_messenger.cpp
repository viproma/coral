/*
Copyright 2013-2017, SINTEF Ocean and the Coral contributors.
This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#include <coral/bus/slave_control_messenger.hpp>

#include <cassert>
#include <utility>

#include <coral/bus/slave_control_messenger_v0.hpp>
#include <coral/error.hpp>
#include <coral/log.hpp>
#include <coral/net/zmqx.hpp>
#include <coral/protocol/execution.hpp>


namespace coral
{
namespace bus
{

namespace
{
    const int NO_TIMER = -1;
}


class PendingSlaveControlConnectionPrivate
{
public:
    PendingSlaveControlConnectionPrivate(
        coral::net::Reactor& reactor,
        const coral::net::SlaveLocator& slaveLocator,
        int maxAttempts,
        std::chrono::milliseconds timeout,
        ConnectToSlaveHandler onComplete);

    ~PendingSlaveControlConnectionPrivate()
    {
        // This object is never destroyed without the outer object
        // (PendingSlaveControlConnection) being destroyed and calling
        // Destroy() first.  Let's verify this:
        assert(!m_onComplete);
        assert(m_timeoutTimer == NO_TIMER);
    }

    bool Active() const CORAL_NOEXCEPT;

    // Aborts an ongoing connection attempt by simply resetting the socket
    // and cancelling the timeout timer and socket listener.  The completion
    // handler does NOT get called.
    void Destroy() CORAL_NOEXCEPT;

    // Does the same as Destroy(), except for two differences:
    //  - errors are reported (not noexcept)
    //  - completion handler is called
    void Close();

private:
    void TryConnect(int remainingAttempts);
    void HandleHelloReply();
    void HandleTimeout();
    void OnComplete(const std::error_code& ec, SlaveControlConnection scc);
    void CancelTimeoutTimer() CORAL_NOEXCEPT;

    coral::net::Reactor& m_reactor;
    const coral::net::SlaveLocator m_slaveLocator;
    const std::chrono::milliseconds m_timeout;

    ConnectToSlaveHandler m_onComplete;
    int m_timeoutTimer;
    coral::net::zmqx::ReqSocket m_socket;
};


struct SlaveControlConnectionPrivate
{
    coral::net::Reactor* reactor;
    coral::net::zmqx::ReqSocket socket;
    std::chrono::milliseconds timeout;
    int protocol;
};


PendingSlaveControlConnectionPrivate::PendingSlaveControlConnectionPrivate(
    coral::net::Reactor& reactor,
    const coral::net::SlaveLocator& slaveLocator,
    int maxAttempts,
    std::chrono::milliseconds timeout,
    ConnectToSlaveHandler onComplete)
    : m_reactor(reactor),
      m_slaveLocator(slaveLocator),
      m_timeout(timeout),
      m_onComplete(std::move(onComplete)),
      m_timeoutTimer(NO_TIMER),
      m_socket()
{
    TryConnect(maxAttempts);
    assert(m_timeoutTimer != NO_TIMER);
}


bool PendingSlaveControlConnectionPrivate::Active() const CORAL_NOEXCEPT
{
    return !!m_onComplete;
}


void PendingSlaveControlConnectionPrivate::Destroy() CORAL_NOEXCEPT
{
    if (Active()) {
        CancelTimeoutTimer();
        m_reactor.RemoveSocket(m_socket.Socket());
        m_socket = coral::net::zmqx::ReqSocket{};
        m_onComplete = nullptr;
    }
}


void PendingSlaveControlConnectionPrivate::Close()
{
    if (Active()) {
        CancelTimeoutTimer();
        m_reactor.RemoveSocket(m_socket.Socket());
        m_socket.Close();
        OnComplete(std::make_error_code(std::errc::operation_canceled), SlaveControlConnection());
    }
}


void PendingSlaveControlConnectionPrivate::TryConnect(int remainingAttempts)
{
    // Connect and send HELLO
    m_socket = coral::net::zmqx::ReqSocket{}; // reset to a fresh socket
    m_socket.Connect(m_slaveLocator.ControlEndpoint());
    CORAL_LOG_TRACE(boost::format("PendingSlaveControlConnectionPrivate  %x: "
            "Connecting to endpoint %s")
        % this % m_slaveLocator.ControlEndpoint().URL());

    std::vector<zmq::message_t> msg;
    coral::protocol::execution::CreateHelloMessage(msg, 0);
    m_socket.Send(msg);
    CORAL_LOG_TRACE(
        boost::format("PendingSlaveControlConnectionPrivate  %x: Sent HELLO")
        % this);

    // Register a timeout timer and a reply listener.
    // Both of these cancel the other if triggered.
    // If the timeout is infinite, use a finite one on the first attempts
    assert(m_timeoutTimer == NO_TIMER);
    auto timeout = m_timeout;
    if (timeout < std::chrono::milliseconds(0) && remainingAttempts > 1) {
        timeout = std::chrono::seconds(1);
        CORAL_LOG_DEBUG(boost::format(
            "PendingSlaveControlConnectionPrivate %x: Using default timeout "
            "(%d ms) for initial connection attempts.")
            % this % timeout.count());
    }
    if (timeout >= std::chrono::milliseconds(0)) {
        m_timeoutTimer = m_reactor.AddTimer(timeout, 1,
            [remainingAttempts, this](coral::net::Reactor& r, int id)
            {
                m_timeoutTimer = NO_TIMER;
                r.RemoveSocket(m_socket.Socket());
                if (remainingAttempts > 1) {
                    TryConnect(remainingAttempts-1);
                } else {
                    HandleTimeout();
                }
            });
    }
    m_reactor.AddSocket(
        m_socket.Socket(),
        [this] (coral::net::Reactor& r, zmq::socket_t& s) {
            CancelTimeoutTimer();
            r.RemoveSocket(s);
            HandleHelloReply();
        });
}


void PendingSlaveControlConnectionPrivate::HandleHelloReply()
{
    std::vector<zmq::message_t> msg;
    m_socket.Receive(msg);
    const auto reply = coral::protocol::execution::ParseMessageType(msg.front());
    CORAL_LOG_TRACE(
        boost::format("PendingSlaveControlConnectionPrivate  %x: Received %s")
        % this
        % coralproto::execution::MessageType_Name(
            static_cast<coralproto::execution::MessageType>(reply)));

    if (reply == coralproto::execution::MSG_HELLO) {
        auto p = std::make_unique<SlaveControlConnectionPrivate>();
        p->reactor = &m_reactor;
        p->socket = std::move(m_socket);
        p->timeout = m_timeout;
        p->protocol = coral::protocol::execution::ParseHelloMessage(msg);
        OnComplete(std::error_code(), SlaveControlConnection(std::move(p)));
    } else {
        m_socket.Close();
        std::error_code ec;
        if (reply == coralproto::execution::MSG_DENIED) {
            ec = make_error_code(std::errc::permission_denied);
        } else if (reply == coralproto::execution::MSG_ERROR) {
            ec = make_error_code(std::errc::connection_refused);
        } else {
            ec = make_error_code(std::errc::bad_message);
        }
        OnComplete(ec, SlaveControlConnection());
    }
}


void PendingSlaveControlConnectionPrivate::HandleTimeout()
{
    OnComplete(std::make_error_code(std::errc::timed_out), SlaveControlConnection());
}


void PendingSlaveControlConnectionPrivate::OnComplete(
    const std::error_code& ec,
    SlaveControlConnection scc)
{
    auto onComplete = std::move(m_onComplete);
    onComplete(ec, std::move(scc));
}


void PendingSlaveControlConnectionPrivate::CancelTimeoutTimer() CORAL_NOEXCEPT
{
    if (m_timeoutTimer == NO_TIMER) return;
    try { m_reactor.RemoveTimer(m_timeoutTimer); }
    catch (...) { assert(!"PendingSlaveControlConnection: Tried to cancel a nonexisting timer"); }
    m_timeoutTimer = NO_TIMER;
}


// =============================================================================
// Public API
// =============================================================================


// === PendingSlaveControlConnection ===

PendingSlaveControlConnection::PendingSlaveControlConnection(
    std::shared_ptr<PendingSlaveControlConnectionPrivate> p) CORAL_NOEXCEPT
    : m_private(std::move(p))
{ }

PendingSlaveControlConnection::PendingSlaveControlConnection(
    PendingSlaveControlConnection&& other) CORAL_NOEXCEPT
    : m_private(std::move(other.m_private))
{ }

PendingSlaveControlConnection& PendingSlaveControlConnection::operator=(
    PendingSlaveControlConnection&& other) CORAL_NOEXCEPT
{
    if (m_private) m_private->Destroy();
    m_private = std::move(other.m_private);
    return *this;
}

PendingSlaveControlConnection::~PendingSlaveControlConnection() CORAL_NOEXCEPT
{
    if (m_private) m_private->Destroy();
}

void PendingSlaveControlConnection::Close()
{
    if (m_private) m_private->Close();
}

PendingSlaveControlConnection::operator bool() const CORAL_NOEXCEPT
{
    return m_private && m_private->Active();
}


// === SlaveControlConnection ===

SlaveControlConnection::SlaveControlConnection() CORAL_NOEXCEPT
    : m_private()
{ }

SlaveControlConnection::SlaveControlConnection(
    std::unique_ptr<SlaveControlConnectionPrivate> p) CORAL_NOEXCEPT
    : m_private(std::move(p))
{ }

SlaveControlConnection::SlaveControlConnection(
    SlaveControlConnection&& other) CORAL_NOEXCEPT
{
    operator=(std::move(other));
}

SlaveControlConnection& SlaveControlConnection::operator=(
    SlaveControlConnection&& other) CORAL_NOEXCEPT
{
    m_private = std::move(other.m_private);
    return *this;
}

SlaveControlConnection::~SlaveControlConnection() CORAL_NOEXCEPT { }


SlaveControlConnection::operator bool() const CORAL_NOEXCEPT
{
    return !!m_private;
}

SlaveControlConnectionPrivate& SlaveControlConnection::Private()
{
    return *m_private;
}


// === Free functions ===

PendingSlaveControlConnection ConnectToSlave(
    coral::net::Reactor& reactor,
    const coral::net::SlaveLocator& slaveLocator,
    int maxAttempts,
    std::chrono::milliseconds timeout,
    ConnectToSlaveHandler onComplete)
{
    CORAL_INPUT_CHECK(maxAttempts > 0);
    CORAL_INPUT_CHECK(onComplete);

    return PendingSlaveControlConnection(
        std::make_shared<PendingSlaveControlConnectionPrivate>(
            reactor,
            slaveLocator,
            maxAttempts,
            timeout,
            std::move(onComplete)));
}


std::unique_ptr<ISlaveControlMessenger> MakeSlaveControlMessenger(
    SlaveControlConnection connection,
    coral::model::SlaveID slaveID,
    const std::string& slaveName,
    const SlaveSetup& setup,
    MakeSlaveControlMessengerHandler onComplete)
{
    CORAL_INPUT_CHECK(connection);
    CORAL_INPUT_CHECK(slaveID != coral::model::INVALID_SLAVE_ID);
    CORAL_INPUT_CHECK(onComplete);
    if (connection.Private().protocol == 0) {
        return std::make_unique<coral::bus::SlaveControlMessengerV0>(
            *connection.Private().reactor,
            std::move(connection.Private().socket),
            slaveID,
            slaveName,
            setup,
            connection.Private().timeout,
            std::move(onComplete));
    } else {
        return nullptr;
    }
}

}} // namespace
