#include <memory>
#include <thread>
#include "gtest/gtest.h"
#include "dsb/protocol/req_rep.hpp"
#include "dsb/util.hpp"


namespace dp = dsb::protocol;

namespace
{
    const std::string MY_PROTOCOL_ID = "CHEEZBURGER";
    const std::uint16_t MY_PROTOCOL_VER = 0;

    class MyProtocolHandler : public dp::RRServerProtocolHandler
    {
    public:
        MyProtocolHandler(dsb::comm::Reactor& reactor)
            : m_reactor{reactor}
        { }

        bool HandleRequest(
            const std::string& protocolIdentifier,
            std::uint16_t protocolVersion,
            const char* requestHeader, size_t requestHeaderSize,
            const char* requestBody, size_t requestBodySize,
            const char*& replyHeader, size_t& replyHeaderSize,
            const char*& replyBody, size_t& replyBodySize) override
        {
            if (protocolIdentifier != MY_PROTOCOL_ID) return false;
            if (protocolVersion != MY_PROTOCOL_VER) return false;
            const auto msg = std::string{requestHeader, requestHeaderSize};
            if (msg == "PING") {
                replyHeader = "PONG";
                replyHeaderSize = 4;
            } else if (msg == "HELLO" && requestBody != nullptr) {
                m_replyBody = std::string{requestBody, requestBodySize};
                replyHeader = "OHAI";
                replyHeaderSize = 4;
                replyBody = m_replyBody.data();
                replyBodySize = m_replyBody.size();
            } else if (msg == "KTHXBAI") {
                m_reactor.Stop();
                replyHeader = "HUGZ";
                replyHeaderSize = 4;
            } else {
                return false;
            }
            return true;
        }

    private:
        dsb::comm::Reactor& m_reactor;
        std::string m_replyBody;
    };

    void RunTestServer(const char* endpoint)
    {
        dsb::comm::Reactor reactor;
        dp::RRServer server{reactor, dsb::comm::P2PEndpoint{endpoint}};
        server.AddProtocolHandler(
            MY_PROTOCOL_ID,
            MY_PROTOCOL_VER,
            std::make_shared<MyProtocolHandler>(reactor));
        reactor.Run();
    }
}

TEST(dsb_protocol, ReqRep)
{
    const char* const endpoint = "inproc://dsb_protocol_req_rep_test";
    auto serverThread = std::thread{&RunTestServer, endpoint};
    auto joinServerThread = dsb::util::OnScopeExit([&] () {
        serverThread.join();
    });

    dsb::comm::Reactor reactor;
    dp::RRClient client{
        reactor,
        MY_PROTOCOL_ID,
        dsb::comm::P2PEndpoint{endpoint}};

    const auto timeout = std::chrono::milliseconds(100);
    std::function<void()> runTest1;
    std::function<void()> runTest2;
    std::function<void()> runTest3;
    std::function<void()> runErrorTest1;
    std::function<void()> runErrorTest2;
    std::function<void()> runShutdownTest;
    dp::RRClient::MaxProtocolReplyHandler onTest1Reply;
    dp::RRClient::ReplyHandler onTest2Reply;
    dp::RRClient::ReplyHandler onTest3Reply;
    dp::RRClient::ReplyHandler onErrorTest1Reply;
    dp::RRClient::ReplyHandler onErrorTest2Reply;
    dp::RRClient::ReplyHandler onShutdownTestReply;

    runTest1 = [&] ()
    {
        client.RequestMaxProtocol(timeout, onTest1Reply);
    };
    onTest1Reply = [&] (const std::error_code& ec, std::uint16_t v)
    {
        EXPECT_TRUE(!ec);
        EXPECT_EQ(MY_PROTOCOL_VER, v);
        runTest2();
    };

    runTest2 = [&] ()
    {
        client.Request(
            MY_PROTOCOL_VER,
            "PING", 4u,
            nullptr, 0u,
            timeout, onTest2Reply);
    };
    onTest2Reply = [&] (
        const std::error_code& ec,
        const char* replyHeader, size_t replyHeaderSize,
        const char* replyBody, size_t replyBodySize)
    {
        EXPECT_TRUE(!ec);
        EXPECT_TRUE(replyHeader && 0 == std::memcmp(replyHeader, "PONG", replyHeaderSize));
        EXPECT_EQ(nullptr, replyBody);
        runTest3();
    };

    runTest3 = [&] () {
        client.Request(
            MY_PROTOCOL_VER,
            "HELLO", 5u,
            "World!", 6u,
            timeout, onTest3Reply);
    };
    onTest3Reply = [&] (
        const std::error_code& ec,
        const char* replyHeader, size_t replyHeaderSize,
        const char* replyBody, size_t replyBodySize)
    {
        EXPECT_TRUE(!ec);
        EXPECT_TRUE(replyHeader && 0 == std::memcmp(replyHeader, "OHAI", replyHeaderSize));
        EXPECT_TRUE(replyBody   && 0 == std::memcmp(replyBody, "World!", replyBodySize));
        runErrorTest1();
    };

    runErrorTest1 = [&] () {
        client.Request(
            2u, // unsupported protocol version
            "PING", 4u,
            nullptr, 0u,
            timeout, onErrorTest1Reply);
    };
    onErrorTest1Reply = [&] (
        const std::error_code& ec,
        const char* replyHeader, size_t replyHeaderSize,
        const char* replyBody, size_t replyBodySize)
    {
        EXPECT_TRUE(ec == std::errc::timed_out);
        runErrorTest2();
    };

    runErrorTest2 = [&] () {
        client.Request(
            1u,
            "DING", 4u, // invalid message
            nullptr, 0u,
            timeout, onErrorTest2Reply);
    };
    onErrorTest2Reply = [&] (
        const std::error_code& ec,
        const char* replyHeader, size_t replyHeaderSize,
        const char* replyBody, size_t replyBodySize)
    {
        EXPECT_TRUE(ec == std::errc::timed_out);
        runShutdownTest();
    };

    runShutdownTest = [&] () {
        client.Request(
            MY_PROTOCOL_VER,
            "KTHXBAI", 7u,
            nullptr, 0u,
            timeout, onShutdownTestReply);
    };
    onShutdownTestReply = [&] (
        const std::error_code& ec,
        const char* replyHeader, size_t replyHeaderSize,
        const char* replyBody, size_t replyBodySize)
    {
        EXPECT_TRUE(!ec);
        ASSERT_NE(nullptr, replyHeader);
        EXPECT_EQ(0, std::memcmp(replyHeader, "HUGZ", replyHeaderSize));
        EXPECT_EQ(nullptr, replyBody);
        reactor.Stop();
    };

    runTest1();
    reactor.Run();
}


