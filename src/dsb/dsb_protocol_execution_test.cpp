#include "gtest/gtest.h"
#include <cstring>
#include "dsb/protocol/execution.hpp"
#include "dsb/error.hpp"
#include "dsb/protobuf.hpp"
#include "testing.pb.h"

using namespace dsb::protocol::execution;

TEST(dsb_protocol_execution, CreateHelloMessage)
{
    dsbproto::testing::IntString pbSrc;
    pbSrc.set_i(314);
    pbSrc.set_s("Hello");
    std::vector<zmq::message_t> msg;
    CreateHelloMessage(msg, 3, pbSrc);

    ASSERT_EQ(2U, msg.size());
    EXPECT_EQ(dsbproto::execution::MSG_HELLO, ParseMessageType(msg[0]));
    EXPECT_EQ(3, ParseHelloMessage(msg));
    dsbproto::testing::IntString pbTgt;
    dsb::protobuf::ParseFromFrame(msg[1], pbTgt);
    EXPECT_EQ(314, pbTgt.i());
    EXPECT_EQ("Hello", pbTgt.s());
}

TEST(dsb_protocol_execution, CreateDeniedMessage)
{
    std::vector<zmq::message_t> msg;
    CreateDeniedMessage(msg, "Hello World!");
    ASSERT_EQ(2U, msg.size());
    EXPECT_EQ(dsbproto::execution::MSG_DENIED, ParseMessageType(msg[0]));
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

TEST(dsb_protocol_execution, CreateMessage)
{
    dsbproto::testing::IntString pbSrc;
    pbSrc.set_i(314);
    pbSrc.set_s("Hello");
    std::vector<zmq::message_t> msg;
    CreateMessage(msg, dsbproto::execution::MSG_READY, pbSrc);

    ASSERT_EQ(2U, msg.size());
    EXPECT_EQ(dsbproto::execution::MSG_READY, ParseMessageType(msg[0]));
    dsbproto::testing::IntString pbTgt;
    dsb::protobuf::ParseFromFrame(msg[1], pbTgt);
    EXPECT_EQ(314, pbTgt.i());
    EXPECT_EQ("Hello", pbTgt.s());
}

TEST(dsb_protocol_execution, CreateMessage_NonErrorMessage)
{
    std::vector<zmq::message_t> msg;
    CreateMessage(msg, dsbproto::execution::MSG_READY);
    EXPECT_EQ(dsbproto::execution::MSG_READY, NonErrorMessageType(msg));
}

TEST(dsb_protocol_execution, CreateErrorMessage_NonErrorMessage)
{
    std::vector<zmq::message_t> msg;
    CreateErrorMessage(
        msg,
        dsbproto::execution::ErrorInfo::INVALID_REQUEST,
        "some error");
    try {
        NonErrorMessageType(msg);
        ADD_FAILURE();
    } catch (const RemoteErrorException&) {
        SUCCEED();
    }
}

TEST(dsb_protocol_execution, ParseMessageType_error)
{
    zmq::message_t msg;
    ASSERT_THROW(ParseMessageType(msg),
                 dsb::error::ProtocolViolationException);
}

TEST(dsb_protocol_execution, ParseHelloMessage_error)
{
    std::vector<zmq::message_t> msg;
    msg.push_back(zmq::message_t(4));
    ASSERT_THROW(ParseHelloMessage(msg),
                 dsb::error::ProtocolViolationException);
}
