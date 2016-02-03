#include "gtest/gtest.h"

#include <chrono>
#include <string>
#include <thread>
#include <vector>
#include "zmq.hpp"

#include "dsb/comm/messaging.hpp"
#include "dsb/comm/util.hpp"

#include "dsb/comm/p2p.hpp"

using namespace dsb::comm;


TEST(dsb_comm, P2PProxy_bidirectional)
{
    const std::string client1Id = "client1";
    const std::string server1Id = "server1";
    const std::string client2Id = "client2";
    const std::string server2Id = "server2";
    const std::string body1 = "wohoo!";
    const std::string body2 = "yeeeah!";
    const std::string body3 = "eeer...";
    const std::string body4 = "yeee-haaw!";

    zmq::context_t ctx;
    auto req1 = zmq::socket_t(ctx, ZMQ_REQ);
    req1.setsockopt(ZMQ_IDENTITY, client1Id.c_str(), client1Id.size());
    auto rep1 = zmq::socket_t(ctx, ZMQ_DEALER);
    rep1.setsockopt(ZMQ_IDENTITY, server1Id.c_str(), server1Id.size());
    auto req2  = zmq::socket_t(ctx, ZMQ_REQ);
    req2.setsockopt(ZMQ_IDENTITY, client2Id.c_str(), client2Id.size());
    auto rep2 = zmq::socket_t(ctx, ZMQ_DEALER);
    rep2.setsockopt(ZMQ_IDENTITY, server2Id.c_str(), server2Id.size());

    std::uint16_t port = 0;
    auto proxy = SpawnTcpP2PProxy("*", port);
    ASSERT_GT(port, 0);

    const auto endpoint = "tcp://localhost:" + std::to_string(port);
    req1.connect(endpoint.c_str());
    rep1.connect(endpoint.c_str());
    req2.connect(endpoint.c_str());
    rep2.connect(endpoint.c_str());
    // Give ZeroMQ some time to establish the connections.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Send a request from client 1 to server 2
    std::vector<zmq::message_t> req1Msg;
    req1Msg.push_back(dsb::comm::ToFrame(server2Id));
    req1Msg.push_back(dsb::comm::ToFrame(""));
    req1Msg.push_back(dsb::comm::ToFrame(body1));
    dsb::comm::Send(req1, req1Msg);

    // Send a request from client 2 to server 1
    std::vector<zmq::message_t> req2Msg;
    req2Msg.push_back(dsb::comm::ToFrame(server1Id));
    req2Msg.push_back(dsb::comm::ToFrame(""));
    req2Msg.push_back(dsb::comm::ToFrame(body2));
    dsb::comm::Send(req2, req2Msg);

    // Server 1: Receive request from client 2
    std::vector<zmq::message_t> rep1Msg;
    dsb::comm::Receive(rep1, rep1Msg);
    ASSERT_EQ(4u, rep1Msg.size());
    EXPECT_TRUE(rep1Msg[0].size() == 0);
    EXPECT_EQ(client2Id, dsb::comm::ToString(rep1Msg[1]));
    EXPECT_TRUE(rep1Msg[2].size() == 0);
    EXPECT_EQ(body2, dsb::comm::ToString(rep1Msg[3]));
    // Send reply from server 1 to client 2
    rep1Msg[3] = dsb::comm::ToFrame(body3);
    dsb::comm::Send(rep1, rep1Msg);

    // Server 2: Receive request from client 1
    std::vector<zmq::message_t> rep2Msg;
    dsb::comm::Receive(rep2, rep2Msg);
    ASSERT_EQ(4u, rep2Msg.size());
    EXPECT_TRUE(rep2Msg[0].size() == 0);
    EXPECT_EQ(client1Id, dsb::comm::ToString(rep2Msg[1]));
    EXPECT_TRUE(rep2Msg[2].size() == 0);
    EXPECT_EQ(body1, dsb::comm::ToString(rep2Msg[3]));
    // Send reply from server 1 to client 2
    rep2Msg[3] = dsb::comm::ToFrame(body4);
    dsb::comm::Send(rep2, rep2Msg);

    // Client 1: Receive reply from server 2
    std::vector<zmq::message_t> recvRep2Msg;
    dsb::comm::Receive(req1, recvRep2Msg);
    ASSERT_EQ(3u, recvRep2Msg.size());
    EXPECT_EQ(server2Id, dsb::comm::ToString(recvRep2Msg[0]));
    EXPECT_TRUE(recvRep2Msg[1].size() == 0);
    EXPECT_EQ(body4, dsb::comm::ToString(recvRep2Msg[2]));

    // Client 2: Receive reply from server 1
    std::vector<zmq::message_t> recvRep1Msg;
    dsb::comm::Receive(req2, recvRep1Msg);
    ASSERT_EQ(3u, recvRep1Msg.size());
    EXPECT_EQ(server1Id, dsb::comm::ToString(recvRep1Msg[0]));
    EXPECT_TRUE(recvRep1Msg[1].size() == 0);
    EXPECT_EQ(body3, dsb::comm::ToString(recvRep1Msg[2]));

    proxy.Terminate();
}

