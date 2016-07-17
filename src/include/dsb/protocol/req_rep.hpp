/**
\file
\brief  Classes that implement a generic request-reply meta-protocol.
*/
#ifndef DSB_PROTOCOL_REQ_REP_HPP
#define DSB_PROTOCOL_REQ_REP_HPP

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unordered_map>

#include "zmq.hpp"

#include "dsb/config.h"
#include "dsb/comm/p2p.hpp"
#include "dsb/comm/reactor.hpp"


namespace dsb
{
namespace protocol
{


/**
\brief  A backend class for clients that communicate with an RRServer.

This class represents the client side of the generic request-reply protocol.
An instance of this class may only connect to one server at a time.

\see RRServer
    For the server-side class and more information about the protocol.
*/
class RRClient
{
public:
    /**
    \brief  Constructs a new client instance connected to the given endpoint,
            and registers it with the given reactor to be notified of replies
            to the requests that are sent.

    `protocolIdentifier` must contain the identifier for the protocol with
    which requests will be made.
    */
    RRClient(
        dsb::comm::Reactor& reactor,
        const std::string& protocolIdentifier,
        const dsb::comm::P2PEndpoint& serverEndpoint);

    // This class is not copyable or movable because it leaks its `this`
    // pointer to the reactor.
    RRClient(const RRClient&) = delete;
    RRClient(RRClient&&) = delete;
    RRClient& operator=(const RRClient&) = delete;
    RRClient& operator=(RRClient&&) = delete;

    ~RRClient() DSB_NOEXCEPT;

    /**
    \brief  A callback type for Request().

    If `ec` contains an error code, the contents of the other arguments are
    unspecified.

    \param [in] ec
        If the request failed, this holds an error code.  This will typically
        be std::errc::timed_out, signifying that the server didn't respond in
        time.
    \param [in] replyHeader
        A pointer to the start of a memory buffer which holds the reply
        header, the length of which is given by `replyHeaderSize`.
        This is never null, but may be empty (i.e., zero length).
    \param [in] replyHeaderSize
        The length (in bytes) of the reply header.
    \param [in] replyBody
        A pointer to the start of a memory buffer which holds the reply
        body, the length of which is given by `replyBodySize`.
        Iff the reply has no body, this will be null.
    \param [in] replyBodySize
        The length (in bytes) of the reply body if `replyBody` is not
        null, otherwise unspecified.
    */
    typedef std::function<void(
            const std::error_code& ec,
            const char* replyHeader, size_t replyHeaderSize,
            const char* replyBody, size_t replyBodySize)>
        ReplyHandler;

    /**
    \brief  Sends a request.

    This sends a request using the protocol whose identifier was given in the
    class constructor and whose version is given by `protocolVersion`.  The
    RRServer on the other end must have a protocol handler associated with the
    same identifier and version.

    \param [in] protocolVersion
        The version number of the protocol with which the request is made.
    \param [in] requestHeader
        A pointer to the start of a memory buffer which holds the request
        header, the length of which is given by `requestHeaderSize`.
        This cannot be null, but may be empty (i.e., zero length).
    \param [in] requestHeaderSize
        The length (in bytes) of the request header.
    \param [in] requestBody
        A pointer to the start of a memory buffer which holds the request
        body, the length of which is given by `requestBodySize`.
        If the request has no body, this may be null.
    \param [in] requestBodySize
        The length (in bytes) of the request body if `requestBody` is not
        null, otherwise this is ignored.
    \param [in] timeout
        How long to wait to be able to send the request before throwing an
        exception, or, if the actual sending was successful, how long to wait
        for a reply before calling `onComplete` with an error code.
    \param [in] onComplete
        A callback that will be called when the server responds or the request
        times out.  This function is guaranteed to be called unless Request()
        throws an exception or the RRClient is destroyed before a reply can be
        received.

    \throws std::runtime_error
        If the timeout is reached before the message could even be sent
        (typically because the connection to the server has failed or has yet
        to be established).
    \throws zmq::error_t
        On communications error.
    */
    void Request(
        std::uint16_t protocolVersion,
        const char* requestHeader, size_t requestHeaderSize,
        const char* requestBody, size_t requestBodySize,
        std::chrono::milliseconds timeout,
        ReplyHandler onComplete);

