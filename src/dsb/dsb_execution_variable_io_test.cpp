#include "boost/chrono/duration.hpp"
#include "boost/thread/thread.hpp"
#include "gtest/gtest.h"

#include "dsb/comm/proxy.hpp"
#include "dsb/execution/variable_io.hpp"
#include "dsb/util.hpp"


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


TEST(dsb_execution, VariablePublishSubscribe)
{
    const auto ep = EndpointPair();
    auto proxy = dsb::comm::SpawnProxy(ZMQ_XSUB, ep.first, ZMQ_XPUB, ep.second);
    auto stopProxy = dsb::util::OnScopeExit([&]() { proxy.Stop(); });

    const dsb::model::SlaveID slaveID = 1;
    const dsb::model::VariableID varXID = 100;
    const dsb::model::VariableID varYID = 200;
    const auto varX = dsb::model::Variable(slaveID, varXID);
    const auto varY = dsb::model::Variable(slaveID, varYID);

    auto pub = dsb::execution::VariablePublisher();
    pub.Connect(ep.first, slaveID);

    auto sub = dsb::execution::VariableSubscriber();
    sub.Connect(ep.second);
    sub.Subscribe(varX);
    boost::this_thread::sleep_for(boost::chrono::milliseconds(100));

    dsb::model::StepID t = 0;
    // Verify that Update() times out if it doesn't receive data
    EXPECT_THROW(sub.Update(t, boost::chrono::milliseconds(1)), std::runtime_error);
    // Test transferring one variable
    pub.Publish(t, varXID, 123);
    sub.Update(t, boost::chrono::seconds(1));
    EXPECT_EQ(123, boost::get<int>(sub.Value(varX)));
    // Verify that Value() throws on a variable which is not subscribed to
    EXPECT_THROW(sub.Value(varY), std::logic_error);

    // Verify that the current value is used, old values are discarded, and
    // future values are queued.
    ++t;
    pub.Publish(t-1, varXID, 123);
    pub.Publish(t,   varXID, 456);
    pub.Publish(t+1, varXID, 789);
    sub.Update(t, boost::chrono::seconds(1));
    EXPECT_EQ(456, boost::get<int>(sub.Value(varX)));

    // Multiple variables
    sub.Subscribe(varY);
    boost::this_thread::sleep_for(boost::chrono::milliseconds(100));

    ++t;
    pub.Publish(t, varYID, std::string("Hello World"));
    sub.Update(t, boost::chrono::seconds(1));
    EXPECT_EQ(789, boost::get<int>(sub.Value(varX)));
    EXPECT_EQ("Hello World", boost::get<std::string>(sub.Value(varY)));

    pub.Publish(t, varXID, 1.0);
    ++t;
    pub.Publish(t, varYID, true);
    EXPECT_THROW(sub.Update(t, boost::chrono::milliseconds(1)), std::runtime_error);
    pub.Publish(t, varXID, 2.0);
    sub.Update(t, boost::chrono::seconds(1));
    EXPECT_EQ(2.0, boost::get<double>(sub.Value(varX)));
    EXPECT_TRUE(boost::get<bool>(sub.Value(varY)));

    // Unsubscribe to one variable
    ++t;
    sub.Unsubscribe(varX);
    pub.Publish(t, varXID, 100);
    pub.Publish(t, varYID, false);
    sub.Update(t, boost::chrono::seconds(1));
    EXPECT_THROW(sub.Value(varX), std::logic_error);
    EXPECT_FALSE(boost::get<bool>(sub.Value(varY)));
}
