#include "gtest/gtest.h"
#include "dsb/comm/protobuf.hpp"
#include "testing.pb.h"

using namespace dsb::comm;

TEST(dsb_comm_protobuf, SerializeAndParse)
{
    dsbproto::testing::IntString pbSrc;
    pbSrc.set_i(123);
    pbSrc.set_s("Hello World!");

    zmq::message_t zMsg;
    SerializeToFrame(pbSrc, &zMsg);

    dsbproto::testing::IntString pbTgt;
    ParseFromFrame(zMsg, &pbTgt);
    EXPECT_EQ(123, pbTgt.i());
    EXPECT_EQ("Hello World!", pbTgt.s());
}