    /**
    \brief  A callback type for RequestMaxProtocol().

    If `ec` contains an error code, the contents of `protocolVersion` are
    unspecified.

    \param [in] ec
        If the request failed, this holds an error code.  This will typically
        be std::errc::timed_out, signifying that the server didn't respond in
        time, or std::errc::protocol_not_supported, which means that the server
        does not support any version of the protocol.
    \param [in] protocolVersion
        The maximum version of the protocol that the server supports.
    */
    typedef std::function<void(
            const std::error_code& ec,
            std::uint16_t version)>
        MaxProtocolReplyHandler;

    /**
    \brief  Sends a "meta request" to the server asking for the maximum
            protocol version it supports.

    The server will respond with the greatest version number that has been
    added with RRServer::AddProtocolHandler().

    \param [in] timeout
        How long to wait to be able to send the request before throwing an
        exception, or, if the actual sending was successful, how long to wait
        for a reply before calling `onComplete` with an error code.
    \param [in] onComplete
        A callback that will be called when the server responds or the request
        times out.  This function is guaranteed to be called unless
        RequestMaxProtocol() throws an exception or the RRClient is destroyed
        before a reply can be received.

    \throws std::runtime_error
        If the timeout is reached before the message could even be sent
        (typically because the connection to the server has failed or has yet
        to be established).
    \throws zmq::error_t
        On communications error.
    */
    void RequestMaxProtocol(
        std::chrono::milliseconds timeout,
        MaxProtocolReplyHandler onComplete);

private:
    void SendRequest(
        const std::string& protocolIdentifier, std::uint16_t protocolVersion,
        const char* requestHeader, size_t requestHeaderSize,
        const char* requestBody, size_t requestBodySize,
        std::chrono::milliseconds timeout);

    void ReceiveReply();

    void CompleteWithError(const std::error_code& ec);

    void SetTimer(std::chrono::milliseconds timeout);

    void CancelTimer();

    dsb::comm::Reactor& m_reactor;
    std::string m_protocolIdentifier;
    dsb::comm::P2PEndpoint m_serverEndpoint;
    dsb::comm::P2PReqSocket m_socket;

