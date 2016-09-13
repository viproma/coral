#include "gtest/gtest.h"
#include <cstring>
#include "coral/protocol/execution.hpp"
#include "coral/error.hpp"
#include "coral/protobuf.hpp"
#include "testing.pb.h"

using namespace coral::protocol::execution;

TEST(coral_protocol_execution, CreateHelloMessage)
{
    coralproto::testing::IntString pbSrc;
    pbSrc.set_i(314);
    pbSrc.set_s("Hello");
    std::vector<zmq::message_t> msg;
    CreateHelloMessage(msg, 3, pbSrc);

    ASSERT_EQ(2U, msg.size());
    EXPECT_EQ(coralproto::execution::MSG_HELLO, ParseMessageType(msg[0]));
    EXPECT_EQ(3, ParseHelloMessage(msg));
    coralproto::testing::IntString pbTgt;
    coral::protobuf::ParseFromFrame(msg[1], pbTgt);
    EXPECT_EQ(314, pbTgt.i());
    EXPECT_EQ("Hello", pbTgt.s());
}

TEST(coral_protocol_execution, CreateDeniedMessage)
{
    std::vector<zmq::message_t> msg;
    CreateDeniedMessage(msg, "Hello World!");
    ASSERT_EQ(2U, msg.size());
    EXPECT_EQ(coralproto::execution::MSG_DENIED, ParseMessageType(msg[0]));
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

TEST(coral_protocol_execution, CreateMessage)
{
    coralproto::testing::IntString pbSrc;
    pbSrc.set_i(314);
    pbSrc.set_s("Hello");
    std::vector<zmq::message_t> msg;
    CreateMessage(msg, coralproto::execution::MSG_READY, pbSrc);

    ASSERT_EQ(2U, msg.size());
    EXPECT_EQ(coralproto::execution::MSG_READY, ParseMessageType(msg[0]));
    coralproto::testing::IntString pbTgt;
    coral::protobuf::ParseFromFrame(msg[1], pbTgt);
    EXPECT_EQ(314, pbTgt.i());
    EXPECT_EQ("Hello", pbTgt.s());
}

TEST(coral_protocol_execution, CreateMessage_NonErrorMessage)
{
    std::vector<zmq::message_t> msg;
    CreateMessage(msg, coralproto::execution::MSG_READY);
    EXPECT_EQ(coralproto::execution::MSG_READY, NonErrorMessageType(msg));
}

TEST(coral_protocol_execution, CreateErrorMessage_NonErrorMessage)
{
    std::vector<zmq::message_t> msg;
    CreateErrorMessage(
        msg,
        coralproto::execution::ErrorInfo::INVALID_REQUEST,
        "some error");
    try {
        NonErrorMessageType(msg);
        ADD_FAILURE();
    } catch (const RemoteErrorException&) {
        SUCCEED();
    }
}

TEST(coral_protocol_execution, ParseMessageType_error)
{
    zmq::message_t msg;
    ASSERT_THROW(ParseMessageType(msg),
                 coral::error::ProtocolViolationException);
}

TEST(coral_protocol_execution, ParseHelloMessage_error)
{
    std::vector<zmq::message_t> msg;
    msg.push_back(zmq::message_t(4));
    ASSERT_THROW(ParseHelloMessage(msg),
                 coral::error::ProtocolViolationException);
}
