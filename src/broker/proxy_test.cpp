#include "gtest/gtest.h"
#include "proxy.hpp"

using namespace dsb::broker;


TEST(dsb_broker, proxy_unidirectional)
{
    auto ctx = std::make_shared<zmq::context_t>();
    auto proxy = SpawnProxy(ctx,
        ZMQ_PULL, "inproc://dsb_proxy_test_frontend",
        ZMQ_PUSH, "inproc://dsb_proxy_test_backend");

    auto fe = zmq::socket_t(*ctx, ZMQ_PUSH);
    fe.connect("inproc://dsb_proxy_test_frontend");
    auto be = zmq::socket_t(*ctx, ZMQ_PULL);
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


TEST(dsb_broker, proxy_bidirectional_multiclient)
{
    auto ctx = std::make_shared<zmq::context_t>();
    auto proxy = SpawnProxy(ctx,
        ZMQ_ROUTER, "inproc://dsb_proxy_test_frontend",
        ZMQ_DEALER, "inproc://dsb_proxy_test_backend");

    const int CLIENT_COUNT = 10;
    std::vector<zmq::socket_t> clients;
    for (int k = 0; k < CLIENT_COUNT; ++k) {
#if DSB_HAS_VARARG_EMPLACE_BACK
        clients.emplace_back(*ctx, ZMQ_REQ);
#else
        clients.emplace_back(zmq::socket_t(*ctx, ZMQ_REQ));
#endif
        clients.back().connect("inproc://dsb_proxy_test_frontend");
    }
    auto server = zmq::socket_t(*ctx, ZMQ_REP);
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