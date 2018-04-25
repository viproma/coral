#include <stdexcept>
#include <gtest/gtest.h>
#include <coral/async.hpp>

namespace
{
    struct MyData
    {
        int eventCount = 0;
    };
}


// Note: Any modifications to this test should probably be ported to the
// next one (CommThread_void) as well.
TEST(coral_async, CommThread)
{
    auto thread = coral::async::CommThread<MyData>{};
    ASSERT_TRUE(thread.Active());

    // Immediate return, void value (also, do setup for following tests)
    auto immediateReturn = thread.Execute<void>(
        [] (coral::net::Reactor& reactor, MyData& data, std::promise<void> promise)
        {
            reactor.AddTimer(
                std::chrono::milliseconds(100),
                -1, // run indefinitely
                [&] (coral::net::Reactor&, int) { ++data.eventCount; });
            promise.set_value();
        });
    EXPECT_NO_THROW(immediateReturn.get());
    ASSERT_TRUE(thread.Active());

    // Delayed return, int value
    auto delayedReturn = thread.Execute<int>(
        [] (coral::net::Reactor& reactor, MyData& data, std::promise<int> promise)
        {
            auto promisePtr =
                std::make_shared<std::promise<int>>(std::move(promise));
            reactor.AddTimer(
                std::chrono::milliseconds(10),
                -1,
                [&data, promisePtr]
                    (coral::net::Reactor& reactor, int self)
                {
                    if (data.eventCount > 5) {
                        reactor.RemoveTimer(self);
                        promisePtr->set_value(data.eventCount);
                    }
                });
        });
    EXPECT_GT(delayedReturn.get(), 5);
    ASSERT_TRUE(thread.Active());

    // Delayed throw
    auto delayedThrow = thread.Execute<int>(
        [] (coral::net::Reactor& reactor, MyData& data, std::promise<int> promise)
        {
            auto promisePtr =
                std::make_shared<std::promise<int>>(std::move(promise));
            reactor.AddTimer(
                std::chrono::milliseconds(10),
                -1,
                [&data, promisePtr]
                    (coral::net::Reactor& reactor, int self)
                {
                    if (data.eventCount > 10) {
                        reactor.RemoveTimer(self);
                        promisePtr->set_exception(
                            std::make_exception_ptr(std::length_error{""}));
                    }
                });
        });
    EXPECT_THROW(delayedThrow.get(), std::length_error);
    ASSERT_TRUE(thread.Active());

    // Immediate throw
    auto immediateThrow = thread.Execute<int>(
        [] (coral::net::Reactor&, MyData&, std::promise<int> promise)
        {
            promise.set_exception(
                std::make_exception_ptr(std::out_of_range{""}));
        });
    EXPECT_THROW(immediateThrow.get(), std::out_of_range);
    ASSERT_TRUE(thread.Active());

    // Normal shutdown
    thread.Shutdown();
    EXPECT_FALSE(thread.Active());

    // Rvalue assignment + unexpected thread death during execution
    thread = coral::async::CommThread<MyData>();
    ASSERT_TRUE(thread.Active());
    auto brokenFuture = thread.Execute<void>(
        [] (coral::net::Reactor&, MyData&, std::promise<void>)
        {
            throw std::underflow_error{""};
        });
    EXPECT_THROW(brokenFuture.get(), std::future_error); // broken promise
    ASSERT_TRUE(thread.Active());
    try {
        thread.Execute<void>([] (coral::net::Reactor&, MyData&, std::promise<void>) { });
        ADD_FAILURE();
    } catch (const coral::async::CommThreadDead& e) {
        EXPECT_THROW(
            std::rethrow_exception(e.OriginalException()),
            std::underflow_error);
    } catch (...) {
        ADD_FAILURE();
    }
    EXPECT_FALSE(thread.Active());

    // Rvalue construction + unexpected delayed thread death during execution
    thread = coral::async::CommThread<MyData>();
    EXPECT_NO_THROW(thread.Execute<void>(
        [] (coral::net::Reactor& reactor, MyData&, std::promise<void> promise)
        {
            // Throw an exception after 100 ms
            reactor.AddTimer(
                std::chrono::milliseconds(100),
                1,
                [] (coral::net::Reactor&, int) { throw std::overflow_error{""}; });
            promise.set_value();
        }).get());
    ASSERT_TRUE(thread.Active());
    auto thread2 = std::move(thread);
    EXPECT_FALSE(thread.Active());
    ASSERT_TRUE(thread2.Active());
    std::this_thread::sleep_for(std::chrono::milliseconds(200)); // time to die
    try {
        thread2.Execute<void>([] (coral::net::Reactor&, MyData&, std::promise<void>) { });
        ADD_FAILURE();
    } catch (const coral::async::CommThreadDead& e) {
        EXPECT_THROW(
            std::rethrow_exception(e.OriginalException()),
            std::overflow_error);
    } catch (...) {
        ADD_FAILURE();
    }
    ASSERT_FALSE(thread2.Active());

    // Attempts to call functions on an inactive thread object
    EXPECT_THROW(
        thread2.Execute<void>([] (coral::net::Reactor&, MyData&, std::promise<void>) { }),
        std::logic_error);
    EXPECT_THROW(thread2.Shutdown(), std::logic_error);
}


