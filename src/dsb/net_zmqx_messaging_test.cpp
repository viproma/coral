#include <chrono>
#include <cstring>
#include "gtest/gtest.h"
#include "dsb/net/zmqx.hpp"

using namespace dsb::net::zmqx;


TEST(dsb_net, WaitForIncomingOutgoing)
{
    auto ctx = zmq::context_t();
    auto sender = zmq::socket_t(ctx, ZMQ_PUSH);
    const auto tOut0 = std::chrono::steady_clock::now();
    EXPECT_FALSE(WaitForOutgoing(sender, std::chrono::milliseconds(200)));
    const auto tOut1 = std::chrono::steady_clock::now();
    EXPECT_LT(std::chrono::milliseconds(180), tOut1 - tOut0);
    EXPECT_GT(std::chrono::milliseconds(220), tOut1 - tOut0);

    auto recver = zmq::socket_t(ctx, ZMQ_PULL);
    const auto tIn0 = std::chrono::steady_clock::now();
    EXPECT_FALSE(WaitForIncoming(recver, std::chrono::milliseconds(200)));
    const auto tIn1 = std::chrono::steady_clock::now();
    EXPECT_LT(std::chrono::milliseconds(180), tIn1 - tIn0);
    EXPECT_GT(std::chrono::milliseconds(220), tIn1 - tIn0);

    const auto endpoint = std::string("inproc://")
        + ::testing::UnitTest::GetInstance()->current_test_info()->test_case_name();
    recver.bind(endpoint.c_str());
    sender.connect(endpoint.c_str());
    EXPECT_TRUE(WaitForOutgoing(sender, std::chrono::milliseconds(200)));
    zmq::message_t msg{"foo", 3};
    sender.send(msg);
    EXPECT_TRUE(msg.size() == 0);
    EXPECT_TRUE(WaitForIncoming(recver, std::chrono::milliseconds(200)));
    recver.recv(&msg);
    EXPECT_EQ("foo", ToString(msg));
}

TEST(dsb_net, SendReceiveMessage)
{
    auto ctx = zmq::context_t();
    auto sender = zmq::socket_t(ctx, ZMQ_PUSH);
    auto recver = zmq::socket_t(ctx, ZMQ_PULL);
    const auto endpoint = std::string("inproc://")
        + ::testing::UnitTest::GetInstance()->current_test_info()->test_case_name();
    recver.bind(endpoint.c_str());
    sender.connect(endpoint.c_str());

    std::vector<zmq::message_t> srcMsg;
    srcMsg.push_back(zmq::message_t(123));
    srcMsg.push_back(zmq::message_t());
    Send(sender, srcMsg, SendFlag::more);
    EXPECT_TRUE(srcMsg.empty());
    srcMsg.push_back(zmq::message_t(321));
    Send(sender, srcMsg);

    std::vector<zmq::message_t> tgtMsg(1);
    Receive(recver, tgtMsg);
    ASSERT_EQ(  3U, tgtMsg.size());
    EXPECT_EQ(123U, tgtMsg[0].size());
    EXPECT_EQ(  0U, tgtMsg[1].size());
    EXPECT_EQ(321U, tgtMsg[2].size());
}

TEST(dsb_net, ToFrame_ToString)
{
    auto msg = ToFrame("foo");
    EXPECT_EQ("foo", ToString(msg));
}
