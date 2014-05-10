#include "gtest/gtest.h"
#include "dsb/comm/error.hpp"
#include "dsb/comm/helpers.hpp"
#include "dsb/comm/protobuf.hpp"
#include "testing.pb.h"

using namespace dsb::comm;

TEST(dsb_comm_helpers, ParseControlMessageType_error)
{
    zmq::message_t msg;
    ASSERT_THROW(ParseControlMessageType(msg), ProtocolViolationException);
}

TEST(dsb_comm_helpers, SendControlHello)
{
    auto context = zmq::context_t();
    auto sender = zmq::socket_t(context, ZMQ_REQ);
    auto recver = zmq::socket_t(context, ZMQ_REP);
    recver.bind   ("inproc://dsb_comm_helpers-SendRecvControlHello");
    sender.connect("inproc://dsb_comm_helpers-SendRecvControlHello");

    dsbproto::testing::IntString pbSrc;
    pbSrc.set_i(314);
    pbSrc.set_s("Hello");
    SendControlHello(sender, 3, pbSrc);

    std::deque<zmq::message_t> zMsg;
    RecvMessage(recver, &zMsg);
    ASSERT_EQ(2, zMsg.size());
    EXPECT_EQ(dsbproto::control::MessageType::HELLO, ParseControlMessageType(zMsg[0]));
    EXPECT_EQ(3, ParseControlProtocolVersion(zMsg[0]));
    dsbproto::testing::IntString pbTgt;
    ParseFromFrame(zMsg[1], &pbTgt);
    EXPECT_EQ(314, pbTgt.i());
    EXPECT_EQ("Hello", pbTgt.s());
}

TEST(dsb_comm_helpers, ParseControlProtocolVersion_error)
{
    zmq::message_t msg(4);
    ASSERT_THROW(ParseControlProtocolVersion(msg), ProtocolViolationException);
}

TEST(dsb_comm_helpers, SendControlMessage)
{
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