// Now do it all over again, only with `void`
TEST(coral_async, CommThread_void)
{
    auto thread = coral::async::CommThread<void>{};
    ASSERT_TRUE(thread.Active());

    int eventCount = 0;

    // Immediate return, void value (also, do setup for following tests)
    auto immediateReturn = thread.Execute<void>(
        [&eventCount] (coral::net::Reactor& reactor, std::promise<void> promise)
        {
            reactor.AddTimer(
                std::chrono::milliseconds(100),
                -1, // run indefinitely
                [&] (coral::net::Reactor&, int) { ++eventCount; });
            promise.set_value();
        });
    EXPECT_NO_THROW(immediateReturn.get());
    ASSERT_TRUE(thread.Active());

    // Delayed return, int value
    auto delayedReturn = thread.Execute<int>(
        [&eventCount] (coral::net::Reactor& reactor, std::promise<int> promise)
        {
            auto promisePtr =
                std::make_shared<std::promise<int>>(std::move(promise));
            reactor.AddTimer(
                std::chrono::milliseconds(10),
                -1,
                [&eventCount, promisePtr]
                    (coral::net::Reactor& reactor, int self)
                {
                    if (eventCount > 5) {
                        reactor.RemoveTimer(self);
                        promisePtr->set_value(eventCount);
                    }
                });
        });
    EXPECT_GT(delayedReturn.get(), 5);
    ASSERT_TRUE(thread.Active());

    // Delayed throw
    auto delayedThrow = thread.Execute<int>(
        [&eventCount] (coral::net::Reactor& reactor, std::promise<int> promise)
        {
            auto promisePtr =
                std::make_shared<std::promise<int>>(std::move(promise));
            reactor.AddTimer(
                std::chrono::milliseconds(10),
                -1,
                [&eventCount, promisePtr]
                    (coral::net::Reactor& reactor, int self)
                {
                    if (eventCount > 10) {
                        reactor.RemoveTimer(self);
                        promisePtr->set_exception(
                            std::make_exception_ptr(std::length_error{""}));
                    }
                });
        });
    EXPECT_THROW(delayedThrow.get(), std::length_error);
    ASSERT_TRUE(thread.Active());

    // Immediate throw
    auto immediateThrow = thread.Execute<int>(
        [] (coral::net::Reactor&, std::promise<int> promise)
        {
            promise.set_exception(
                std::make_exception_ptr(std::out_of_range{""}));
        });
    EXPECT_THROW(immediateThrow.get(), std::out_of_range);
    ASSERT_TRUE(thread.Active());

    // Normal shutdown
    thread.Shutdown();
    EXPECT_FALSE(thread.Active());

    // Rvalue assignment + unexpected thread death during execution
    thread = coral::async::CommThread<void>();
    ASSERT_TRUE(thread.Active());
    auto brokenFuture = thread.Execute<void>(
        [] (coral::net::Reactor&, std::promise<void>)
        {
            throw std::underflow_error{""};
        });
    EXPECT_THROW(brokenFuture.get(), std::future_error); // broken promise
    ASSERT_TRUE(thread.Active());
    try {
        thread.Execute<void>([] (coral::net::Reactor&, std::promise<void>) { });
        ADD_FAILURE();
    } catch (const coral::async::CommThreadDead& e) {
        EXPECT_THROW(
            std::rethrow_exception(e.OriginalException()),
            std::underflow_error);
    } catch (...) {
        ADD_FAILURE();
    }
    EXPECT_FALSE(thread.Active());

    // Rvalue construction + unexpected delayed thread death during execution
    thread = coral::async::CommThread<void>();
    EXPECT_NO_THROW(thread.Execute<void>(
        [] (coral::net::Reactor& reactor, std::promise<void> promise)
        {
            // Throw an exception after 100 ms
            reactor.AddTimer(
                std::chrono::milliseconds(100),
                1,
                [] (coral::net::Reactor&, int) { throw std::overflow_error{""}; });
            promise.set_value();
        }).get());
    ASSERT_TRUE(thread.Active());
    auto thread2 = std::move(thread);
    EXPECT_FALSE(thread.Active());
    ASSERT_TRUE(thread2.Active());
    std::this_thread::sleep_for(std::chrono::milliseconds(200)); // time to die
    try {
        thread2.Execute<void>([] (coral::net::Reactor&, std::promise<void>) { });
        ADD_FAILURE();
    } catch (const coral::async::CommThreadDead& e) {
        EXPECT_THROW(
            std::rethrow_exception(e.OriginalException()),
            std::overflow_error);
    } catch (...) {
        ADD_FAILURE();
    }
    ASSERT_FALSE(thread2.Active());

    // Attempts to call functions on an inactive thread object
    EXPECT_THROW(
        thread2.Execute<void>([] (coral::net::Reactor&, std::promise<void>) { }),
        std::logic_error);
    EXPECT_THROW(thread2.Shutdown(), std::logic_error);
}


namespace
{
    struct BadData
    {
        BadData() { throw std::domain_error{""}; }
    };
}


TEST(coral_async, CommThread_BadData)
{
    auto thread = coral::async::CommThread<BadData>{};
    ASSERT_TRUE(thread.Active());
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // time to die
    try {
        thread.Execute<void>([] (coral::net::Reactor&, BadData&, std::promise<void>) { });
        ADD_FAILURE();
    } catch (const coral::async::CommThreadDead& e) {
        EXPECT_THROW(
            std::rethrow_exception(e.OriginalException()),
            std::domain_error);
    } catch (...) {
        ADD_FAILURE();
    }
    EXPECT_FALSE(thread.Active());
}
