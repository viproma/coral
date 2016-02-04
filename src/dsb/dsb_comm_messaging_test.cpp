#include <chrono>
#include <cstring>
#include "gtest/gtest.h"
#include "dsb/comm/messaging.hpp"

using namespace dsb::comm;


TEST(dsb_comm, WaitForIncomingOutgoing)
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

TEST(dsb_comm, SendReceiveMessage)
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

TEST(dsb_comm, SendReceiveAddressedMessage)
{
    auto ctx = zmq::context_t();
    auto sender = zmq::socket_t(ctx, ZMQ_PUSH);
    auto recver = zmq::socket_t(ctx, ZMQ_PULL);
    const auto endpoint = std::string("inproc://")
        + ::testing::UnitTest::GetInstance()->current_test_info()->test_case_name();
    recver.bind(endpoint.c_str());
    sender.connect(endpoint.c_str());

    std::vector<zmq::message_t> env;
    env.push_back(zmq::message_t(3));
    std::memcpy(env.back().data(), "foo", 3);
    std::vector<zmq::message_t> srcMsg;
    srcMsg.push_back(zmq::message_t(123));
    srcMsg.push_back(zmq::message_t(321));
    AddressedSend(sender, env, srcMsg);

    std::vector<zmq::message_t> tgtMsg(1);
    Receive(recver, tgtMsg);
    ASSERT_EQ(4U, tgtMsg.size());
    ASSERT_EQ(3U, tgtMsg[0].size());
    EXPECT_EQ(0, std::memcmp(tgtMsg[0].data(), "foo", 3));
    EXPECT_EQ(  0U, tgtMsg[1].size());
    EXPECT_EQ(123U, tgtMsg[2].size());
    EXPECT_EQ(321U, tgtMsg[3].size());
}

TEST(dsb_comm, PopMessageEnvelope)
{
    std::vector<zmq::message_t> msg;
    msg.push_back(zmq::message_t(123));
    msg.push_back(zmq::message_t(321));
    msg.push_back(zmq::message_t());
    msg.push_back(zmq::message_t(97));
    std::vector<zmq::message_t> env;
    env.push_back(zmq::message_t());
    const auto size = PopMessageEnvelope(msg, &env);
    EXPECT_EQ(  3U, size);
    ASSERT_EQ(  2U, env.size());
    EXPECT_EQ(123U, env[0].size());
    EXPECT_EQ(321U, env[1].size());
    ASSERT_EQ(  1U, msg.size());
    EXPECT_EQ( 97U, msg[0].size());
}

TEST(dsb_comm, PopMessageEnvelope_emptyEnvelope)
{
    std::vector<zmq::message_t> msg;
    msg.push_back(zmq::message_t());
    msg.push_back(zmq::message_t(123));
    msg.push_back(zmq::message_t(321));
    msg.push_back(zmq::message_t(97));
    std::vector<zmq::message_t> env;
    env.push_back(zmq::message_t());
    const auto size = PopMessageEnvelope(msg, &env);
    EXPECT_EQ(  1U, size);
    EXPECT_EQ(  0U, env.size());
    ASSERT_EQ(  3U, msg.size());
    EXPECT_EQ(123U, msg[0].size());
    EXPECT_EQ(321U, msg[1].size());
    EXPECT_EQ( 97U, msg[2].size());
}

TEST(dsb_comm, PopMessageEnvelope_noEnvelope)
{
    std::vector<zmq::message_t> msg;
    msg.push_back(zmq::message_t(123));
    msg.push_back(zmq::message_t(321));
    msg.push_back(zmq::message_t(97));
    std::vector<zmq::message_t> env;
    env.push_back(zmq::message_t());
    const auto size = PopMessageEnvelope(msg, &env);
    EXPECT_EQ(  0U, size);
    EXPECT_EQ(  0U, env.size());
    ASSERT_EQ(  3U, msg.size());
    EXPECT_EQ(123U, msg[0].size());
    EXPECT_EQ(321U, msg[1].size());
    EXPECT_EQ( 97U, msg[2].size());
}

TEST(dsb_comm, PopMessageEnvelope_dropEnvelope)
{
    std::vector<zmq::message_t> msg;
    msg.push_back(zmq::message_t(123));
    msg.push_back(zmq::message_t(321));
    msg.push_back(zmq::message_t());
    msg.push_back(zmq::message_t(97));
    const auto size = PopMessageEnvelope(msg);
    EXPECT_EQ( 3U, size);
    ASSERT_EQ( 1U, msg.size());
    EXPECT_EQ(97U, msg[0].size());
}

