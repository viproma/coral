#include <thread>
#include "gtest/gtest.h"
#include "dsb/comm/reactor.hpp"

using namespace dsb::comm;



TEST(dsb_comm, Reactor)
{
    zmq::context_t ctx;
    zmq::socket_t svr1(ctx, ZMQ_PULL);
    svr1.bind("inproc://dsb_comm_Reactor_test_1");
    zmq::socket_t svr2(ctx, ZMQ_PULL);
    svr2.bind("inproc://dsb_comm_Reactor_test_2");

    std::thread([&ctx]() {
        zmq::socket_t cli1(ctx, ZMQ_PUSH);
        cli1.connect("inproc://dsb_comm_Reactor_test_1");
        cli1.send("hello", 5);
        std::this_thread::sleep_for(std::chrono::milliseconds(13));
        cli1.send("world", 5);
    }).detach();

    std::thread([&ctx]() {
        zmq::socket_t cli2(ctx, ZMQ_PUSH);
        cli2.connect("inproc://dsb_comm_Reactor_test_2");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        cli2.send("foo", 3);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        cli2.send("bar", 3);
    }).detach();

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
    reactor.AddTimer(std::chrono::milliseconds(12), 5, [&](Reactor&, int) {
        ++timer1Events;
    });

    // This timer runs until the reactor is stopped.
    int timer2Events = 0;
    reactor.AddTimer(std::chrono::milliseconds(10), -1, [&](Reactor&, int) {
        ++timer2Events;
    });

    // This timer is set up to run indefinitely, but is removed after 5 events
    // by another timer (which subsequently removes itself).
    int timer3Events = 0;
    const auto timer3 = reactor.AddTimer(std::chrono::milliseconds(9), 10, [&](Reactor&, int) {
        ++timer3Events;
    });
    reactor.AddTimer(std::chrono::milliseconds(4), -1, [&](Reactor& r, int id) {
        if (timer3Events == 5) {
            r.RemoveTimer(timer3);
            r.RemoveTimer(id);
        }
    });

    // This timer stops the reactor.
    bool lifetimeExpired = false;
    reactor.AddTimer(std::chrono::milliseconds(100), 1, [&](Reactor& r, int) {
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


// Regression test for issue VIPROMA-39
TEST(dsb_comm, Reactor_bug39)
{
    Reactor reactor;
    zmq::context_t ctx;
    auto sck1 = zmq::socket_t(ctx, ZMQ_PAIR);
    sck1.bind("inproc://dsb_comm_Reactor_bug39");

    int canary = 87634861;
    reactor.AddSocket(sck1, [canary](Reactor& r, zmq::socket_t& s) {
        // Add enough dummy handlers that we're sure to trigger a reallocation.
        for (size_t i = 0; i < 1000; ++i) {
            const int backup = canary;
            r.AddSocket(s, [] (Reactor&, zmq::socket_t&) { });
            if (canary != backup) throw std::runtime_error("Memory error detected");
        }
        r.Stop();
    });
    reactor.AddTimer(std::chrono::milliseconds(10), 1, [canary](Reactor& r, int) {
        for (size_t i = 0; i < 1000; ++i) {
            const int backup = canary;
            r.AddTimer(std::chrono::milliseconds(10), 1, [](Reactor&,int){});
            if (canary != backup) throw std::runtime_error("Memory error detected");
        }
        r.Stop();
    });

    std::thread([&ctx]() {
        auto sck2 = zmq::socket_t(ctx, ZMQ_PAIR);
        sck2.connect("inproc://dsb_comm_Reactor_bug39");
        sck2.send("hello", 5);
    }).detach();

    ASSERT_NO_THROW(reactor.Run());
}
