#include "dsb/bus/slave_control_messenger.hpp"

#include <cassert>
#include <utility>

#include "dsb/bus/slave_control_messenger_v0.hpp"
#include "dsb/comm/socket.hpp"
#include "dsb/error.hpp"
#include "dsb/log.hpp"
#include "dsb/protocol/execution.hpp"


namespace dsb
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
        dsb::comm::Reactor& reactor,
        const dsb::net::SlaveLocator& slaveLocator,
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

    bool Active() const DSB_NOEXCEPT;

    // Aborts an ongoing connection attempt by simply resetting the socket
    // and cancelling the timeout timer and socket listener.  The completion
    // handler does NOT get called.
    void Destroy() DSB_NOEXCEPT;

    // Does the same as Destroy(), except for two differences:
    //  - errors are reported (not noexcept)
    //  - completion handler is called
    void Close();

private:
    void TryConnect(int remainingAttempts);
    void HandleHelloReply();
    void HandleTimeout();
    void OnComplete(const std::error_code& ec, SlaveControlConnection scc);
    void CancelTimeoutTimer() DSB_NOEXCEPT;

    dsb::comm::Reactor& m_reactor;
    const dsb::net::SlaveLocator m_slaveLocator;
    const std::chrono::milliseconds m_timeout;

    ConnectToSlaveHandler m_onComplete;
    int m_timeoutTimer;
    dsb::comm::ReqSocket m_socket;
};


struct SlaveControlConnectionPrivate
{
    dsb::comm::Reactor* reactor;
    dsb::comm::ReqSocket socket;
    std::chrono::milliseconds timeout;
    int protocol;
};


