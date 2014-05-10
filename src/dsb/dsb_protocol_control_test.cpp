#include "gtest/gtest.h"
#include "dsb/comm/helpers.hpp"
#include "dsb/protobuf.hpp"
#include "dsb/protocol/control.hpp"
#include "dsb/protocol/error.hpp"
#include "testing.pb.h"

using namespace dsb::protocol::control;

TEST(dsb_protocol_control, ParseMessageType_error)
{
    zmq::message_t msg;
    ASSERT_THROW(ParseMessageType(msg),
                 dsb::protocol::ProtocolViolationException);
}

TEST(dsb_protocol_control, SendHello)
{
    auto context = zmq::context_t();
    auto sender = zmq::socket_t(context, ZMQ_REQ);
    auto recver = zmq::socket_t(context, ZMQ_REP);
    recver.bind   ("inproc://dsb_protocol_control-SendRecvHello");
    sender.connect("inproc://dsb_protocol_control-SendRecvHello");

    dsbproto::testing::IntString pbSrc;
    pbSrc.set_i(314);
    pbSrc.set_s("Hello");
    SendHello(sender, 3, pbSrc);

    std::deque<zmq::message_t> zMsg;
    dsb::comm::RecvMessage(recver, &zMsg);
    ASSERT_EQ(2, zMsg.size());
    EXPECT_EQ(dsbproto::control::MessageType::HELLO, ParseMessageType(zMsg[0]));
    EXPECT_EQ(3, ParseProtocolVersion(zMsg[0]));
    dsbproto::testing::IntString pbTgt;
    dsb::protobuf::ParseFromFrame(zMsg[1], &pbTgt);
    EXPECT_EQ(314, pbTgt.i());
    EXPECT_EQ("Hello", pbTgt.s());
}

TEST(dsb_protocol_control, ParseProtocolVersion_error)
{
    zmq::message_t msg(4);
    ASSERT_THROW(ParseProtocolVersion(msg),
                 dsb::protocol::ProtocolViolationException);
}

TEST(dsb_protocol_control, SendMessage)
{
}
