#include <stdexcept>
#include "gtest/gtest.h"
#include "dsb/async.hpp"


TEST(dsb_async, CommThread)
{
    int eventCount = 0;

    auto thread = dsb::async::CommThread([&] (dsb::comm::Reactor& reactor)
    {
        reactor.AddTimer(
            std::chrono::milliseconds(100),
            -1, // run indefinitely
            [&] (dsb::comm::Reactor&, int) { ++eventCount; });
    });
    ASSERT_TRUE(thread.Active());

    // Delayed return
    ASSERT_LT(eventCount, 3); // kind of a precondition for this test
    auto delayedReturn = thread.Execute<int>(
        [&] (dsb::comm::Reactor& reactor, std::promise<int> promise)
        {
            // Hack: VS2013 doesn't support moving into lambdas
            auto promisePtr =
                std::make_shared<std::promise<int>>(std::move(promise));

            reactor.AddTimer(
                std::chrono::milliseconds(10),
                -1,
                [&eventCount, promisePtr]
                    (dsb::comm::Reactor& reactor, int self) mutable
                {
                    if (eventCount > 3) {
                        reactor.RemoveTimer(self);
                        promisePtr->set_value(123);
                    }
                });
        });
    EXPECT_EQ(123, delayedReturn.get());
    EXPECT_TRUE(eventCount > 3);
    EXPECT_TRUE(thread.Active());

    // Immediate return
    auto immediateReturn = thread.Execute<int>(
        [&] (dsb::comm::Reactor& reactor, std::promise<int> promise)
        {
            promise.set_value(456);
        });
    EXPECT_EQ(456, immediateReturn.get());
    EXPECT_TRUE(thread.Active());

    // Delayed throw
    ASSERT_LT(eventCount, 8); // kind of a precondition for this test
    auto delayedThrow = thread.Execute<int>(
        [&] (dsb::comm::Reactor& reactor, std::promise<int> promise)
        {
            // Hack: VS2013 doesn't support moving into lambdas
            auto promisePtr =
                std::make_shared<std::promise<int>>(std::move(promise));

            reactor.AddTimer(
                std::chrono::milliseconds(10),
                -1,
                [&eventCount, promisePtr]
                    (dsb::comm::Reactor& reactor, int self) mutable
                {
                    if (eventCount > 8) {
                        reactor.RemoveTimer(self);
                        promisePtr->set_exception(
                            std::make_exception_ptr(std::length_error{""}));
                    }
                });
        });
    EXPECT_THROW(delayedThrow.get(), std::length_error);
    EXPECT_TRUE(thread.Active());

    // Immediate throw
    auto immediateThrow = thread.Execute<int>(
        [&] (dsb::comm::Reactor& reactor, std::promise<int> promise)
        {
            promise.set_exception(
                std::make_exception_ptr(std::out_of_range{""}));
        });
    EXPECT_THROW(immediateThrow.get(), std::out_of_range);
    EXPECT_TRUE(thread.Active());

    // Normal shutdown
    thread.Shutdown();
    EXPECT_FALSE(thread.Active());

    // Rvalue assignment + unexpected thread death during init
    thread = dsb::async::CommThread([](dsb::comm::Reactor&)
    {
        throw std::domain_error{""};
    });
    EXPECT_TRUE(thread.Active());
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // time to die
    try {
        thread.Execute<void>([] (dsb::comm::Reactor&, std::promise<void>) { });
        ADD_FAILURE();
    } catch (const dsb::async::CommThreadDead& e) {
        EXPECT_THROW(
            std::rethrow_exception(e.OriginalException()),
            std::domain_error);
    } catch (...) {
        ADD_FAILURE();
    }
    EXPECT_FALSE(thread.Active());

    // Rvalue construction + unexpected thread death during execution
    thread = dsb::async::CommThread([] (dsb::comm::Reactor& reactor)
    {
        // Throw an exception after 100 ms
        reactor.AddTimer(
            std::chrono::milliseconds(100),
            1,
            [] (dsb::comm::Reactor&, int) { throw std::overflow_error{""}; });
    });
    EXPECT_TRUE(thread.Active());
    auto thread2 = std::move(thread);
    EXPECT_FALSE(thread.Active());
    EXPECT_TRUE(thread2.Active());
    std::this_thread::sleep_for(std::chrono::milliseconds(200)); // time to die
    try {
        thread2.Execute<void>([] (dsb::comm::Reactor&, std::promise<void>) { });
        ADD_FAILURE();
    } catch (const dsb::async::CommThreadDead& e) {
        EXPECT_THROW(
            std::rethrow_exception(e.OriginalException()),
            std::overflow_error);
    } catch (...) {
        ADD_FAILURE();
    }
    EXPECT_FALSE(thread2.Active());

    // Attempts to call functions on an inactive thread object
    EXPECT_THROW(
        thread2.Execute<void>([] (dsb::comm::Reactor&, std::promise<void>) { }),
        std::logic_error);
    EXPECT_THROW(thread2.Shutdown(), std::logic_error);
}
