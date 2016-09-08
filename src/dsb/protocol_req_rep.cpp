#include "dsb/protocol/req_rep.hpp"

#include "dsb/error.hpp"
#include "dsb/net/zmqx.hpp"
#include "dsb/util.hpp"


namespace dsb
{
namespace protocol
{

namespace
{
    const std::string META_PROTOCOL_IDENTIFIER = "DSRRMETA";
    const std::string META_REQ_MAX_PROTOCOL_VERSION = "MAX_PROTOCOL_VERSION";
    const std::string META_REP_OK = "OK";
    const std::string META_REP_ERROR = "ERROR";
    const int NO_TIMER = -1;
    const std::uint16_t INVALID_PROTOCOL_VERSION = 0xFFFFu;
}


// =============================================================================
// RRClient
// =============================================================================

RRClient::RRClient(
    dsb::net::Reactor& reactor,
    const std::string& protocolIdentifier,
    const dsb::net::Endpoint& serverEndpoint)
    : m_reactor{reactor}
    , m_protocolIdentifier(protocolIdentifier)
    , m_serverEndpoint{serverEndpoint}
    , m_timeoutTimerID{NO_TIMER}
{
    DSB_INPUT_CHECK(!protocolIdentifier.empty());
    if (protocolIdentifier == META_PROTOCOL_IDENTIFIER) {
        throw std::invalid_argument(
            META_PROTOCOL_IDENTIFIER + " is a reserved protocol identifier");
    }
    m_socket.Connect(m_serverEndpoint);
    m_reactor.AddSocket(
        m_socket.Socket(),
        [this] (dsb::net::Reactor&, zmq::socket_t&) {
            ReceiveReply();
        });
}


RRClient::~RRClient() DSB_NOEXCEPT
{
    m_reactor.RemoveSocket(m_socket.Socket());
    if (m_timeoutTimerID != NO_TIMER) CancelTimer();
}


void RRClient::Request(
    std::uint16_t protocolVersion,
    const char* requestHeader, size_t requestHeaderSize,
    const char* requestBody, size_t requestBodySize,
    std::chrono::milliseconds timeout,
    ReplyHandler onComplete)
{
    if (m_onComplete || m_onMaxProtocolComplete) {
        throw dsb::error::PreconditionViolation(
            "Request already in progress");
    }
    if (protocolVersion == INVALID_PROTOCOL_VERSION) {
        throw std::invalid_argument(
            "Protocol version number is reserved for internal use");
    }
    SendRequest(
        m_protocolIdentifier, protocolVersion,
        requestHeader, requestHeaderSize,
        requestBody, requestBodySize,
        timeout);
    SetTimer(timeout);
    m_requestProtocolVersion = protocolVersion;
    m_onComplete = std::move(onComplete);
}


void RRClient::RequestMaxProtocol(
    std::chrono::milliseconds timeout,
    MaxProtocolReplyHandler onComplete)
{
    if (m_onComplete || m_onMaxProtocolComplete) {
        throw dsb::error::PreconditionViolation(
            "Request already in progress");
    }
    const std::uint16_t protocolVersion = 0u;
    SendRequest(
        META_PROTOCOL_IDENTIFIER, protocolVersion,
        META_REQ_MAX_PROTOCOL_VERSION.data(), META_REQ_MAX_PROTOCOL_VERSION.size(),
        m_protocolIdentifier.data(), m_protocolIdentifier.size(),
        timeout);
    SetTimer(timeout);
    m_requestProtocolVersion = protocolVersion;
    m_onMaxProtocolComplete = std::move(onComplete);
}


void RRClient::SendRequest(
    const std::string& protocolIdentifier, std::uint16_t protocolVersion,
    const char* requestHeader, size_t requestHeaderSize,
    const char* requestBody, size_t requestBodySize,
    std::chrono::milliseconds timeout)
{
    assert(!protocolIdentifier.empty());
    assert(requestHeader != nullptr);

    std::vector<zmq::message_t> msg;
    msg.emplace_back(protocolIdentifier.size() + 2);
    std::memcpy(
        msg.back().data(),
        protocolIdentifier.data(),
        protocolIdentifier.size());
    dsb::util::EncodeUint16(
        protocolVersion,
        static_cast<char*>(msg.back().data()) + protocolIdentifier.size());
    msg.emplace_back(requestHeader, requestHeaderSize);
    if (requestBody != nullptr) msg.emplace_back(requestBody, requestBodySize);

    if (!dsb::net::zmqx::WaitForOutgoing(m_socket.Socket(), timeout)) {
        throw std::runtime_error("Send timed out");
    }
    m_socket.Send(msg);
}


namespace
{
    // Parses the reply and calls the callback handler for a MaxProtocol
    // request.  On entry, `handler` must refer to a valid function object.
    // On return the function object will have been called and `handler`
    // will be empty.
    void HandleMetaMaxProtocolReply(
        const zmq::message_t& header,
        const zmq::message_t* body,
        RRClient::MaxProtocolReplyHandler& handler)
    {
        assert(handler);
        const auto h = dsb::net::zmqx::ToString(header);
        if (h == META_REP_OK && body != nullptr && body->size() == 2) {
            dsb::util::LastCall(handler,
                std::error_code(),
                dsb::util::DecodeUint16(static_cast<const char*>(body->data())));
        } else if (h == META_REP_ERROR) {
            dsb::util::LastCall(handler,
                make_error_code(std::errc::protocol_not_supported),
                INVALID_PROTOCOL_VERSION);
        } else {
            dsb::util::LastCall(handler,
                make_error_code(std::errc::bad_message),
                INVALID_PROTOCOL_VERSION);
        }
        assert(!handler);
    }
}


void RRClient::ReceiveReply()
{
    // Receive message, but if we didn't expect one (no handlers = no request
    // in progress), just ignore it and return.
    std::vector<zmq::message_t> msg;
    m_socket.Receive(msg);
    if (!m_onComplete && !m_onMaxProtocolComplete) return;

    CancelTimer();

    if (msg.size() < 2 || msg[0].size() < 3) {
        CompleteWithError(make_error_code(std::errc::bad_message));
        return;
    }
    const auto protocolIdentifier = std::string{
        static_cast<const char*>(msg[0].data()),
        msg[0].size() - 2};
    const auto protocolVersion = dsb::util::DecodeUint16(
        static_cast<const char*>(msg[0].data()) + protocolIdentifier.size());

    if (protocolVersion != m_requestProtocolVersion) {
        CompleteWithError(make_error_code(std::errc::bad_message));
    } else if (m_onComplete && protocolIdentifier == m_protocolIdentifier) {
        dsb::util::LastCall(m_onComplete,
            std::error_code(),
            static_cast<const char*>(msg[1].data()),
            msg[1].size(),
            msg.size() > 2 ? static_cast<const char*>(msg[2].data()) : nullptr,
            msg.size() > 2 ? msg[2].size() : 0u);
    } else if (m_onMaxProtocolComplete
                && protocolIdentifier == META_PROTOCOL_IDENTIFIER) {
        HandleMetaMaxProtocolReply(
            msg[1],
            msg.size() > 2 ? &msg[2] : nullptr,
            m_onMaxProtocolComplete);
    } else {
        CompleteWithError(make_error_code(std::errc::bad_message));
    }
}


void RRClient::CompleteWithError(const std::error_code& ec)
{
    assert(ec);
    if (m_onComplete) {
        dsb::util::LastCall(m_onComplete, ec, nullptr, 0u, nullptr, 0u);
    } else if (m_onMaxProtocolComplete) {
        dsb::util::LastCall(m_onMaxProtocolComplete, ec, INVALID_PROTOCOL_VERSION);
    } else {
        assert(false);
    }
}


void RRClient::SetTimer(std::chrono::milliseconds timeout)
{
    assert(m_timeoutTimerID == NO_TIMER);
    m_timeoutTimerID = m_reactor.AddTimer(timeout, 1,
        [this] (dsb::net::Reactor&, int) {
            m_timeoutTimerID = NO_TIMER;
            CompleteWithError(make_error_code(std::errc::timed_out));
        });
}


void RRClient::CancelTimer()
{
    assert(m_timeoutTimerID != NO_TIMER);
    m_reactor.RemoveTimer(m_timeoutTimerID);
    m_timeoutTimerID = NO_TIMER;
}


// =============================================================================
// RRServer
// =============================================================================

namespace
{
    void Copy(const char* cstr, std::vector<char>& vec)
    {
        vec.clear();
        while (cstr && *cstr) vec.push_back(*(cstr++));
    }
}


class RRServer::Private
{
public:
    Private(
        dsb::net::Reactor& reactor,
        const dsb::net::Endpoint& endpoint)
        : m_reactor{reactor}
    {
        m_socket.Bind(endpoint);
        m_reactor.AddSocket(
            m_socket.Socket(),
            [this] (dsb::net::Reactor&, zmq::socket_t&) {
                HandleRequest();
            });
    }

