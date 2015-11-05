#include "gtest/gtest.h"
#include "dsb/config.h"
#include "dsb/comm/proxy.hpp"
#include "dsb/comm/util.hpp"

using namespace dsb::comm;


TEST(dsb_proxy, unidirectional)
{
    auto proxy = SpawnProxy(
        ZMQ_PULL, "inproc://dsb_proxy_test_frontend",
        ZMQ_PUSH, "inproc://dsb_proxy_test_backend");

    auto fe = zmq::socket_t(dsb::comm::GlobalContext(), ZMQ_PUSH);
    fe.connect("inproc://dsb_proxy_test_frontend");
    auto be = zmq::socket_t(dsb::comm::GlobalContext(), ZMQ_PULL);
    be.connect("inproc://dsb_proxy_test_backend");

    for (char c = 0; c < 10; ++c) {
        fe.send(&c, sizeof(c));
    }
    for (char c = 0; c < 10; ++c) {
        char buf = -1;
        EXPECT_EQ(sizeof(buf), be.recv(&buf, sizeof(buf)));
        EXPECT_EQ(c, buf);
    }
    proxy.Stop();
    EXPECT_TRUE(proxy.Thread__().try_join_for(boost::chrono::milliseconds(10)));
}


TEST(dsb_proxy, bidirectional_multiclient)
{
    auto proxy = SpawnProxy(
        ZMQ_ROUTER, "inproc://dsb_proxy_test_frontend",
        ZMQ_DEALER, "inproc://dsb_proxy_test_backend");

    const int CLIENT_COUNT = 10;
    std::vector<zmq::socket_t> clients;
    for (int k = 0; k < CLIENT_COUNT; ++k) {
        clients.emplace_back(dsb::comm::GlobalContext(), ZMQ_REQ);
        clients.back().connect("inproc://dsb_proxy_test_frontend");
    }
    auto server = zmq::socket_t(dsb::comm::GlobalContext(), ZMQ_REP);
    server.connect("inproc://dsb_proxy_test_backend");

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
    EXPECT_TRUE(proxy.Thread__().try_join_for(boost::chrono::milliseconds(10)));
}


TEST(dsb_proxy, silence_timeout)
{
    using namespace boost::chrono;
    using namespace boost::this_thread;

    auto proxy = SpawnProxy(
        ZMQ_PULL, "inproc://dsb_proxy_test_frontend",
        ZMQ_PUSH, "inproc://dsb_proxy_test_backend",
        milliseconds(200));
    auto push = zmq::socket_t(dsb::comm::GlobalContext(), ZMQ_PUSH);
    push.connect("inproc://dsb_proxy_test_frontend");
    auto pull = zmq::socket_t(dsb::comm::GlobalContext(), ZMQ_PULL);
    pull.connect("inproc://dsb_proxy_test_backend");

    sleep_for(milliseconds(100));
    push.send("", 0);
    sleep_for(milliseconds(150));
    ASSERT_FALSE(proxy.Thread__().try_join_for(milliseconds(0)));
    ASSERT_TRUE(proxy.Thread__().try_join_for(milliseconds(70)));
}
