#ifdef _WIN32
#   define NOMINMAX
#endif
#include <algorithm>
#include <chrono>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "boost/chrono/duration.hpp"
#include "boost/thread/barrier.hpp"
#include "boost/thread/latch.hpp"
#include "gtest/gtest.h"

#include "coral/bus/variable_io.hpp"
#include "coral/net/zmqx.hpp"
#include "coral/util.hpp"


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


TEST(coral_bus, VariablePublishSubscribe)
{
    const coral::model::SlaveID slaveID = 1;
    const coral::model::VariableID varXID = 100;
    const coral::model::VariableID varYID = 200;
    const auto varX = coral::model::Variable(slaveID, varXID);
    const auto varY = coral::model::Variable(slaveID, varYID);

    auto pub = coral::bus::VariablePublisher();
    pub.Bind(coral::net::Endpoint{"tcp://*:*"});

    auto inetEndpoint = coral::net::ip::Endpoint{pub.BoundEndpoint().Address()};
    inetEndpoint.SetAddress(coral::net::ip::Address{"localhost"});
    const auto endpoint = inetEndpoint.ToEndpoint("tcp");

    auto sub = coral::bus::VariableSubscriber();
    sub.Connect(&endpoint, 1);
    sub.Subscribe(varX);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    coral::model::StepID t = 0;
    // Verify that Update() times out if it doesn't receive data
    EXPECT_FALSE(sub.Update(t, std::chrono::milliseconds(1)));
    // Test transferring one variable
    pub.Publish(t, slaveID, varXID, 123);
    EXPECT_TRUE(sub.Update(t, std::chrono::seconds(1)));
    EXPECT_EQ(123, boost::get<int>(sub.Value(varX)));
    // Verify that Value() throws on a variable which is not subscribed to
    EXPECT_THROW(sub.Value(varY), std::logic_error);

    // Verify that the current value is used, old values are discarded, and
    // future values are queued.
    ++t;
    pub.Publish(t-1, slaveID, varXID, 123);
    pub.Publish(t,   slaveID, varXID, 456);
    pub.Publish(t+1, slaveID, varXID, 789);
    ASSERT_TRUE(sub.Update(t, std::chrono::seconds(1)));
    EXPECT_EQ(456, boost::get<int>(sub.Value(varX)));

    // Multiple variables
    sub.Subscribe(varY);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ++t;
    pub.Publish(t, slaveID, varYID, std::string("Hello World"));
    ASSERT_TRUE(sub.Update(t, std::chrono::seconds(1)));
    EXPECT_EQ(789, boost::get<int>(sub.Value(varX)));
    EXPECT_EQ("Hello World", boost::get<std::string>(sub.Value(varY)));

    pub.Publish(t, slaveID, varXID, 1.0);
    ++t;
    pub.Publish(t, slaveID, varYID, true);
    EXPECT_FALSE(sub.Update(t, std::chrono::milliseconds(1)));
    pub.Publish(t, slaveID, varXID, 2.0);
    ASSERT_TRUE(sub.Update(t, std::chrono::seconds(1)));
    EXPECT_EQ(2.0, boost::get<double>(sub.Value(varX)));
    EXPECT_TRUE(boost::get<bool>(sub.Value(varY)));

    // Unsubscribe to one variable
    ++t;
    sub.Unsubscribe(varX);
    pub.Publish(t, slaveID, varXID, 100);
    pub.Publish(t, slaveID, varYID, false);
    ASSERT_TRUE(sub.Update(t, std::chrono::seconds(1)));
    EXPECT_THROW(sub.Value(varX), std::logic_error);
    EXPECT_FALSE(boost::get<bool>(sub.Value(varY)));
}