TEST(dsb_comm, P2PProxy_timeout)
{
    auto proxy = BackgroundP2PProxy(
        zmq::socket_t(GlobalContext(), ZMQ_ROUTER),
        std::chrono::milliseconds(100));
    const auto then = std::chrono::steady_clock::now();
    proxy.Thread__().join();
    const auto shutdownTime = std::chrono::steady_clock::now() - then;
    EXPECT_GT(shutdownTime, std::chrono::milliseconds(80));
    EXPECT_LT(shutdownTime, std::chrono::milliseconds(120));
}

TEST(dsb_comm, P2PProxy_misc)
{
    auto proxy = BackgroundP2PProxy(
        zmq::socket_t(GlobalContext(), ZMQ_ROUTER),
        std::chrono::milliseconds(500));
    auto proxy2 = std::move(proxy); // move construction
    EXPECT_FALSE(proxy.Thread__().joinable());
    EXPECT_TRUE(proxy2.Thread__().joinable());
    proxy = std::move(proxy2); // move assignment
    EXPECT_FALSE(proxy2.Thread__().joinable());
    EXPECT_TRUE(proxy.Thread__().joinable());
    proxy.Detach();
    EXPECT_FALSE(proxy.Thread__().joinable());
}


TEST(dsb_comm, P2PEndpoint)
{
    P2PEndpoint e0;
    EXPECT_TRUE(e0.Endpoint().empty());
    EXPECT_FALSE(e0.IsBehindProxy());
    EXPECT_TRUE(e0.Identity().empty());

    auto e1 = P2PEndpoint("tcp://localhost");
    EXPECT_EQ("tcp://localhost", e1.Endpoint());
    EXPECT_FALSE(e1.IsBehindProxy());
    EXPECT_TRUE(e1.Identity().empty());

    auto e2 = P2PEndpoint("ipc://myproxy$myid");
    EXPECT_EQ("ipc://myproxy", e2.Endpoint());
    EXPECT_TRUE(e2.IsBehindProxy());
    EXPECT_EQ("myid", e2.Identity());

    auto e3 = P2PEndpoint("inproc://foo", "bar");
    EXPECT_EQ("inproc://foo", e3.Endpoint());
    EXPECT_TRUE(e3.IsBehindProxy());
    EXPECT_EQ("bar", e3.Identity());
}


namespace
{
    void RequestReplyTest(P2PReqSocket& cli, P2PRepSocket& svr)
    {
        std::vector<zmq::message_t> m;
        m.push_back(zmq::message_t(5));
        m.push_back(zmq::message_t(5));
        std::memcpy(m[0].data(), "hello", 5);
        std::memcpy(m[1].data(), "world", 5);
        cli.Send(m);
        EXPECT_TRUE(m.empty());

        svr.Receive(m);
        EXPECT_EQ(2U, m.size());
        EXPECT_EQ(5U, m[0].size());
        EXPECT_EQ(5U, m[1].size());
        EXPECT_EQ(0, std::memcmp(m[0].data(), "hello", 5));
        EXPECT_EQ(0, std::memcmp(m[1].data(), "world", 5));

        std::memcpy(m[0].data(), "hallo", 5);
        std::memcpy(m[1].data(), "verda", 5);
        svr.Send(m);
        EXPECT_TRUE(m.empty());

        cli.Receive(m);
        EXPECT_EQ(2U, m.size());
        EXPECT_EQ(5U, m[0].size());
        EXPECT_EQ(5U, m[1].size());
        EXPECT_EQ(0, std::memcmp(m[0].data(), "hallo", 5));
        EXPECT_EQ(0, std::memcmp(m[1].data(), "verda", 5));
    }
}

