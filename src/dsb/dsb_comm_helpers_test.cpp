#include "gtest/gtest.h"
#include "dsb/comm/helpers.hpp"

using namespace dsb::comm;

TEST(dsb_comm_helpers, SendRecvControlHello)
{
    auto context = zmq::context_t();
    auto sender = zmq::socket_t(context, ZMQ_REQ);
    auto recver = zmq::socket_t(context, ZMQ_REP);
    recver.bind   ("inproc://dsb_comm_helpers-SendRecvControlHello");
    sender.connect("inproc://dsb_comm_helpers-SendRecvControlHello");
}

TEST(dsb_comm_helpers, SendRecvControlMessage)
{
}