TEST(dsb_protocol, ReqRepMoreErrors)
{
    const char* const endpoint = "inproc://dsb_protocol_req_rep_test2";
    auto serverThread = std::thread{&RunTestServer, endpoint};
    auto joinServerThread = dsb::util::OnScopeExit([&] () {
        serverThread.join();
    });

    const auto timeout = std::chrono::milliseconds(200);
    std::function<void()> runTest1;
    std::function<void()> runTest2;
    std::function<void()> runTest3;
    std::function<void()> runTest4;
    dp::RRClient::MaxProtocolReplyHandler onTest1Reply;
    dp::RRClient::ReplyHandler onTest2Reply;
    dp::RRClient::ReplyHandler onTest3Reply;

    dsb::comm::Reactor reactor;
    auto client = std::make_unique<dp::RRClient>(
        reactor,
        "SOME_UNKNOWN_PROTOCOL",
        dsb::comm::P2PEndpoint{endpoint});

    runTest1 = [&] ()
    {
        // Request max version for an unknown protocol.
        client->RequestMaxProtocol(timeout, onTest1Reply);
    };
    onTest1Reply = [&] (const std::error_code& ec, std::uint16_t v)
    {
        EXPECT_TRUE(ec == std::errc::protocol_not_supported);
        runTest2();
    };

    runTest2 = [&] ()
    {
        // Send a normal request using an unknown protocol.
        client->Request(
            MY_PROTOCOL_VER,
            "PING", 4u,
            nullptr, 0u,
            timeout, onTest2Reply);
    };
    onTest2Reply = [&] (
        const std::error_code& ec,
        const char* replyHeader, size_t replyHeaderSize,
        const char* replyBody, size_t replyBodySize)
    {
        EXPECT_TRUE(ec == std::errc::timed_out);
        runTest3();
    };

    runTest3 = [&] () {
        // Switch to a valid protocol...
        client = std::make_unique<dp::RRClient>(
            reactor,
            MY_PROTOCOL_ID,
            dsb::comm::P2PEndpoint{endpoint});
        // Shut the server down in preparation for the next test.
        client->Request(
            MY_PROTOCOL_VER,
            "KTHXBAI", 7u,
            nullptr, 0u,
            timeout, onTest3Reply);
    };
    onTest3Reply = [&] (
        const std::error_code& ec,
        const char* replyHeader, size_t replyHeaderSize,
        const char* replyBody, size_t replyBodySize)
    {
        EXPECT_TRUE(!ec);
        EXPECT_TRUE(replyHeader && 0 == std::memcmp(replyHeader, "HUGZ", replyHeaderSize));
        EXPECT_EQ(nullptr, replyBody);
        runTest4();
    };

    runTest4 = [&] () {
        // Try to send a request after the server has shut down (but first,
        // give ZMQ some time to notice that the server has in fact shut down).
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        EXPECT_THROW(client->Request(
            MY_PROTOCOL_VER,
            "PING", 4u,
            nullptr, 0u,
            timeout, dp::RRClient::ReplyHandler{}), std::runtime_error);
        reactor.Stop();
    };

    runTest1();
    reactor.Run();
}
