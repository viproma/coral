#include "gtest/gtest.h"

#include <chrono>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "zmq.hpp"

#include "dsb/net/messaging.hpp"
#include "dsb/net/util.hpp"

#include "dsb/net/socket.hpp"

using namespace dsb::net;


namespace
{
    void RequestReplyTest(ReqSocket& cli, RepSocket& svr)
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

TEST(dsb_net, ReqRepSocketDirect)
{
    ReqSocket cli;
    RepSocket svr;
    svr.Bind(dsb::net::Endpoint{"tcp://*:12345"});
    EXPECT_EQ("tcp://0.0.0.0:12345", svr.BoundEndpoint().URL());
    cli.Connect(dsb::net::Endpoint{"tcp://localhost:12345"});
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    RequestReplyTest(cli, svr);
    RequestReplyTest(cli, svr);
    svr.Close();
    cli.Close();
    // ...and do it again
    svr.Bind(dsb::net::Endpoint{"tcp://*:12346"});
    EXPECT_EQ("tcp://0.0.0.0:12346", svr.BoundEndpoint().URL());
    cli.Connect(dsb::net::Endpoint{"tcp://localhost:12346"});
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    RequestReplyTest(cli, svr);
    RequestReplyTest(cli, svr);
}


TEST(dsb_net, ReqRepSocketDirectReverse)
{
    ReqSocket cli;
    RepSocket svr;
    cli.Bind(dsb::net::Endpoint{"tcp://*:12345"});
    svr.Connect(dsb::net::Endpoint{"tcp://localhost:12345"});
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    RequestReplyTest(cli, svr);
    RequestReplyTest(cli, svr);
    svr.Close();
    cli.Close();
    // ...and do it again
    cli.Bind(dsb::net::Endpoint{"tcp://*:12346"});
    svr.Connect(dsb::net::Endpoint{"tcp://localhost:12346"});
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    RequestReplyTest(cli, svr);
    RequestReplyTest(cli, svr);
}


TEST(dsb_net, ReqRepSocketOutOfOrder)
{
    ReqSocket cli;
    RepSocket svr;
    svr.Bind(dsb::net::Endpoint{"tcp://*:12345"});
    cli.Connect(dsb::net::Endpoint{"tcp://localhost:12345"});
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