TEST(dsb_comm, P2PReqRepSocketDirect)
{
    P2PReqSocket cli;
    P2PRepSocket svr;
    svr.Bind(P2PEndpoint("tcp://*:12345"));
    cli.Connect(P2PEndpoint("tcp://localhost:12345"));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    RequestReplyTest(cli, svr);
    RequestReplyTest(cli, svr);
    svr.Close();
    cli.Close();
    // ...and do it again
    svr.Bind(P2PEndpoint("tcp://*:12346"));
    cli.Connect(P2PEndpoint("tcp://localhost:12346"));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    RequestReplyTest(cli, svr);
    RequestReplyTest(cli, svr);
}


TEST(dsb_comm, P2PReqRepSocketDirectReverse)
{
    P2PReqSocket cli;
    P2PRepSocket svr;
    cli.Bind("tcp://*:12345");
    svr.Connect("tcp://localhost:12345");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    RequestReplyTest(cli, svr);
    RequestReplyTest(cli, svr);
    svr.Close();
    cli.Close();
    // ...and do it again
    cli.Bind("tcp://*:12346");
    svr.Connect("tcp://localhost:12346");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    RequestReplyTest(cli, svr);
    RequestReplyTest(cli, svr);
}


TEST(dsb_comm, P2PReqRepSocketProxied)
{
    P2PReqSocket cli;
    P2PRepSocket svr;
    std::uint16_t port = 0;
    auto prx = SpawnTcpP2PProxy("*", port);
    const auto endpoint = "tcp://localhost:" + std::to_string(port);
    svr.Bind(P2PEndpoint(endpoint, "bobby"));
    cli.Connect(P2PEndpoint(endpoint, "bobby"));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    RequestReplyTest(cli, svr);
    RequestReplyTest(cli, svr);
    svr.Close();
    cli.Close();
    // ...and do it again (but with a new identity, since the ROUTER socket
    // may not have discovered that 'bobby' has disconnected yet)
    svr.Bind(P2PEndpoint(endpoint, "johnny"));
    cli.Connect(P2PEndpoint(endpoint, "johnny"));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    RequestReplyTest(cli, svr);
    RequestReplyTest(cli, svr);
}


TEST(dsb_comm, P2PReqRepSocketOutOfOrder)
{
    P2PReqSocket cli;
    P2PRepSocket svr;
    svr.Bind(P2PEndpoint("tcp://*:12345"));
    cli.Connect(P2PEndpoint("tcp://localhost:12345"));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::vector<zmq::message_t> m;
    m.push_back(zmq::message_t(5));
    m.push_back(zmq::message_t(5));
    std::memcpy(m[0].data(), "hello", 5);
    std::memcpy(m[1].data(), "world", 5);
    cli.Send(m);
    EXPECT_TRUE(m.empty());
    m.push_back(zmq::message_t(12));
    std::memcpy(m[0].data(), "out of order", 12);
    cli.Send(m);

    svr.Receive(m);
    EXPECT_EQ(2U, m.size());
    EXPECT_EQ(5U, m[0].size());
    EXPECT_EQ(5U, m[1].size());
    EXPECT_EQ(0, std::memcmp(m[0].data(), "hello", 5));
    EXPECT_EQ(0, std::memcmp(m[1].data(), "world", 5));

    std::memcpy(m[0].data(), "hallo", 5);
    std::memcpy(m[1].data(), "verda", 5);
    svr.Send(m);
    EXPECT_TRUE(m.empty());

    cli.Receive(m);
    EXPECT_EQ(2U, m.size());
    EXPECT_EQ(5U, m[0].size());
    EXPECT_EQ(5U, m[1].size());
    EXPECT_EQ(0, std::memcmp(m[0].data(), "hallo", 5));
    EXPECT_EQ(0, std::memcmp(m[1].data(), "verda", 5));

    svr.Receive(m);
    EXPECT_EQ(1U, m.size());
    EXPECT_EQ(12U, m[0].size());
    EXPECT_EQ(0, std::memcmp(m[0].data(), "out of order", 5));
}
