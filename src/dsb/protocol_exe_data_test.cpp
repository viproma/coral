#include "gtest/gtest.h"
#include "dsb/protocol/exe_data.hpp"

namespace ed = dsb::protocol::exe_data;

TEST(dsb_protocol_exe_data, CreateAndParse)
{
    ed::Message msg;
    msg.variable = dsb::model::Variable(123, 456);
    msg.value = 3.14;
    msg.timestepID = 100;

    std::vector<zmq::message_t> raw;
    ed::CreateMessage(msg, raw);

    const auto msg2 = ed::ParseMessage(raw);
    EXPECT_EQ(msg.variable,   msg2.variable);
    EXPECT_EQ(msg.value,      msg2.value);
    EXPECT_EQ(msg.timestepID, msg2.timestepID);
}
