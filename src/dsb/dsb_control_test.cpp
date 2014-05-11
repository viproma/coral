#include "gtest/gtest.h"
#include "dsb/control.hpp"
#include "dsb/error.hpp"
#include "dsb/protobuf.hpp"
#include "testing.pb.h"

using namespace dsb::control;

TEST(dsb_control, ParseMessageType_error)
{
    zmq::message_t msg;
    ASSERT_THROW(ParseMessageType(msg),
                 dsb::error::ProtocolViolationException);
}

TEST(dsb_control, CreateHelloMessage)
{
    dsbproto::testing::IntString pbSrc;
    pbSrc.set_i(314);
    pbSrc.set_s("Hello");
    std::deque<zmq::message_t> msg;
    CreateHelloMessage(3, pbSrc, msg);

    ASSERT_EQ(2, msg.size());
    EXPECT_EQ(dsbproto::control::MessageType::HELLO, ParseMessageType(msg[0]));
    EXPECT_EQ(3, ParseProtocolVersion(msg[0]));
    dsbproto::testing::IntString pbTgt;
    dsb::protobuf::ParseFromFrame(msg[1], pbTgt);
    EXPECT_EQ(314, pbTgt.i());
    EXPECT_EQ("Hello", pbTgt.s());
}

TEST(dsb_control, ParseProtocolVersion_error)
{
    zmq::message_t msg(4);
    ASSERT_THROW(ParseProtocolVersion(msg),
                 dsb::error::ProtocolViolationException);
}

TEST(dsb_control, CreateMessage)
{
    dsbproto::testing::IntString pbSrc;
    pbSrc.set_i(314);
    pbSrc.set_s("Hello");
    std::deque<zmq::message_t> msg;
    CreateMessage(dsbproto::control::MessageType::DESCRIBE, pbSrc, msg);

    ASSERT_EQ(2, msg.size());
    EXPECT_EQ(dsbproto::control::MessageType::DESCRIBE, ParseMessageType(msg[0]));
    dsbproto::testing::IntString pbTgt;
    dsb::protobuf::ParseFromFrame(msg[1], pbTgt);
    EXPECT_EQ(314, pbTgt.i());
    EXPECT_EQ("Hello", pbTgt.s());
}
