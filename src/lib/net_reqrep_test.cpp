#include <memory>
#include <thread>
#include "gtest/gtest.h"
#include "coral/net/reqrep.hpp"
#include "coral/util.hpp"


namespace dnr = coral::net::reqrep;

namespace
{
    const std::string MY_PROTOCOL_ID = "CHEEZBURGER";
    const std::uint16_t MY_PROTOCOL_VER = 0;

    class MyProtocolHandler : public dnr::ServerProtocolHandler
    {
    public:
        MyProtocolHandler(coral::net::Reactor& reactor)
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
        coral::net::Reactor& m_reactor;
        std::string m_replyBody;
    };

    void RunTestServer(const char* endpoint)
    {
        coral::net::Reactor reactor;
        dnr::Server server{reactor, coral::net::Endpoint{endpoint}};
        server.AddProtocolHandler(
            MY_PROTOCOL_ID,
            MY_PROTOCOL_VER,
            std::make_shared<MyProtocolHandler>(reactor));
        reactor.Run();
    }
}

TEST(coral_net_reqrep, ReqRep)
{
    const char* const endpoint = "inproc://coral_net_reqrep_req_rep_test";
    auto serverThread = std::thread{&RunTestServer, endpoint};
    auto joinServerThread = coral::util::OnScopeExit([&] () {
        serverThread.join();
    });

    coral::net::Reactor reactor;
    dnr::Client client{
        reactor,
        MY_PROTOCOL_ID,
        coral::net::Endpoint{endpoint}};

    const auto timeout = std::chrono::milliseconds(100);
    std::function<void()> runTest1;
    std::function<void()> runTest2;
    std::function<void()> runTest3;
    std::function<void()> runErrorTest1;
    std::function<void()> runErrorTest2;
    std::function<void()> runShutdownTest;
    dnr::Client::MaxProtocolReplyHandler onTest1Reply;
    dnr::Client::ReplyHandler onTest2Reply;
    dnr::Client::ReplyHandler onTest3Reply;
    dnr::Client::ReplyHandler onErrorTest1Reply;
    dnr::Client::ReplyHandler onErrorTest2Reply;
    dnr::Client::ReplyHandler onShutdownTestReply;

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


TEST(coral_net_reqrep, ReqRepMoreErrors)
{
    const char* const endpoint = "inproc://coral_net_reqrep_req_rep_test2";
    auto serverThread = std::thread{&RunTestServer, endpoint};
    auto joinServerThread = coral::util::OnScopeExit([&] () {
        serverThread.join();
    });

    const auto timeout = std::chrono::milliseconds(200);
    std::function<void()> runTest1;
    std::function<void()> runTest2;
    std::function<void()> runTest3;
    std::function<void()> runTest4;
    dnr::Client::MaxProtocolReplyHandler onTest1Reply;
    dnr::Client::ReplyHandler onTest2Reply;
    dnr::Client::ReplyHandler onTest3Reply;

    coral::net::Reactor reactor;
    auto client = std::make_unique<dnr::Client>(
        reactor,
        "SOME_UNKNOWN_PROTOCOL",
        coral::net::Endpoint{endpoint});

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
        client = std::make_unique<dnr::Client>(
            reactor,
            MY_PROTOCOL_ID,
            coral::net::Endpoint{endpoint});
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
            timeout, dnr::Client::ReplyHandler{}), std::runtime_error);
        reactor.Stop();
    };

    runTest1();
    reactor.Run();
}
