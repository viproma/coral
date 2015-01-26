#include <cstring>
#include "boost/thread.hpp"
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

    std::deque<zmq::message_t> env;
    env.push_back(zmq::message_t(3));
    std::memcpy(env.back().data(), "foo", 3);
    std::deque<zmq::message_t> srcMsg;
    srcMsg.push_back(zmq::message_t(123));
    srcMsg.push_back(zmq::message_t(321));
    AddressedSend(sender, env, srcMsg);

    std::deque<zmq::message_t> tgtMsg(1);
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
    std::deque<zmq::message_t> msg;
    msg.push_back(zmq::message_t(123));
    msg.push_back(zmq::message_t(321));
    msg.push_back(zmq::message_t());
    msg.push_back(zmq::message_t(97));
    std::deque<zmq::message_t> env;
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
    std::deque<zmq::message_t> msg;
    msg.push_back(zmq::message_t());
    msg.push_back(zmq::message_t(123));
    msg.push_back(zmq::message_t(321));
    msg.push_back(zmq::message_t(97));
    std::deque<zmq::message_t> env;
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
    std::deque<zmq::message_t> msg;
    msg.push_back(zmq::message_t(123));
    msg.push_back(zmq::message_t(321));
    msg.push_back(zmq::message_t(97));
    std::deque<zmq::message_t> env;
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
    std::deque<zmq::message_t> msg;
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
    auto msg1 = std::deque<zmq::message_t>();
    auto msg2 = std::deque<zmq::message_t>();
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
    auto msg1 = std::deque<zmq::message_t>();
    msg1.push_back(ToFrame("foo"));
    msg1.push_back(ToFrame("bar"));
    auto msg2 = std::deque<zmq::message_t>();
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
    auto msg1 = std::deque<zmq::message_t>();
    msg1.push_back(ToFrame("foo"));
    msg1.push_back(ToFrame("bar"));
    auto msg2 = std::deque<zmq::message_t>();
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
    auto msg1 = std::deque<zmq::message_t>();
    auto msg2 = std::deque<zmq::message_t>();
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
    auto msg1 = std::deque<zmq::message_t>();
    msg1.push_back(ToFrame("foo"));
    msg1.push_back(ToFrame("bar"));
    auto msg2 = std::deque<zmq::message_t>();
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
    auto msg1 = std::deque<zmq::message_t>();
    msg1.push_back(ToFrame("foo"));
    msg1.push_back(ToFrame("bar"));
    auto msg2 = std::deque<zmq::message_t>();
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

TEST(dsb_comm, LastEndpoint)
{
    zmq::context_t ctx;
    zmq::socket_t sck(ctx, ZMQ_REP);
    EXPECT_TRUE(LastEndpoint(sck).empty());
    sck.bind("inproc://dsb_comm_LastEndpoint_test");
    EXPECT_EQ("inproc://dsb_comm_LastEndpoint_test", LastEndpoint(sck));
}

TEST(dsb_comm, Reactor)
{
    zmq::context_t ctx;
    zmq::socket_t svr1(ctx, ZMQ_PULL);
    svr1.bind("inproc://dsb_comm_Reactor_test_1");
    zmq::socket_t svr2(ctx, ZMQ_PULL);
    svr2.bind("inproc://dsb_comm_Reactor_test_2");

    boost::thread([&ctx]() {
        zmq::socket_t cli1(ctx, ZMQ_PUSH);
        cli1.connect("inproc://dsb_comm_Reactor_test_1");
        cli1.send("hello", 5);
        boost::this_thread::sleep_for(boost::chrono::milliseconds(13));
        cli1.send("world", 5);
    });

    boost::thread([&ctx]() {
        zmq::socket_t cli2(ctx, ZMQ_PUSH);
        cli2.connect("inproc://dsb_comm_Reactor_test_2");
        boost::this_thread::sleep_for(boost::chrono::milliseconds(10));
        cli2.send("foo", 3);
        boost::this_thread::sleep_for(boost::chrono::milliseconds(10));
        cli2.send("bar", 3);
    });

    Reactor reactor;

    int svr1Received = 0;
    reactor.AddSocket(svr1, [&](Reactor&, zmq::socket_t& s) {
        ++svr1Received;
        char buf[6];
        s.recv(buf, 5);
        buf[5] = '\0';
        if (svr1Received == 1) EXPECT_STREQ("hello", buf);
        else                   EXPECT_STREQ("world", buf);
    });

    int svr2Received1 = 0;
    int svr2Received2 = 0;
    reactor.AddSocket(svr2, [&](Reactor&, zmq::socket_t& s) {
        ++svr2Received1;
        char buf[4];
        s.recv(buf, 3);
        buf[3] = '\0';
        if (svr2Received1 == 1) EXPECT_STREQ("foo", buf);
        else                    EXPECT_STREQ("bar", buf); // We never actually get here
    });
    reactor.AddSocket(svr2, [&](Reactor& r, zmq::socket_t& s) {
        ++svr2Received2;
        r.RemoveSocket(s);
    });

    // This timer has 5 events.
    int timer1Events = 0;
    reactor.AddTimer(boost::chrono::milliseconds(12), 5, [&](Reactor&, int) {
        ++timer1Events;
    });

    // This timer runs until the reactor is stopped.
    int timer2Events = 0;
    reactor.AddTimer(boost::chrono::milliseconds(10), -1, [&](Reactor&, int) {
        ++timer2Events;
    });

    // This timer is set up to run indefinitely, but is removed after 5 events
    // by another timer (which subsequently removes itself).
    int timer3Events = 0;
    const auto timer3 = reactor.AddTimer(boost::chrono::milliseconds(9), 10, [&](Reactor&, int) {
        ++timer3Events;
    });
    reactor.AddTimer(boost::chrono::milliseconds(4), -1, [&](Reactor& r, int id) {
        if (timer3Events == 5) {
            r.RemoveTimer(timer3);
            r.RemoveTimer(id);
        }
    });

    // This timer stops the reactor.
    bool lifetimeExpired = false;
    reactor.AddTimer(boost::chrono::milliseconds(100), 1, [&](Reactor& r, int) {
        lifetimeExpired = true;
        r.Stop();
    });
    reactor.Run();

    EXPECT_EQ(2, svr1Received);
    EXPECT_EQ(1, svr2Received1);
    EXPECT_EQ(1, svr2Received2);
    EXPECT_EQ(5, timer1Events);
    EXPECT_TRUE(timer2Events >= 9 || timer2Events <= 11);
    EXPECT_EQ(5, timer3Events);
    EXPECT_TRUE(lifetimeExpired);
}