PendingSlaveControlConnectionPrivate::PendingSlaveControlConnectionPrivate(
    dsb::comm::Reactor& reactor,
    const dsb::net::SlaveLocator& slaveLocator,
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


bool PendingSlaveControlConnectionPrivate::Active() const DSB_NOEXCEPT
{
    return !!m_onComplete;
}


void PendingSlaveControlConnectionPrivate::Destroy() DSB_NOEXCEPT
{
    if (Active()) {
        CancelTimeoutTimer();
        m_reactor.RemoveSocket(m_socket.Socket());
        m_socket = dsb::comm::ReqSocket{};
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
    m_socket = dsb::comm::ReqSocket{}; // reset to a fresh socket
    m_socket.Connect(m_slaveLocator.ControlEndpoint());
    DSB_LOG_TRACE(boost::format("PendingSlaveControlConnectionPrivate  %x: "
            "Connecting to endpoint %s")
        % this % m_slaveLocator.ControlEndpoint().URL());

    std::vector<zmq::message_t> msg;
    dsb::protocol::execution::CreateHelloMessage(msg, 0);
    m_socket.Send(msg);
    DSB_LOG_TRACE(
        boost::format("PendingSlaveControlConnectionPrivate  %x: Sent HELLO")
        % this);

    // Register a timeout timer and a reply listener.
    // Both of these cancel the other if triggered.
    assert(m_timeoutTimer == NO_TIMER);
    m_timeoutTimer = m_reactor.AddTimer(m_timeout, 1,
        [remainingAttempts, this](dsb::comm::Reactor& r, int id)
        {
            m_timeoutTimer = NO_TIMER;
            r.RemoveSocket(m_socket.Socket());
            if (remainingAttempts > 1) {
                TryConnect(remainingAttempts-1);
            } else {
                HandleTimeout();
            }
        });
    m_reactor.AddSocket(
        m_socket.Socket(),
        [this] (dsb::comm::Reactor& r, zmq::socket_t& s) {
            CancelTimeoutTimer();
            r.RemoveSocket(s);
            HandleHelloReply();
        });
}


void PendingSlaveControlConnectionPrivate::HandleHelloReply()
{
    std::vector<zmq::message_t> msg;
    m_socket.Receive(msg);
    const auto reply = dsb::protocol::execution::ParseMessageType(msg.front());
    DSB_LOG_TRACE(
        boost::format("PendingSlaveControlConnectionPrivate  %x: Received %s")
        % this
        % dsbproto::execution::MessageType_Name(
            static_cast<dsbproto::execution::MessageType>(reply)));

    if (reply == dsbproto::execution::MSG_HELLO) {
        auto p = std::make_unique<SlaveControlConnectionPrivate>();
        p->reactor = &m_reactor;
        p->socket = std::move(m_socket);
        p->timeout = m_timeout;
        p->protocol = dsb::protocol::execution::ParseHelloMessage(msg);
        OnComplete(std::error_code(), SlaveControlConnection(std::move(p)));
    } else {
        m_socket.Close();
        std::error_code ec;
        if (reply == dsbproto::execution::MSG_DENIED) {
            ec = make_error_code(std::errc::permission_denied);
        } else if (reply == dsbproto::execution::MSG_ERROR) {
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


void PendingSlaveControlConnectionPrivate::CancelTimeoutTimer() DSB_NOEXCEPT
{
    assert(m_timeoutTimer != NO_TIMER);
    try { m_reactor.RemoveTimer(m_timeoutTimer); }
    catch (...) { assert(!"PendingSlaveControlConnection: Tried to cancel a nonexisting timer"); }
    m_timeoutTimer = NO_TIMER;
}


// =============================================================================
// Public API
// =============================================================================


// === PendingSlaveControlConnection ===

PendingSlaveControlConnection::PendingSlaveControlConnection(
    std::shared_ptr<PendingSlaveControlConnectionPrivate> p) DSB_NOEXCEPT
    : m_private(std::move(p))
{ }

PendingSlaveControlConnection::PendingSlaveControlConnection(
    PendingSlaveControlConnection&& other) DSB_NOEXCEPT
    : m_private(std::move(other.m_private))
{ }

PendingSlaveControlConnection& PendingSlaveControlConnection::operator=(
    PendingSlaveControlConnection&& other) DSB_NOEXCEPT
{
    if (m_private) m_private->Destroy();
    m_private = std::move(other.m_private);
    return *this;
}

PendingSlaveControlConnection::~PendingSlaveControlConnection() DSB_NOEXCEPT
{
    if (m_private) m_private->Destroy();
}

void PendingSlaveControlConnection::Close()
{
    if (m_private) m_private->Close();
}

PendingSlaveControlConnection::operator bool() const DSB_NOEXCEPT
{
    return m_private && m_private->Active();
}


// === SlaveControlConnection ===

SlaveControlConnection::SlaveControlConnection() DSB_NOEXCEPT
    : m_private()
{ }

SlaveControlConnection::SlaveControlConnection(
    std::unique_ptr<SlaveControlConnectionPrivate> p) DSB_NOEXCEPT
    : m_private(std::move(p))
{ }

SlaveControlConnection::SlaveControlConnection(
    SlaveControlConnection&& other) DSB_NOEXCEPT
{
    operator=(std::move(other));
}

SlaveControlConnection& SlaveControlConnection::operator=(
    SlaveControlConnection&& other) DSB_NOEXCEPT
{
    m_private = std::move(other.m_private);
    return *this;
}

SlaveControlConnection::~SlaveControlConnection() DSB_NOEXCEPT { }


SlaveControlConnection::operator bool() const DSB_NOEXCEPT
{
    return !!m_private;
}

SlaveControlConnectionPrivate& SlaveControlConnection::Private()
{
    return *m_private;
}


// === Free functions ===

PendingSlaveControlConnection ConnectToSlave(
    dsb::comm::Reactor& reactor,
    const dsb::net::SlaveLocator& slaveLocator,
    int maxAttempts,
    std::chrono::milliseconds timeout,
    ConnectToSlaveHandler onComplete)
{
    DSB_INPUT_CHECK(maxAttempts > 0);
    DSB_INPUT_CHECK(timeout > std::chrono::milliseconds(0));
    DSB_INPUT_CHECK(onComplete);

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
    dsb::model::SlaveID slaveID,
    const std::string& slaveName,
    const SlaveSetup& setup,
    MakeSlaveControlMessengerHandler onComplete)
{
    DSB_INPUT_CHECK(connection);
    DSB_INPUT_CHECK(slaveID != dsb::model::INVALID_SLAVE_ID);
    DSB_INPUT_CHECK(onComplete);
    if (connection.Private().protocol == 0) {
        return std::make_unique<dsb::bus::SlaveControlMessengerV0>(
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