    int m_timeoutTimerID;
    std::uint16_t m_requestProtocolVersion;
    ReplyHandler m_onComplete;
    MaxProtocolReplyHandler m_onMaxProtocolComplete;
};


/**
\brief  An interface for classes that implement the server side of
        request-reply protocols, to be used with RRServer.

Objects of this type may be added to an RRServer and associated with
a particular protocol and a specific version of that protocol.  Whenever
an RRServer receives an incoming request, it routes the request to the
corresponding handler.

One implementation may handle multiple protocols and/or multiple versions
of the same protocol, by calling RRServer::AddProtocolHandler() several
times with the same object.
*/
class RRServerProtocolHandler
{
public:
    /**
    \brief  Handles an incoming request and generates a reply.

    Each request/reply consists of a mandatory header and an optional body,
    the contents and format of which are left to the implementor.  If the
    request is invalid, or for some other reason should be ignored, the
    function may return `false`, in which case the server will not send a
    reply.

    \param [in] protocolIdentifier
        The identifier for the protocol with which the request was made.
    \param [in] protocolVersion
        The version number of the protocol with which the request was made.
    \param [in] requestHeader
        A pointer to the start of a memory buffer which holds the request
        header, the length of which is given by `requestHeaderSize`.
        This is never null, but may be empty (i.e., zero length).
    \param [in] requestHeaderSize
        The length (in bytes) of the request header.
    \param [in] requestBody
        A pointer to the start of a memory buffer which holds the request
        body, the length of which is given by `requestBodySize`.
        Iff the request has no body, this will be null.
    \param [in] requestBodySize
        The length (in bytes) of the request body if `requestBody` is not
        null, otherwise unspecified.
    \param [out] replyHeader
        On successful return, this must contain a pointer to a memory buffer
        which holds the reply header, whose length is given by
        `replyHeaderSize`.  The pointer may not be null in this case, even
        if the header is empty. The memory buffer and its contents must
        remain valid until the next call to HandleRequest() or until the
        object is destroyed, whichever comes first.
        If the function returns `false`, this parameter is ignored.
    \param [out] replyHeaderSize
        On successful return, this must contain the length of the reply header.
        If the function returns `false`, this parameter is ignored.
    \param [out] replyBody
        On successful return, this may be set to a pointer to a memory buffer
        which holds the reply header, whose length is given by
        `replyBodySize`.  Setting it to null signifies that the reply has no
        body.  If non-null, the memory buffer and its contents must
        remain valid until the next call to HandleRequest() or until the
        object is destroyed, whichever comes first.
        If the function returns `false`, this parameter is ignored.
    \param [out] replyBodySize
        If `replyBody` is not null, this must contain the length of the reply
        header on successful return.
        If the function returns `false`, this parameter is ignored.

    \returns
        Whether the request was successfully handled and a reply should be sent.
    */
    virtual bool HandleRequest(
        const std::string& protocolIdentifier,
        std::uint16_t protocolVersion,
        const char* requestHeader, size_t requestHeaderSize,
        const char* requestBody, size_t requestBodySize,
        const char*& replyHeader, size_t& replyHeaderSize,
        const char*& replyBody, size_t& replyBodySize) = 0;

    virtual ~RRServerProtocolHandler() = default;
};


/**
\brief  A generic server class for simple request-reply protocols.

This class receives request messages consisting of 2 or 3 frames.  The first
frame contains the protocol identifier and version.  This is used by the
RRServer to select the appropriate protocol handler, implemented as a
RRServerProtocolHandler.  The second frame is called the message header, while
the (optional) third frame is the message body.  These are simply forwarded
to the protocol handler, whose job it is to formulate a reply message.
The reply message has the same format.
The RRServer class takes care of the actual receiving and sending of messages,
so the protocol handler only needs to deal with the actual header and body
content, and not the details of the network communication.

Multiple clients may connect and send requests to one server.

\see RRClient
    The corresponding client side class.
*/
class RRServer
{
public:
    /**
    \brief  Constructs a new server instance bound to the given endpoint,
            and registers it with the given reactor to wait for incoming
            requests.
    */
    RRServer(
        dsb::comm::Reactor& reactor,
        const dsb::comm::P2PEndpoint& endpoint);

    ~RRServer() DSB_NOEXCEPT;

    RRServer(const RRServer&) = delete;
    RRServer& operator=(const RRServer&) = delete;

    RRServer(RRServer&&) DSB_NOEXCEPT;
    RRServer& operator=(RRServer&&) DSB_NOEXCEPT;

    /**
    \brief  Adds a protocol handler for the protocol with the given identifier
            and version.

    If a protocol handler supports multiple protocols and/or versions, it may
    be added more than once.
    */
    void AddProtocolHandler(
        const std::string& protocolIdentifier,
        std::uint16_t protocolVersion,
        std::shared_ptr<RRServerProtocolHandler> handler);

    /**
    \brief  Returns the endpoint to which the server is bound.

    This is generally the one that was specified in the constructor, unless
    the server is bound to a local endpoint (not a proxy), in which case
    there are two special cases:

      - If the address was specified as `*` (i.e., bind on all interfaces),
        then the returned address will be `0.0.0.0`.
      - If the port was specified as `*` (i.e., ask the OS for an available
        emphemeral port), then the actual port will be returned.
    */
    dsb::comm::P2PEndpoint BoundEndpoint() const;

private:
    class Private;
    std::unique_ptr<Private> m_private;
};


}} // namespace
#endif
