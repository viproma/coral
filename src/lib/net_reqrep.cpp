/*
Copyright 2013-present, SINTEF Ocean.
This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#include <coral/net/reqrep.hpp>

#include <coral/error.hpp>
#include <coral/net/zmqx.hpp>
#include <coral/util.hpp>


namespace coral
{
namespace net
{
namespace reqrep
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
// Client
// =============================================================================

Client::Client(
    coral::net::Reactor& reactor,
    const std::string& protocolIdentifier,
    const coral::net::Endpoint& serverEndpoint)
    : m_reactor{reactor}
    , m_protocolIdentifier(protocolIdentifier)
    , m_serverEndpoint{serverEndpoint}
    , m_timeoutTimerID{NO_TIMER}
{
    CORAL_INPUT_CHECK(!protocolIdentifier.empty());
    if (protocolIdentifier == META_PROTOCOL_IDENTIFIER) {
        throw std::invalid_argument(
            META_PROTOCOL_IDENTIFIER + " is a reserved protocol identifier");
    }
    m_socket.Connect(m_serverEndpoint);
    m_reactor.AddSocket(
        m_socket.Socket(),
        [this] (coral::net::Reactor&, zmq::socket_t&) {
            ReceiveReply();
        });
}


Client::~Client() noexcept
{
    m_reactor.RemoveSocket(m_socket.Socket());
    if (m_timeoutTimerID != NO_TIMER) CancelTimer();
}


void Client::Request(
    std::uint16_t protocolVersion,
    const char* requestHeader, size_t requestHeaderSize,
    const char* requestBody, size_t requestBodySize,
    std::chrono::milliseconds timeout,
    ReplyHandler onComplete)
{
    if (m_onComplete || m_onMaxProtocolComplete) {
        throw coral::error::PreconditionViolation(
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
    if (timeout >= std::chrono::milliseconds(0)) {
        SetTimer(timeout);
    }
    m_requestProtocolVersion = protocolVersion;
    m_onComplete = std::move(onComplete);
}


void Client::RequestMaxProtocol(
    std::chrono::milliseconds timeout,
    MaxProtocolReplyHandler onComplete)
{
    if (m_onComplete || m_onMaxProtocolComplete) {
        throw coral::error::PreconditionViolation(
            "Request already in progress");
    }
    const std::uint16_t protocolVersion = 0u;
    SendRequest(
        META_PROTOCOL_IDENTIFIER, protocolVersion,
        META_REQ_MAX_PROTOCOL_VERSION.data(), META_REQ_MAX_PROTOCOL_VERSION.size(),
        m_protocolIdentifier.data(), m_protocolIdentifier.size(),
        timeout);
    if (timeout >= std::chrono::milliseconds(0)) {
        SetTimer(timeout);
    }
    m_requestProtocolVersion = protocolVersion;
    m_onMaxProtocolComplete = std::move(onComplete);
}


void Client::SendRequest(
    const std::string& protocolIdentifier, std::uint16_t protocolVersion,
    const char* requestHeader, size_t requestHeaderSize,
    const char* requestBody, size_t requestBodySize,
    std::chrono::milliseconds timeout)
{
    assert(!protocolIdentifier.empty());
    assert(protocolVersion != INVALID_PROTOCOL_VERSION);
    assert(requestHeader != nullptr);

    std::vector<zmq::message_t> msg;
    msg.emplace_back(protocolIdentifier.size() + 2);
    std::memcpy(
        msg.back().data(),
        protocolIdentifier.data(),
        protocolIdentifier.size());
    coral::util::EncodeUint16(
        protocolVersion,
        static_cast<char*>(msg.back().data()) + protocolIdentifier.size());
    msg.emplace_back(requestHeader, requestHeaderSize);
    if (requestBody != nullptr) msg.emplace_back(requestBody, requestBodySize);

    if (!coral::net::zmqx::WaitForOutgoing(m_socket.Socket(), timeout)) {
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
        Client::MaxProtocolReplyHandler& handler)
    {
        assert(handler);
        const auto h = coral::net::zmqx::ToString(header);
        if (h == META_REP_OK && body != nullptr && body->size() == 2) {
            coral::util::LastCall(handler,
                std::error_code(),
                coral::util::DecodeUint16(static_cast<const char*>(body->data())));
        } else if (h == META_REP_ERROR) {
            coral::util::LastCall(handler,
                make_error_code(std::errc::protocol_not_supported),
                INVALID_PROTOCOL_VERSION);
        } else {
            coral::util::LastCall(handler,
                make_error_code(std::errc::bad_message),
                INVALID_PROTOCOL_VERSION);
        }
        assert(!handler);
    }
}


void Client::ReceiveReply()
{
    // Receive message, but if we didn't expect one (no handlers = no request
    // in progress), just ignore it and return.
    std::vector<zmq::message_t> msg;
    m_socket.Receive(msg);
    if (!m_onComplete && !m_onMaxProtocolComplete) return;

    if (m_timeoutTimerID != NO_TIMER) CancelTimer();

    if (msg.size() < 2 || msg[0].size() < 3) {
        CompleteWithError(make_error_code(std::errc::bad_message));
        return;
    }
    const auto protocolIdentifier = std::string{
        static_cast<const char*>(msg[0].data()),
        msg[0].size() - 2};
    const auto protocolVersion = coral::util::DecodeUint16(
        static_cast<const char*>(msg[0].data()) + protocolIdentifier.size());

    if (protocolVersion != m_requestProtocolVersion) {
        CompleteWithError(make_error_code(std::errc::bad_message));
    } else if (m_onComplete && protocolIdentifier == m_protocolIdentifier) {
        coral::util::LastCall(m_onComplete,
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


void Client::CompleteWithError(const std::error_code& ec)
{
    assert(ec);
    if (m_onComplete) {
        coral::util::LastCall(m_onComplete, ec, nullptr, 0u, nullptr, 0u);
    } else if (m_onMaxProtocolComplete) {
        coral::util::LastCall(m_onMaxProtocolComplete, ec, INVALID_PROTOCOL_VERSION);
    } else {
        assert(false);
    }
}


void Client::SetTimer(std::chrono::milliseconds timeout)
{
    assert(m_timeoutTimerID == NO_TIMER);
    assert(timeout >= std::chrono::milliseconds(0));
    m_timeoutTimerID = m_reactor.AddTimer(timeout, 1,
        [this] (coral::net::Reactor&, int) {
            m_timeoutTimerID = NO_TIMER;
            CompleteWithError(make_error_code(std::errc::timed_out));
        });
}


void Client::CancelTimer()
{
    assert(m_timeoutTimerID != NO_TIMER);
    m_reactor.RemoveTimer(m_timeoutTimerID);
    m_timeoutTimerID = NO_TIMER;
}


// =============================================================================
// Server
// =============================================================================

namespace
{
    void Copy(const char* cstr, std::vector<char>& vec)
    {
        vec.clear();
        while (cstr && *cstr) vec.push_back(*(cstr++));
    }
}


class Server::Private
{
public:
    Private(
        coral::net::Reactor& reactor,
        const coral::net::Endpoint& endpoint)
        : m_reactor{reactor}
    {
        m_socket.Bind(endpoint);
        m_reactor.AddSocket(
            m_socket.Socket(),
            [this] (coral::net::Reactor&, zmq::socket_t&) {
                HandleRequest();
            });
    }

    ~Private() noexcept
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
        std::shared_ptr<ServerProtocolHandler> handler)
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

    coral::net::Endpoint BoundEndpoint() const
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
        const auto protocolVersion = coral::util::DecodeUint16(
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
        if (0 == coral::util::ArrayStringCmp(
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
                coral::util::EncodeUint16(pv->first, m_metaReplyBody.data());
            }
        } else return false;

        replyHeader = m_metaReplyHeader.data();
        replyHeaderSize = m_metaReplyHeader.size();
        replyBody = m_metaReplyBody.data();
        replyBodySize = m_metaReplyBody.size();
        return true;
    }

    coral::net::Reactor& m_reactor;
    coral::net::zmqx::RepSocket m_socket;
    std::unordered_map<
            std::string,
            std::map<std::uint16_t, std::shared_ptr<ServerProtocolHandler>>>
        m_handlers;

    std::string m_metaReplyHeader;
    std::vector<char> m_metaReplyBody;
};


Server::Server(
    coral::net::Reactor& reactor,
    const coral::net::Endpoint& endpoint)
    : m_private{std::make_unique<Private>(reactor, endpoint)}
{
}


Server::~Server() noexcept
{
    // Do nothing, rely on ~Private().
}


Server::Server(Server&& other) noexcept
    : m_private(std::move(other.m_private))
{
}


Server& Server::operator=(Server&& other) noexcept
{
    m_private = std::move(other.m_private);
    return *this;
}


void Server::AddProtocolHandler(
    const std::string& protocolIdentifier,
    std::uint16_t protocolVersion,
    std::shared_ptr<ServerProtocolHandler> handler)
{
    m_private->AddProtocolHandler(
        protocolIdentifier,
        protocolVersion,
        std::move(handler));
}


coral::net::Endpoint Server::BoundEndpoint() const
{
    return m_private->BoundEndpoint();
}


}}} // namespace
