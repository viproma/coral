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
