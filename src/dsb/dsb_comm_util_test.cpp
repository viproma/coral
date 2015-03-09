#include "gtest/gtest.h"
#include "dsb/comm/util.hpp"

using namespace dsb::comm;

TEST(dsb_comm, LastEndpoint)
{
    zmq::context_t ctx;
    zmq::socket_t sck(ctx, ZMQ_REP);
    EXPECT_TRUE(LastEndpoint(sck).empty());
    sck.bind("inproc://dsb_comm_LastEndpoint_test");
    EXPECT_EQ("inproc://dsb_comm_LastEndpoint_test", LastEndpoint(sck));
}