TEST(dsb_comm, CopyMessage_emptySource)
{
    auto msg1 = std::vector<zmq::message_t>();
    auto msg2 = std::vector<zmq::message_t>();
    msg2.push_back(ToFrame("foo"));
    msg2.push_back(ToFrame("bar"));
    ASSERT_TRUE(msg1.empty());
    ASSERT_EQ(2U, msg2.size());
    CopyMessage(msg1, msg2);
    EXPECT_TRUE(msg1.empty());
    EXPECT_TRUE(msg2.empty());
}

TEST(dsb_comm, CopyMessage_emptyTarget)
{
    auto msg1 = std::vector<zmq::message_t>();
    msg1.push_back(ToFrame("foo"));
    msg1.push_back(ToFrame("bar"));
    auto msg2 = std::vector<zmq::message_t>();
    ASSERT_EQ(2U, msg1.size());
    ASSERT_TRUE(msg2.empty());
    CopyMessage(msg1, msg2);
    ASSERT_EQ(2U, msg1.size());
    EXPECT_EQ("foo", ToString(msg1[0]));
    EXPECT_EQ("bar", ToString(msg1[1]));
    ASSERT_EQ(2U, msg2.size());
    EXPECT_EQ("foo", ToString(msg2[0]));
    EXPECT_EQ("bar", ToString(msg2[1]));
}

TEST(dsb_comm, CopyMessage_nonEmptyTarget)
{
    auto msg1 = std::vector<zmq::message_t>();
    msg1.push_back(ToFrame("foo"));
    msg1.push_back(ToFrame("bar"));
    auto msg2 = std::vector<zmq::message_t>();
    msg2.push_back(ToFrame("baz"));
    ASSERT_EQ(2U, msg1.size());
    ASSERT_EQ(1U, msg2.size());
    CopyMessage(msg1, msg2);
    ASSERT_EQ(2U, msg1.size());
    EXPECT_EQ("foo", ToString(msg1[0]));
    EXPECT_EQ("bar", ToString(msg1[1]));
    ASSERT_EQ(2U, msg2.size());
    EXPECT_EQ("foo", ToString(msg2[0]));
    EXPECT_EQ("bar", ToString(msg2[1]));
}

namespace
{
    // Converts a reference into a const reference.
    template<typename T> const T& Const(T& ref) { return ref; }
}

TEST(dsb_comm, CopyMessage_const_emptySource)
{
    auto msg1 = std::vector<zmq::message_t>();
    auto msg2 = std::vector<zmq::message_t>();
    msg2.push_back(ToFrame("foo"));
    msg2.push_back(ToFrame("bar"));
    ASSERT_TRUE(msg1.empty());
    ASSERT_EQ(2U, msg2.size());
    CopyMessage(Const(msg1), msg2);
    EXPECT_TRUE(msg1.empty());
    EXPECT_TRUE(msg2.empty());
}

TEST(dsb_comm, CopyMessage_const_emptyTarget)
{
    auto msg1 = std::vector<zmq::message_t>();
    msg1.push_back(ToFrame("foo"));
    msg1.push_back(ToFrame("bar"));
    auto msg2 = std::vector<zmq::message_t>();
    ASSERT_EQ(2U, msg1.size());
    ASSERT_TRUE(msg2.empty());
    CopyMessage(Const(msg1), msg2);
    ASSERT_EQ(2U, msg1.size());
    EXPECT_EQ("foo", ToString(msg1[0]));
    EXPECT_EQ("bar", ToString(msg1[1]));
    ASSERT_EQ(2U, msg2.size());
    EXPECT_EQ("foo", ToString(msg2[0]));
    EXPECT_EQ("bar", ToString(msg2[1]));
}

TEST(dsb_comm, CopyMessage_const_nonEmptyTarget)
{
    auto msg1 = std::vector<zmq::message_t>();
    msg1.push_back(ToFrame("foo"));
    msg1.push_back(ToFrame("bar"));
    auto msg2 = std::vector<zmq::message_t>();
    msg2.push_back(ToFrame("baz"));
    ASSERT_EQ(2U, msg1.size());
    ASSERT_EQ(1U, msg2.size());
    CopyMessage(Const(msg1), msg2);
    ASSERT_EQ(2U, msg1.size());
    EXPECT_EQ("foo", ToString(msg1[0]));
    EXPECT_EQ("bar", ToString(msg1[1]));
    ASSERT_EQ(2U, msg2.size());
    EXPECT_EQ("foo", ToString(msg2[0]));
    EXPECT_EQ("bar", ToString(msg2[1]));
}

TEST(dsb_comm, ToFrame_ToString)
{
    auto msg = ToFrame("foo");
    EXPECT_EQ("foo", ToString(msg));
}
