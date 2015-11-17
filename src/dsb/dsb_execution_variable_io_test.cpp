#include <string>
#include <utility>
#include <vector>

#include "boost/chrono.hpp"
#include "boost/thread.hpp"
#include "boost/thread/latch.hpp"
#include "gtest/gtest.h"

#include "dsb/comm/proxy.hpp"
#include "dsb/comm/util.hpp"
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


TEST(dsb_execution, VariablePublishSubscribePerformance)
{
    const int VAR_COUNT = 5000;
    const int STEP_COUNT = 50;

    // Fire up the proxy
    auto proxySocket1 = zmq::socket_t(dsb::comm::GlobalContext(), ZMQ_XSUB);
    auto proxySocket2 = zmq::socket_t(dsb::comm::GlobalContext(), ZMQ_XPUB);
    int zero = 0;
    proxySocket1.setsockopt(ZMQ_SNDHWM, &zero, sizeof(zero));
    proxySocket1.setsockopt(ZMQ_RCVHWM, &zero, sizeof(zero));
    proxySocket2.setsockopt(ZMQ_SNDHWM, &zero, sizeof(zero));
    proxySocket2.setsockopt(ZMQ_RCVHWM, &zero, sizeof(zero));
    const auto proxyPort1 = dsb::comm::BindToEphemeralPort(proxySocket1);
    const auto proxyPort2 = dsb::comm::BindToEphemeralPort(proxySocket2);
    const auto proxyEndpoint1 = "tcp://localhost:" + std::to_string(proxyPort1);
    const auto proxyEndpoint2 = "tcp://localhost:" + std::to_string(proxyPort2);
    auto proxy = dsb::comm::SpawnProxy(std::move(proxySocket1), std::move(proxySocket2));

    // Prepare the list of variables
    const dsb::model::SlaveID publisherID = 1;
    std::vector<dsb::model::Variable> vars;
    for (int i = 1; i <= VAR_COUNT; ++i) {
        vars.emplace_back(publisherID, i);
    }

    boost::latch primeLatch(1);
    boost::barrier stepBarrier(2);

    auto pubThread = boost::thread(
        [STEP_COUNT, proxyEndpoint1, publisherID, &vars, &primeLatch, &stepBarrier] ()
        {
            auto pub = dsb::execution::VariablePublisher();
            pub.Connect(proxyEndpoint1, publisherID);
            do {
                for (size_t i = 0; i < vars.size(); ++i) {
                    pub.Publish(0, vars[i].ID(), i*1.0);
                }
            } while (primeLatch.wait_for(boost::chrono::milliseconds(100)) == boost::cv_status::timeout);
            for (int stepID = 1; stepID <= STEP_COUNT; ++stepID) {
                for (size_t i = 0; i < vars.size(); ++i) {
                    pub.Publish(stepID, vars[i].ID(), i*1.0*stepID);
                }
                stepBarrier.wait();
            }
        });

    auto sub = dsb::execution::VariableSubscriber();
    sub.Connect(proxyEndpoint2);

    //const auto tSub = boost::chrono::steady_clock::now();
    for (const auto& var : vars) sub.Subscribe(var);

    //const auto tPrime = boost::chrono::steady_clock::now();
    for (;;) {
        try {
            sub.Update(0, boost::chrono::milliseconds(10000));
            break;
        } catch (const std::runtime_error&) { }
    }
    primeLatch.count_down();

    const auto tSim = boost::chrono::steady_clock::now();
    const auto stepTimeout = boost::chrono::milliseconds(std::max(100, VAR_COUNT*10));
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
    const auto tStop = boost::chrono::steady_clock::now();

    pubThread.join();
    proxy.Stop(true);

    // Throughput = average number of variable values transferred per second
    const auto throughput = STEP_COUNT * VAR_COUNT
        * (1e9 / boost::chrono::duration_cast<boost::chrono::nanoseconds>(tStop-tSim).count());(STEP_COUNT*VAR_COUNT*1e9/boost::chrono::duration_cast<boost::chrono::nanoseconds>(tStop-tSim).count());
    EXPECT_GT(throughput, 10000.0);
/*
    std::cout
        << "\nSubscription time: " << boost::chrono::duration_cast<boost::chrono::milliseconds>(tPrime-tSub)
        << "\nPriming time     : " << boost::chrono::duration_cast<boost::chrono::milliseconds>(tSim-tPrime)
        << "\nTotal sim time   : " << boost::chrono::duration_cast<boost::chrono::milliseconds>(tStop-tSim)
        << "\nStep time        : " << (boost::chrono::duration_cast<boost::chrono::microseconds>(tStop-tSim)/STEP_COUNT) << " (" << STEP_COUNT << " steps)"
        << "\nPer-var time     : " <<  (boost::chrono::duration_cast<boost::chrono::nanoseconds>(tStop-tSim)/(STEP_COUNT*VAR_COUNT)) << " (" << VAR_COUNT << " vars)"
        << "\nThroughput       : " << throughput << " vars/second"
        << std::endl;
*/
}
