#include <cstring>
#include "gtest/gtest.h"
#include "dsb/comm.hpp"

using namespace dsb::comm;


TEST(dsb_comm, SendReceiveMessage)
{
    auto ctx = zmq::context_t();
    auto sender = zmq::socket_t(ctx, ZMQ_PUSH);
    auto recver = zmq::socket_t(ctx, ZMQ_PULL);
    const auto endpoint = std::string("inproc://")
        + ::testing::UnitTest::GetInstance()->current_test_info()->test_case_name();
    recver.bind(endpoint.c_str());
    sender.connect(endpoint.c_str());

    std::deque<zmq::message_t> srcMsg;
    srcMsg.push_back(zmq::message_t(123));
    srcMsg.push_back(zmq::message_t());
    srcMsg.push_back(zmq::message_t(321));
    Send(sender, srcMsg);

    std::deque<zmq::message_t> tgtMsg(1);
    Receive(recver, tgtMsg);
    ASSERT_EQ(3, tgtMsg.size());
    EXPECT_EQ(123, tgtMsg[0].size());
    EXPECT_EQ(  0, tgtMsg[1].size());
    EXPECT_EQ(321, tgtMsg[2].size());
}

TEST(dsb_comm, SendReceiveAddressedMessage)
{
    auto ctx = zmq::context_t();
    auto sender = zmq::socket_t(ctx, ZMQ_PUSH);
    auto recver = zmq::socket_t(ctx, ZMQ_PULL);
    const auto endpoint = std::string("inproc://")
        + ::testing::UnitTest::GetInstance()->current_test_info()->test_case_name();
    recver.bind(endpoint.c_str());
    sender.connect(endpoint.c_str());

    std::deque<zmq::message_t> env;
    env.push_back(zmq::message_t(3));
    std::memcpy(env.back().data(), "foo", 3);
    std::deque<zmq::message_t> srcMsg;
    srcMsg.push_back(zmq::message_t(123));
    srcMsg.push_back(zmq::message_t(321));
    AddressedSend(sender, env, srcMsg);

    std::deque<zmq::message_t> tgtMsg(1);
    Receive(recver, tgtMsg);
    ASSERT_EQ(4, tgtMsg.size());
    ASSERT_EQ(3, tgtMsg[0].size());
    EXPECT_EQ(0, std::memcmp(tgtMsg[0].data(), "foo", 3));
    EXPECT_EQ(  0, tgtMsg[1].size());
    EXPECT_EQ(123, tgtMsg[2].size());
    EXPECT_EQ(321, tgtMsg[3].size());
}

TEST(dsb_comm, PopMessageEnvelope)
{
    std::deque<zmq::message_t> msg;
    msg.push_back(zmq::message_t(123));
    msg.push_back(zmq::message_t(321));
    msg.push_back(zmq::message_t());
    msg.push_back(zmq::message_t(97));
    std::deque<zmq::message_t> env;
    env.push_back(zmq::message_t());
    const auto size = PopMessageEnvelope(msg, &env);
    EXPECT_EQ(3, size);
    ASSERT_EQ(2, env.size());
    EXPECT_EQ(123, env[0].size());
    EXPECT_EQ(321, env[1].size());
    ASSERT_EQ(1, msg.size());
    EXPECT_EQ(97, msg[0].size());
}

TEST(dsb_comm, PopMessageEnvelope_emptyEnvelope)
{
    std::deque<zmq::message_t> msg;
    msg.push_back(zmq::message_t());
    msg.push_back(zmq::message_t(123));
    msg.push_back(zmq::message_t(321));
    msg.push_back(zmq::message_t(97));
    std::deque<zmq::message_t> env;
    env.push_back(zmq::message_t());
    const auto size = PopMessageEnvelope(msg, &env);
    EXPECT_EQ(1, size);
    EXPECT_EQ(0, env.size());
    ASSERT_EQ(3, msg.size());
    EXPECT_EQ(123, msg[0].size());
    EXPECT_EQ(321, msg[1].size());
    EXPECT_EQ( 97, msg[2].size());
}

TEST(dsb_comm, PopMessageEnvelope_noEnvelope)
{
    std::deque<zmq::message_t> msg;
    msg.push_back(zmq::message_t(123));
    msg.push_back(zmq::message_t(321));
    msg.push_back(zmq::message_t(97));
    std::deque<zmq::message_t> env;
    env.push_back(zmq::message_t());
    const auto size = PopMessageEnvelope(msg, &env);
    EXPECT_EQ(0, size);
    EXPECT_EQ(0, env.size());
    ASSERT_EQ(3, msg.size());
    EXPECT_EQ(123, msg[0].size());
    EXPECT_EQ(321, msg[1].size());
    EXPECT_EQ( 97, msg[2].size());
}

TEST(dsb_comm, PopMessageEnvelope_dropEnvelope)
{
    std::deque<zmq::message_t> msg;
    msg.push_back(zmq::message_t(123));
    msg.push_back(zmq::message_t(321));
    msg.push_back(zmq::message_t());
    msg.push_back(zmq::message_t(97));
    const auto size = PopMessageEnvelope(msg);
    EXPECT_EQ(3, size);
    ASSERT_EQ(1, msg.size());
    EXPECT_EQ(97, msg[0].size());
}


TEST(dsb_comm, ToString)
{
    zmq::message_t msg(3);
    std::memcpy(msg.data(), "foo", 3);
    EXPECT_EQ("foo", ToString(msg));
}
