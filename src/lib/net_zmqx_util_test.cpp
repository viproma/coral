#include <stdexcept>
#include <string>
#include <typeinfo> // std::bad_cast

#include <gtest/gtest.h>
#include <coral/net/zmqx.hpp>

using namespace coral::net::zmqx;


TEST(coral_net, GlobalContext)
{
    auto& c1 = GlobalContext();
    auto& c2 = GlobalContext();
    EXPECT_EQ(&c1, &c2);
}


TEST(coral_net, BindToEphemeralPort)
{
    zmq::context_t ctx;
    auto srv = zmq::socket_t(ctx, ZMQ_REP);
    const auto port = BindToEphemeralPort(srv);
#ifdef _WIN32
    // Windows versions through XP use the port range 1025-5000 (but later
    // versions use the IANA recommended range, 49152-65535).
    EXPECT_GE(port, 1025);
#else
    // Many Linux kernels use the port range 32768-61000.
    EXPECT_GE(port, 32768);
#endif
    const auto endpoint = "tcp://localhost:" + std::to_string(port);
    auto cli = zmq::socket_t(ctx, ZMQ_REQ);
    cli.connect(endpoint.c_str());
    cli.send("hello", 6); // sixth char is the terminating zero
    char buf[6];
    srv.recv(buf, 6);
    EXPECT_STREQ("hello", buf);
}


TEST(coral_net, LastEndpoint)
{
    zmq::context_t ctx;
    zmq::socket_t sck(ctx, ZMQ_REP);
    EXPECT_TRUE(LastEndpoint(sck).empty());
    sck.bind("inproc://coral_net_LastEndpoint_test");
    EXPECT_EQ("inproc://coral_net_LastEndpoint_test", LastEndpoint(sck));
}


TEST(coral_net, EndpointPort)
{
    EXPECT_EQ(1234, EndpointPort("tcp://some.addr:1234"));
    EXPECT_THROW(EndpointPort("Hello World!"), std::invalid_argument);
    EXPECT_THROW(EndpointPort("tcp://some.addr:port"), std::bad_cast);
}
