#include "gtest/gtest.h"
#include "dsb/comm/helpers.hpp"

using namespace dsb::comm;


TEST(dsb_comm_helpers, PopMessageEnvelope)
{
    std::deque<zmq::message_t> msg;
    msg.emplace_back(123);
    msg.emplace_back(321);
    msg.emplace_back();
    msg.emplace_back(97);
    std::deque<zmq::message_t> env;
    env.emplace_back();
    const auto size = PopMessageEnvelope(msg, &env);
    EXPECT_EQ(3, size);
    ASSERT_EQ(2, env.size());
    EXPECT_EQ(123, env[0].size());
    EXPECT_EQ(321, env[1].size());
    ASSERT_EQ(1, msg.size());
    EXPECT_EQ(97, msg[0].size());
}

TEST(dsb_comm_helpers, PopMessageEnvelope_emptyEnvelope)
{
    std::deque<zmq::message_t> msg;
    msg.emplace_back();
    msg.emplace_back(123);
    msg.emplace_back(321);
    msg.emplace_back(97);
    std::deque<zmq::message_t> env;
    env.emplace_back();
    const auto size = PopMessageEnvelope(msg, &env);
    EXPECT_EQ(1, size);
    EXPECT_EQ(0, env.size());
    ASSERT_EQ(3, msg.size());
    EXPECT_EQ(123, msg[0].size());
    EXPECT_EQ(321, msg[1].size());
    EXPECT_EQ( 97, msg[2].size());
}

TEST(dsb_comm_helpers, PopMessageEnvelope_noEnvelope)
{
    std::deque<zmq::message_t> msg;
    msg.emplace_back(123);
    msg.emplace_back(321);
    msg.emplace_back(97);
    std::deque<zmq::message_t> env;
    env.emplace_back();
    const auto size = PopMessageEnvelope(msg, &env);
    EXPECT_EQ(0, size);
    EXPECT_EQ(0, env.size());
    ASSERT_EQ(3, msg.size());
    EXPECT_EQ(123, msg[0].size());
    EXPECT_EQ(321, msg[1].size());
    EXPECT_EQ( 97, msg[2].size());
}

TEST(dsb_comm_helpers, PopMessageEnvelope_dropEnvelope)
{
    std::deque<zmq::message_t> msg;
    msg.emplace_back(123);
    msg.emplace_back(321);
    msg.emplace_back();
    msg.emplace_back(97);
    const auto size = PopMessageEnvelope(msg);
    EXPECT_EQ(3, size);
    ASSERT_EQ(1, msg.size());
    EXPECT_EQ(97, msg[0].size());
}