    ~Private() DSB_NOEXCEPT
    {
        m_reactor.RemoveSocket(m_socket.Socket());
    }

    Private(const Private&) = delete;
    Private(Private&&) = delete;
    Private& operator=(const Private&) = delete;
    Private& operator=(Private&&) = delete;

    void AddProtocolHandler(
        const std::string& protocolIdentifier,
        std::uint16_t protocolVersion,
        std::shared_ptr<RRServerProtocolHandler> handler)
    {
        if (protocolIdentifier.empty()) {
            throw std::invalid_argument("Protocol identifier is empty");
        }
        if (protocolIdentifier == META_PROTOCOL_IDENTIFIER) {
            throw std::invalid_argument(
                META_PROTOCOL_IDENTIFIER + " is a reserved protocol identifier");
        }
        const auto pi = m_handlers.find(protocolIdentifier);
        if (pi != m_handlers.end() && pi->second.count(protocolVersion) > 0) {
            throw std::invalid_argument(
                "A handler already exists for this protocol version");
        }
        m_handlers[protocolIdentifier][protocolVersion] = handler;
    }

    dsb::net::Endpoint BoundEndpoint() const
    {
        return m_socket.BoundEndpoint();
    }

private:
    void HandleRequest()
    {
        std::vector<zmq::message_t> msg;
        m_socket.Receive(msg);
        if (msg.size() < 2 || msg[0].size() < 3) {
            // Ignore request
            return;
        }
        const auto protocolIdentifier = std::string{
            static_cast<const char*>(msg[0].data()),
            msg[0].size() - 2};
        const auto protocolVersion = dsb::util::DecodeUint16(
            static_cast<const char*>(msg[0].data()) + protocolIdentifier.size());

        const char* replyHeader = nullptr;
        size_t replyHeaderSize = 0u;
        const char* replyBody = nullptr;
        size_t replyBodySize = 0u;

        const auto hasReply = DispatchRequest(
            protocolIdentifier,
            protocolVersion,
            static_cast<const char*>(msg[1].data()),
            msg[1].size(),
            msg.size() > 2 ? static_cast<const char*>(msg[2].data()) : nullptr,
            msg.size() > 2 ? msg[2].size() : 0u,
            replyHeader,
            replyHeaderSize,
            replyBody,
            replyBodySize);
        if (hasReply) {
            assert(replyHeader != nullptr);
            msg[1].rebuild(replyHeader, replyHeaderSize);
            if (replyBody != nullptr) {
                msg.resize(3);
                msg[2].rebuild(replyBody, replyBodySize);
            } else {
                msg.resize(2);
            }
            m_socket.Send(msg);
        } // else ignore request
    }

