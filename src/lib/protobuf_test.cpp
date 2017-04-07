#include <gtest/gtest.h>
#include <coral/protobuf.hpp>
#include <testing.pb.h>


using namespace coral::protobuf;

TEST(coral_protobuf, SerializeAndParse)
{
    coralproto::testing::IntString pbSrc;
    pbSrc.set_i(123);
    pbSrc.set_s("Hello World!");

    zmq::message_t zMsg;
    SerializeToFrame(pbSrc, zMsg);

    coralproto::testing::IntString pbTgt;
    ParseFromFrame(zMsg, pbTgt);
    EXPECT_EQ(123, pbTgt.i());
    EXPECT_EQ("Hello World!", pbTgt.s());
}