TEST(coral_bus, VariablePublishSubscribePerformance)
{
    const int VAR_COUNT = 5000;
    const int STEP_COUNT = 50;
    const std::uint16_t PORT = 59264;

    // Prepare the list of variables
    const coral::model::SlaveID publisherID = 1;
    std::vector<coral::model::Variable> vars;
    for (int i = 1; i <= VAR_COUNT; ++i) {
        vars.emplace_back(publisherID, i);
    }

    boost::latch primeLatch(1);
    boost::barrier stepBarrier(2);

    auto pubThread = std::thread(
        [STEP_COUNT, PORT, publisherID, &vars, &primeLatch, &stepBarrier] ()
        {
            coral::bus::VariablePublisher pub;
            pub.Bind(coral::net::ip::Endpoint{"*", PORT}.ToEndpoint("tcp"));
            do {
                for (size_t i = 0; i < vars.size(); ++i) {
                    pub.Publish(0, publisherID, vars[i].ID(), i*1.0);
                }
            } while (primeLatch.wait_for(boost::chrono::milliseconds(100)) == boost::cv_status::timeout);
            for (int stepID = 1; stepID <= STEP_COUNT; ++stepID) {
                for (size_t i = 0; i < vars.size(); ++i) {
                    pub.Publish(stepID, publisherID, vars[i].ID(), i*1.0*stepID);
                }
                stepBarrier.wait();
            }
        });

    coral::bus::VariableSubscriber sub;
    const auto endpoint = coral::net::ip::Endpoint{"localhost", PORT}.ToEndpoint("tcp");
    sub.Connect(&endpoint, 1);

//    const auto tSub = std::chrono::steady_clock::now();
    for (const auto& var : vars) sub.Subscribe(var);

//    const auto tPrime = std::chrono::steady_clock::now();
    for (;;) {
        try {
            sub.Update(0, std::chrono::milliseconds(10000));
            break;
        } catch (const std::runtime_error&) { }
    }
    primeLatch.count_down();

    const auto tSim = std::chrono::steady_clock::now();
    const auto stepTimeout = std::chrono::milliseconds(std::max(100, VAR_COUNT*10));
    for (int stepID = 1; stepID <= STEP_COUNT; ++stepID) {
        // NOTE: If an exeption is thrown here (typically due to timeout),
        // stepBarrier will be destroyed while the other thread is still waiting
        // for it, triggering an assertion failure in the barrier destructor:
        //
        // boost::condition_variable::~condition_variable(): Assertion `!ret' failed.
        //
        // The solution is to increase the timeout -- or improve performance. ;)
        sub.Update(stepID, stepTimeout);
        for (size_t i = 0; i < vars.size(); ++i) {
            ASSERT_EQ(i*1.0*stepID, boost::get<double>(sub.Value(vars[i])));
        }
        stepBarrier.wait();
    }
    const auto tStop = std::chrono::steady_clock::now();

    pubThread.join();

    // Throughput = average number of variable values transferred per second
    const auto throughput = STEP_COUNT * VAR_COUNT
        * (1e9 / std::chrono::duration_cast<std::chrono::nanoseconds>(tStop-tSim).count());(STEP_COUNT*VAR_COUNT*1e9/std::chrono::duration_cast<std::chrono::nanoseconds>(tStop-tSim).count());
    EXPECT_GT(throughput, 50000.0);
/*
    std::cout
        << "\nSubscription time: " << std::chrono::duration_cast<std::chrono::milliseconds>(tPrime-tSub).count() << " ms"
        << "\nPriming time     : " << std::chrono::duration_cast<std::chrono::milliseconds>(tSim-tPrime).count() << " ms"
        << "\nTotal sim time   : " << std::chrono::duration_cast<std::chrono::milliseconds>(tStop-tSim).count() << " ms"
        << "\nStep time        : " << (std::chrono::duration_cast<std::chrono::microseconds>(tStop-tSim)/STEP_COUNT).count() << " us (" << STEP_COUNT << " steps)"
        << "\nPer-var time     : " <<  (std::chrono::duration_cast<std::chrono::nanoseconds>(tStop-tSim)/(STEP_COUNT*VAR_COUNT)).count() << " us (" << VAR_COUNT << " vars)"
        << "\nThroughput       : " << throughput << " vars/second"
        << std::endl;
*/
}