    bool DispatchRequest(
        const std::string& protocolIdentifier,
        std::uint16_t protocolVersion,
        const char* requestHeader, size_t requestHeaderSize,
        const char* requestBody, size_t requestBodySize,
        const char*& replyHeader, size_t& replyHeaderSize,
        const char*& replyBody, size_t& replyBodySize)
    {
        if (protocolIdentifier == META_PROTOCOL_IDENTIFIER) {
            return HandleMetaRequest(
                protocolVersion,
                requestHeader, requestHeaderSize,
                requestBody, requestBodySize,
                replyHeader, replyHeaderSize,
                replyBody, replyBodySize);
        }

        const auto pi = m_handlers.find(protocolIdentifier);
        if (pi == m_handlers.end()) return false;

        const auto pv = pi->second.find(protocolVersion);
        if (pv == pi->second.end()) return false;

        return pv->second->HandleRequest(
            protocolIdentifier,
            protocolVersion,
            requestHeader, requestHeaderSize,
            requestBody, requestBodySize,
            replyHeader, replyHeaderSize,
            replyBody, replyBodySize);
    }

    bool HandleMetaRequest(
        std::uint16_t protocolVersion,
        const char* requestHeader, size_t requestHeaderSize,
        const char* requestBody, size_t requestBodySize,
        const char*& replyHeader, size_t& replyHeaderSize,
        const char*& replyBody, size_t& replyBodySize)
    {
        if (protocolVersion != 0) return false;
        if (0 == dsb::util::ArrayStringCmp(
                requestHeader, requestHeaderSize, META_REQ_MAX_PROTOCOL_VERSION.c_str())
            && requestBody != nullptr)
        {
            const auto requestedID = std::string{requestBody, requestBodySize};
            const auto pi = m_handlers.find(requestedID);
            if (pi == m_handlers.end()) {
                m_metaReplyHeader = META_REP_ERROR;
                Copy("Protocol not supported", m_metaReplyBody);
            } else {
                const auto pv = pi->second.rbegin();
                assert(pv != pi->second.rend());
                m_metaReplyHeader = META_REP_OK;
                m_metaReplyBody.resize(2);
                dsb::util::EncodeUint16(pv->first, m_metaReplyBody.data());
            }
        } else return false;

        replyHeader = m_metaReplyHeader.data();
        replyHeaderSize = m_metaReplyHeader.size();
        replyBody = m_metaReplyBody.data();
        replyBodySize = m_metaReplyBody.size();
        return true;
    }

