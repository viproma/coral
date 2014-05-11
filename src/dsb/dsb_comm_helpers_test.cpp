#include <cstring>
#include "gtest/gtest.h"
#include "dsb/comm/helpers.hpp"

using namespace dsb::comm;


TEST(dsb_comm_helpers, SendReceiveMessage)
{
    auto ctx = zmq::context_t();
    auto sender = zmq::socket_t(ctx, ZMQ_PUSH);
    auto recver = zmq::socket_t(ctx, ZMQ_PULL);
    const auto endpoint = std::string("ipc://")
        + ::testing::UnitTest::GetInstance()->current_test_info()->test_case_name();
    recver.bind(endpoint.c_str());
    sender.connect(endpoint.c_str());
    
    std::deque<zmq::message_t> srcMsg;
    srcMsg.emplace_back(123);
    srcMsg.emplace_back();
    srcMsg.emplace_back(321);
    Send(sender, srcMsg);

    std::deque<zmq::message_t> tgtMsg(1);
    Receive(recver, tgtMsg);
    ASSERT_EQ(3, tgtMsg.size());
    EXPECT_EQ(123, tgtMsg[0].size());
    EXPECT_EQ(  0, tgtMsg[1].size());
    EXPECT_EQ(321, tgtMsg[2].size());
}

TEST(dsb_comm_helpers, SendReceiveAddressedMessage)
{
    auto ctx = zmq::context_t();
    auto sender = zmq::socket_t(ctx, ZMQ_PUSH);
    auto recver = zmq::socket_t(ctx, ZMQ_PULL);
    const auto endpoint = std::string("ipc://")
        + ::testing::UnitTest::GetInstance()->current_test_info()->test_case_name();
    recver.bind(endpoint.c_str());
    sender.connect(endpoint.c_str());
    
    std::deque<zmq::message_t> srcMsg;
    srcMsg.emplace_back(123);
    srcMsg.emplace_back(321);
    AddressedSend(sender, "foo", srcMsg);

    std::deque<zmq::message_t> tgtMsg(1);
    Receive(recver, tgtMsg);
    ASSERT_EQ(4, tgtMsg.size());
    ASSERT_EQ(3, tgtMsg[0].size());
    EXPECT_EQ(0, std::memcmp(tgtMsg[0].data(), "foo", 3));
    EXPECT_EQ(  0, tgtMsg[1].size());
    EXPECT_EQ(123, tgtMsg[2].size());
    EXPECT_EQ(321, tgtMsg[3].size());
}

TEST(dsb_comm_helpers, PopMessageEnvelope)
{
    std::deque<zmq::message_t> msg;
    msg.emplace_back(123);
    msg.emplace_back(321);
    msg.emplace_back();
    msg.emplace_back(97);
    std::deque<zmq::message_t> env;
    env.emplace_back();
    const auto size = PopMessageEnvelope(msg, &env);
    EXPECT_EQ(3, size);
    ASSERT_EQ(2, env.size());
    EXPECT_EQ(123, env[0].size());
    EXPECT_EQ(321, env[1].size());
    ASSERT_EQ(1, msg.size());
    EXPECT_EQ(97, msg[0].size());
}

TEST(dsb_comm_helpers, PopMessageEnvelope_emptyEnvelope)
{
    std::deque<zmq::message_t> msg;
    msg.emplace_back();
    msg.emplace_back(123);
    msg.emplace_back(321);
    msg.emplace_back(97);
    std::deque<zmq::message_t> env;
    env.emplace_back();
    const auto size = PopMessageEnvelope(msg, &env);
    EXPECT_EQ(1, size);
    EXPECT_EQ(0, env.size());
    ASSERT_EQ(3, msg.size());
    EXPECT_EQ(123, msg[0].size());
    EXPECT_EQ(321, msg[1].size());
    EXPECT_EQ( 97, msg[2].size());
}

TEST(dsb_comm_helpers, PopMessageEnvelope_noEnvelope)
{
    std::deque<zmq::message_t> msg;
    msg.emplace_back(123);
    msg.emplace_back(321);
    msg.emplace_back(97);
    std::deque<zmq::message_t> env;
    env.emplace_back();
    const auto size = PopMessageEnvelope(msg, &env);
    EXPECT_EQ(0, size);
    EXPECT_EQ(0, env.size());
    ASSERT_EQ(3, msg.size());
    EXPECT_EQ(123, msg[0].size());
    EXPECT_EQ(321, msg[1].size());
    EXPECT_EQ( 97, msg[2].size());
}

TEST(dsb_comm_helpers, PopMessageEnvelope_dropEnvelope)
{
    std::deque<zmq::message_t> msg;
    msg.emplace_back(123);
    msg.emplace_back(321);
    msg.emplace_back();
    msg.emplace_back(97);
    const auto size = PopMessageEnvelope(msg);
    EXPECT_EQ(3, size);
    ASSERT_EQ(1, msg.size());
    EXPECT_EQ(97, msg[0].size());
}


TEST(dsb_comm_helpers, ToString)
{
    zmq::message_t msg(3);
    std::memcpy(msg.data(), "foo", 3);
    EXPECT_EQ("foo", ToString(msg));
}
