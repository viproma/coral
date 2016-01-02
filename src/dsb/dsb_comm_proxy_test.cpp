#include <utility>
#include "gtest/gtest.h"
#include "dsb/config.h"
#include "dsb/comm/proxy.hpp"
#include "dsb/comm/util.hpp"

using namespace dsb::comm;


namespace
{
    std::pair<std::string, std::string> EndpointPair()
    {
        const auto base = std::string("inproc://")
            + ::testing::UnitTest::GetInstance()->current_test_info()->test_case_name()
            + '_'
            + ::testing::UnitTest::GetInstance()->current_test_info()->name();
        return std::make_pair(base + "1", base + "2");
    }
}


TEST(dsb_proxy, unidirectional)
{
    const auto ep = EndpointPair();
    auto proxy = SpawnProxy(ZMQ_PULL, ep.first, ZMQ_PUSH, ep.second);

    auto fe = zmq::socket_t(dsb::comm::GlobalContext(), ZMQ_PUSH);
    fe.connect(ep.first.c_str());
    auto be = zmq::socket_t(dsb::comm::GlobalContext(), ZMQ_PULL);
    be.connect(ep.second.c_str());

    for (char c = 0; c < 10; ++c) {
        fe.send(&c, sizeof(c));
    }
    for (char c = 0; c < 10; ++c) {
        char buf = -1;
        EXPECT_EQ(sizeof(buf), be.recv(&buf, sizeof(buf)));
        EXPECT_EQ(c, buf);
    }
    proxy.Stop();
    proxy.Thread__().join();
}


TEST(dsb_proxy, bidirectional_multiclient)
{
    const auto ep = EndpointPair();
    auto proxy = SpawnProxy(ZMQ_ROUTER, ep.first, ZMQ_DEALER, ep.second);

    const int CLIENT_COUNT = 10;
    std::vector<zmq::socket_t> clients;
    for (int k = 0; k < CLIENT_COUNT; ++k) {
        clients.emplace_back(dsb::comm::GlobalContext(), ZMQ_REQ);
        clients.back().connect(ep.first.c_str());
    }
    auto server = zmq::socket_t(dsb::comm::GlobalContext(), ZMQ_REP);
    server.connect(ep.second.c_str());

    for (int i = 0; i < 100; ++i) {
        for (int k = 0; k < CLIENT_COUNT; ++k) {
            const auto req = i*k;
            clients[k].send(&req, sizeof(req));
        }
        for (int k = 0; k < CLIENT_COUNT; ++k) {
            int req = -1;
            server.recv(&req, sizeof(req));
            req += 123;
            server.send(&req, sizeof(req));
        }
        for (int k = 0; k < CLIENT_COUNT; ++k) {
            int rep = -1;
            clients[k].recv(&rep, sizeof(rep));
            EXPECT_EQ(i*k+123, rep);
        }
    }
    proxy.Stop();
    proxy.Thread__().join();
}


TEST(dsb_proxy, silence_timeout)
{
    using namespace std::chrono;
    using namespace std::this_thread;

    const auto ep = EndpointPair();
    auto proxy = SpawnProxy(
        ZMQ_PULL, ep.first,
        ZMQ_PUSH, ep.second,
        milliseconds(200));
    auto push = zmq::socket_t(dsb::comm::GlobalContext(), ZMQ_PUSH);
    push.connect(ep.first.c_str());
    auto pull = zmq::socket_t(dsb::comm::GlobalContext(), ZMQ_PULL);
    pull.connect(ep.second.c_str());

    sleep_for(milliseconds(100));
    push.send("", 0);
    const auto then = steady_clock::now();
    proxy.Thread__().join();
    const auto shutdownTime = steady_clock::now() - then;
    EXPECT_GT(shutdownTime, milliseconds(180));
    EXPECT_LT(shutdownTime, milliseconds(220));
}