    dsb::net::Reactor& m_reactor;
    dsb::net::zmqx::RepSocket m_socket;
    std::unordered_map<
            std::string,
            std::map<std::uint16_t, std::shared_ptr<RRServerProtocolHandler>>>
        m_handlers;

    std::string m_metaReplyHeader;
    std::vector<char> m_metaReplyBody;
};


RRServer::RRServer(
    dsb::net::Reactor& reactor,
    const dsb::net::Endpoint& endpoint)
    : m_private{std::make_unique<Private>(reactor, endpoint)}
{
}


RRServer::~RRServer() DSB_NOEXCEPT
{
    // Do nothing, rely on ~Private().
}


RRServer::RRServer(RRServer&& other) DSB_NOEXCEPT
    : m_private(std::move(other.m_private))
{
}


RRServer& RRServer::operator=(RRServer&& other) DSB_NOEXCEPT
{
    m_private = std::move(other.m_private);
    return *this;
}


void RRServer::AddProtocolHandler(
    const std::string& protocolIdentifier,
    std::uint16_t protocolVersion,
    std::shared_ptr<RRServerProtocolHandler> handler)
{
    m_private->AddProtocolHandler(
        protocolIdentifier,
        protocolVersion,
        std::move(handler));
}


dsb::net::Endpoint RRServer::BoundEndpoint() const
{
    return m_private->BoundEndpoint();
}


}} // namespace
