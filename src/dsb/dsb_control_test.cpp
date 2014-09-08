#include "gtest/gtest.h"
#include <cstring>
#include "dsb/control.hpp"
#include "dsb/error.hpp"
#include "dsb/protobuf.hpp"
#include "testing.pb.h"

using namespace dsb::control;

TEST(dsb_control, CreateHelloMessage)
{
    dsbproto::testing::IntString pbSrc;
    pbSrc.set_i(314);
    pbSrc.set_s("Hello");
    std::deque<zmq::message_t> msg;
    CreateHelloMessage(msg, 3, pbSrc);

    ASSERT_EQ(2U, msg.size());
    EXPECT_EQ(dsbproto::control::MSG_HELLO, ParseMessageType(msg[0]));
    EXPECT_EQ(3, ParseHelloMessage(msg));
    dsbproto::testing::IntString pbTgt;
    dsb::protobuf::ParseFromFrame(msg[1], pbTgt);
    EXPECT_EQ(314, pbTgt.i());
    EXPECT_EQ("Hello", pbTgt.s());
}

TEST(dsb_control, CreateDeniedMessage)
{
    std::deque<zmq::message_t> msg;
    CreateDeniedMessage(msg, "Hello World!");
    ASSERT_EQ(2U, msg.size());
    EXPECT_EQ(dsbproto::control::MSG_DENIED, ParseMessageType(msg[0]));
    try {
        ParseHelloMessage(msg);
        ADD_FAILURE();
    } catch (const RemoteErrorException& e) {
        SUCCEED();
        EXPECT_STRNE(nullptr, std::strstr(e.what(), "Hello World!"));
    } catch (...) {
        ADD_FAILURE();
    }
}

TEST(dsb_control, CreateMessage)
{
    dsbproto::testing::IntString pbSrc;
    pbSrc.set_i(314);
    pbSrc.set_s("Hello");
    std::deque<zmq::message_t> msg;
    CreateMessage(msg, dsbproto::control::MSG_READY, pbSrc);

    ASSERT_EQ(2U, msg.size());
    EXPECT_EQ(dsbproto::control::MSG_READY, ParseMessageType(msg[0]));
    dsbproto::testing::IntString pbTgt;
    dsb::protobuf::ParseFromFrame(msg[1], pbTgt);
    EXPECT_EQ(314, pbTgt.i());
    EXPECT_EQ("Hello", pbTgt.s());
}

TEST(dsb_control, CreateMessage_NonErrorMessage)
{
    std::deque<zmq::message_t> msg;
    CreateMessage(msg, dsbproto::control::MSG_READY);
    EXPECT_EQ(dsbproto::control::MSG_READY, NonErrorMessageType(msg));
}

TEST(dsb_control, CreateErrorMessage_NonErrorMessage)
{
    std::deque<zmq::message_t> msg;
    CreateErrorMessage(
        msg,
        dsbproto::control::ErrorInfo::INVALID_REQUEST,
        "some error");
    try {
        NonErrorMessageType(msg);
        ADD_FAILURE();
    } catch (const RemoteErrorException&) {
        SUCCEED();
    }
}

TEST(dsb_control, ParseMessageType_error)
{
    zmq::message_t msg;
    ASSERT_THROW(ParseMessageType(msg),
                 dsb::error::ProtocolViolationException);
}

TEST(dsb_control, ParseHelloMessage_error)
{
    std::deque<zmq::message_t> msg;
    msg.push_back(zmq::message_t(4));
    ASSERT_THROW(ParseHelloMessage(msg),
                 dsb::error::ProtocolViolationException);
}
