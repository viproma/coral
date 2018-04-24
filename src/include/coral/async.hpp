/**
\file
\brief  Module header for coral::async
\copyright
    Copyright 2013-2017, SINTEF Ocean and the Coral contributors.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifndef CORAL_ASYNC_HPP
#define CORAL_ASYNC_HPP

#include <cassert>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <thread>
#include <utility>

#include <zmq.hpp>

#include <coral/config.h>
#include <coral/error.hpp>
#include <coral/log.hpp>
#include <coral/net/reactor.hpp>
#include <coral/net/zmqx.hpp>
#include <coral/util.hpp>


namespace coral
{
/// Facilities for asynchronous function execution
namespace async
{


/**
\brief  Contains the Type member alias, which defines the signature for
        functions executed asynchronously by CommThread.

\tparam StackData
    The type used for the `StackData` parameter of the CommThread template.
\tparam Result
    The type of value returned by the task function (by means of the
    future/promise mechanism).
*/
template<typename StackData, typename Result>
struct CommThreadTask
{
    /**
    \brief  An std::function specialisation which defines the signature
            for functions executed asynchronously by CommThread.

    If `StackData` is not `void`, the signature is defined as follows:
    ~~~{.cpp}
    void fun(
        coral::net::Reactor& reactor,
        StackData& data,
        std::promise<Result> promise);
    ~~~
    And if `StackData` is `void`, the signature is defined like this:
    ~~~{.cpp}
    void fun(
        coral::net::Reactor& reactor,
        std::promise<Result> promise);
    ~~~
    Here, `reactor` and `data` are the dedicated coral::net::Reactor and
    `StackData` objects associated with the background thread, respectively,
    and `promise` is the std::promise object which the task function should
    use to return its result (or throw an exception).
    */
    using Type = std::function<void(
        coral::net::Reactor&,
        StackData&,
        std::promise<Result>)>;
};

// Specialisation of the above for `StackData = void`.
template<typename Result>
struct CommThreadTask<void, Result>
{
    using Type = std::function<void(
        coral::net::Reactor&,
        std::promise<Result>)>;
};


namespace detail
{
    template<typename StackData>
    struct CommThreadAnyTask
    {
        using Type = std::function<void(coral::net::Reactor&, StackData&)>;
        using SharedPtr = std::shared_ptr<Type>;
        using WeakPtr = std::weak_ptr<Type>;
    };

    template<>
    struct CommThreadAnyTask<void>
    {
        using Type = std::function<void(coral::net::Reactor&)>;
        using SharedPtr = std::shared_ptr<Type>;
        using WeakPtr = std::weak_ptr<Type>;
    };
} // namespace detail


/**
\brief  Creates and controls a background communications thread.

The constructor of this class creates a new thread whose lifetime is tied
to that of the constructed object (i.e., the destructor will wait for the
background thread to complete before returning.)  This thread can be used
to execute arbitrary code, but its primary design purpose is to run
event-based communications code based on coral::net::Reactor.  The thread
therefore has a dedicated `Reactor` object, a reference to which is passed
to all functions that are executed in it.

To execute tasks in the background thread, use the Execute() method.
Results and exceptions from such functions should be transferred to
the foreground thread using the std::future / std::promise mechanism.

The background thread may have a dedicated object of type `StackData`.
This is located on that thread's stack, and can be used to hold objects
persistently across Execute() calls.  The `StackData` object does
not move in memory, and its lifetime ends before that of the `Reactor`.
A reference to this object is passed to each function that is executed
in the background thread.  `StackData` may be `void`, in which case no
object is ever created, and no reference is passed to the executed
functions.

Any exceptions that are *thrown* in the background thread (as opposed to
being reported using a promise) will cause the thread to terminate.
This is true both for exceptions that escape functions which are executed
with Execute() and for those thrown in code which is executed indirectly as
a result of reactor events.
If this happens, a subsequent call to Execute() or Shutdown() will throw
a CommThreadDead exception.  The original exception can be obtained from
this object.

After construction, the CommThread object is in the "active" state,
where Active() returns `true`.  It remains in this state until one
of the following happens, after which it is "inactive" and Active()
returns `false`:

  - Shutdown() is called to perform a controlled shutdown of the
    background thread.

  - Execute() or Shutdown() throws CommThreadDead because the background
    thread has terminated due to an unexpected exception.

  - The object is moved from, i.e., it is used as the source for a move
    construction or move assignment operation.

\tparam StackData
    The type used for an object that holds stack data persistently
    across function calls in the background thread, or `void` if
    no such object is needed.
*/
template<typename StackData>
class CommThread
{
public:
    /**
    \brief  Creates the background thread.
    \post `Active() == true`
    */
    explicit CommThread();

    /**
    \brief  If the CommThread object is still active, shuts down the
            background thread and waits for it to terminate.

    The destructor calls Shutdown() to terminate the thread, but it will
    silently ignore any  exceptions thrown by that function.
    Therefore, it is usually recommended to call Shutdown() manually
    before destruction, in order to catch any errors that might have
    occurred in the background thread.
    */
    ~CommThread() noexcept;

    // Copying is disabled
    CommThread(const CommThread&) = delete;
    CommThread& operator=(const CommThread&) = delete;

    /**
    \brief Move constructor.
    \post `other.Active() == false`
    */
    CommThread(CommThread&& other) noexcept;

    /**
    \brief Move assignment operator.
    \post `other.Active() == false`
    */
    CommThread& operator=(CommThread&& other) noexcept;

    /**
    \brief  Executes a task asynchronously in the background thread.

    This function returns a std::future that shares its state with the
    std::promise object passed to the `task` function.  The promise should
    be used to report results or errors.  This may be done immediately,
    in the body of the `task` function, or it may be done at a later
    time by registering a reactor event.

    A reference to the background thread's coral::net::Reactor object is
    also passed to `task`.  Do *not* call coral::net::Reactor::Stop() on
    this object to terminate the thread; this will lead to unspecified
    behaviour.  Instead, use Shutdown() to terminate the thread in a
    controlled manner.

    Finally, if the type parameter `StackData` is not `void`, the
    function also receives a reference to an object of the given type.
    This object is located on the background thread's stack, and thus
    persists across function calls.

    See CommThreadTask for more information about the type and signature
    of the `task` function.

    Any exceptions that are thrown and allowed to escape from `task`
    will cause the background thread to terminate, and a subsequent
    call to Execute() or Shutdown() will throw a CommThreadDead exception,
    rendering the CommThread object in an inactive state.

    \warning
        Visual Studio versions prior to 2015 do not handle std::promise
        destruction correctly.  Specifically, a destroyed std::promise
        does not cause a "broken promise" error to be thrown by its
        corresponding `std::future`.  Thus, if an exception is thrown in
        the background thread when there are any unfulfilled promises,
        calls to std::future::get() will block indefinitely.

    \tparam Result
        The type of the function's "return value".  The function will
        receive an object of type `std::promise<Result>` which it can
        use to return the value (or an exception).  `Result` may be `void`,
        in which case the promise is simply used to report whether
        the task was completed successfully.
    \param [in] task
        A function to be executed in the background thread.
    \returns
        A std::future object that shares state with the std::promise
        passed to the `task` function.
    \throws CommThreadDead
        If the background thread has terminated unexpectedly due to
        an exception.
    \pre
        `Active() == true`
    */
    template<typename Result>
    std::future<Result> Execute(
        typename CommThreadTask<StackData, Result>::Type task);

    /**
    \brief  Terminates the background thread in a controlled manner.

    This function will block until the background thread has terminated.
    After it returns, the CommThread object will be in the "inactive" state.

    \throws CommThreadDead
        If the background thread has terminated unexpectedly due to an
        exception, either before this function was called or during the
        shutdown procedure.  (The latter should usually not happen, as
        destructors are supposed to be `noexcept`.)
    \pre
        `Active() == true`
    \post
        `Active() == false`
    */
    void Shutdown();

    /**
    \brief  Returns whether the CommThread object is active.

    If this function returns `false`, there is no background thread associated
    with this object.  It may have been terminated, or ownership of it may have
    been transferred to another CommThread object by a move operation.

    The converse is not necessarily true, however:  Even if Active() returns
    `true`, the thread may still have terminated due to an exception.
    The only way to discover whether this is the case is to attempt to
    run Execute() or Shutdown() and see if CommThreadDead is thrown.
    If this happens, Active() will return `false` afterwards.
    */
    bool Active() const noexcept;

private:
    // Waits for the background thread to terminate and performs cleanup
    void WaitForThreadTermination();
    void DestroySilently() noexcept;

    bool m_active;
    zmq::socket_t m_socket;
    std::future<void> m_threadStatus;
    typename detail::CommThreadAnyTask<StackData>::WeakPtr m_nextTask;
};


/**
\brief  An exception that signals an error that has caused CommThread's
        background thread to terminate unexpectedly.

The original exception that caused the thread to terminate can be obtained
through the OriginalException() method.
*/
class CommThreadDead : public std::exception
{
public:
    /// Constructor
    CommThreadDead(std::exception_ptr originalException) noexcept;

    /// Returns a pointer to the exception that caused the thread to terminate
    std::exception_ptr OriginalException() const noexcept;

    /// Returns a generic error message saying that the background thread died
    const char* what() const noexcept override;

private:
    std::exception_ptr m_originalException;
};


// =============================================================================
// Templates
// =============================================================================

namespace detail
{
    template<typename StackData>
    void CommThreadMessagingLoop(
        zmq::socket_t& bgSocket,
        typename CommThreadAnyTask<StackData>::SharedPtr nextTask)
    {
        coral::net::Reactor reactor;
        StackData data;
        reactor.AddSocket(
            bgSocket,
            [nextTask, &data] (coral::net::Reactor& r, zmq::socket_t& s) {
                char dummy;
                s.recv(&dummy, 1);

                // Now, the foreground thread is blocked waiting for our
                // reply, so we can freely access `nextTask`.
                typename CommThreadAnyTask<StackData>::Type myTask;
                swap(*nextTask, myTask);

                // Unblock foreground thread again and then run the task.
                s.send("", 0);
                myTask(r, data);
            });
        reactor.Run();
    }

    template<>
    inline void CommThreadMessagingLoop<void>(
        zmq::socket_t& bgSocket,
        typename CommThreadAnyTask<void>::SharedPtr nextTask)
    {
        coral::net::Reactor reactor;
        reactor.AddSocket(
            bgSocket,
            [nextTask] (coral::net::Reactor& r, zmq::socket_t& s) {
                char dummy;
                s.recv(&dummy, 1);

                // Now, the foreground thread is blocked waiting for our
                // reply, so we can freely access `nextTask`.
                CommThreadAnyTask<void>::Type myTask;
                swap(*nextTask, myTask);

                // Unblock foreground thread again and then run the task.
                s.send("", 0);
                myTask(r);
            });
        reactor.Run();
    }

    // Note: Some of the parameters are only shared_ptr because VS2013 has
    // a bug that prevents the use of move-only objects as arguments to the
    // thread function.
    template<typename StackData>
    void CommThreadBackground(
        std::shared_ptr<zmq::socket_t> bgSocket,
        std::shared_ptr<std::promise<void>> statusNotifier,
        typename CommThreadAnyTask<StackData>::SharedPtr nextTask)
        noexcept
    {
        try {
            CommThreadMessagingLoop<StackData>(
                *bgSocket,
                std::move(nextTask));

            // We should possibly use set_value_at_thread_exit() and
            // set_exception_at_thread_exit() in the following, but those are
            // not supported in GCC 4.9.
            statusNotifier->set_value();
        } catch (...) {
            statusNotifier->set_exception(
                std::current_exception());
        }

        // This is to avoid the potential race condition where the background
        // thread dies after the foreground thread has sent a task notification
        // and is waiting to receive an acknowledgement.
        bgSocket->send("", 0);
    }
} // namespace detail


template<typename StackData>
CommThread<StackData>::CommThread()
    : m_active{true}
    , m_socket{coral::net::zmqx::GlobalContext(), ZMQ_PAIR}
    , m_threadStatus{}
    , m_nextTask{}
{
    auto bgSocket =
        std::make_shared<zmq::socket_t>(coral::net::zmqx::GlobalContext(), ZMQ_PAIR);
    bgSocket->setsockopt(ZMQ_LINGER, 0);
    m_socket.setsockopt(ZMQ_LINGER, 0);
    const auto endpoint = "inproc://" + coral::util::RandomUUID();
    bgSocket->bind(endpoint);
    m_socket.connect(endpoint);

    auto statusNotifier = std::make_shared<std::promise<void>>();
    m_threadStatus = statusNotifier->get_future();

    auto sharedTask =
        std::make_shared<typename detail::CommThreadAnyTask<StackData>::Type>(/*empty*/);
    m_nextTask = sharedTask;

    std::thread{&detail::CommThreadBackground<StackData>,
        bgSocket, statusNotifier, sharedTask}.detach();
}


template<typename StackData>
CommThread<StackData>::~CommThread() noexcept
{
    DestroySilently();
}


template<typename StackData>
CommThread<StackData>::CommThread(CommThread&& other) noexcept
    : m_active{other.m_active}
    , m_socket{std::move(other.m_socket)}
    , m_threadStatus{std::move(other.m_threadStatus)}
    , m_nextTask{std::move(other.m_nextTask)}
{
    other.m_active = false;
}


template<typename StackData>
CommThread<StackData>& CommThread<StackData>::operator=(CommThread&& other) noexcept
{
    DestroySilently();
    m_active = other.m_active;
    m_socket = std::move(other.m_socket);
    m_threadStatus = std::move(other.m_threadStatus);
    m_nextTask = std::move(other.m_nextTask);
    other.m_active = false;
    return *this;
}


namespace detail
{
    template<typename StackData, typename Result>
    struct CommThreadFunctions
    {
        static typename CommThreadAnyTask<StackData>::Type WrapTask(
            typename CommThreadTask<StackData, Result>::Type task,
            std::promise<Result> promise)
        {
            const auto sharedPromise =
                std::make_shared<typename std::promise<Result>>(std::move(promise));
            return [task, sharedPromise]
                (coral::net::Reactor& reactor, StackData& data)
            {
                task(reactor, data, std::move(*sharedPromise));
            };
        }
    };


    template<typename Result>
    struct CommThreadFunctions<void, Result>
    {
        static typename CommThreadAnyTask<void>::Type WrapTask(
            typename CommThreadTask<void, Result>::Type task,
            std::promise<Result> promise)
        {
            const auto sharedPromise =
                std::make_shared<typename std::promise<Result>>(std::move(promise));
            return [task, sharedPromise]
                (coral::net::Reactor& reactor)
            {
                task(reactor, std::move(*sharedPromise));
            };
        }
    };
} // namespace detail


template<typename StackData>
template<typename Result>
std::future<Result> CommThread<StackData>::Execute(
    typename CommThreadTask<StackData, Result>::Type task)
{
    CORAL_PRECONDITION_CHECK(Active());
    CORAL_INPUT_CHECK(task);

    auto promise = std::promise<Result>{};
    auto future = promise.get_future();

    if (auto sharedTask = m_nextTask.lock()) {
        assert(!(*sharedTask));
        *sharedTask = detail::CommThreadFunctions<StackData, Result>::WrapTask(
            std::move(task),
            std::move(promise));

        // Notify background thread that a task is ready and wait for it
        // to acknowledge it before returning.
        m_socket.send("", 0);
        char dummy;
        m_socket.recv(&dummy, 1);
        return future;
    } else {
        // The weak_ptr has expired, meaning that the thread (which holds
        // a shared_ptr to the same object) must be dead.
        WaitForThreadTermination();

        // The above function should have thrown.  If it didn't, it probably
        // means that calling code did something stupid.
        coral::log::Log(
            coral::log::error,
            "CommThread background thread has terminated silently and "
            "unexpectedly.  Perhaps Reactor::Stop() was called?");
        std::terminate();
    }
}


namespace detail
{
    inline void CommThreadShutdown(
        coral::net::Reactor& reactor,
        std::promise<void> promise)
    {
        reactor.Stop();
        promise.set_value();
    }

    template<typename StackData>
    typename CommThreadTask<StackData, void>::Type CommThreadShutdownTask()
    {
        return [] (coral::net::Reactor& r, StackData&, std::promise<void> p)
        {
            CommThreadShutdown(r, std::move(p));
        };
    }

    template<>
    inline typename CommThreadTask<void, void>::Type CommThreadShutdownTask<void>()
    {
        return [] (coral::net::Reactor& r, std::promise<void> p)
        {
            CommThreadShutdown(r, std::move(p));
        };
    }
} // namespace detail


template<typename StackData>
void CommThread<StackData>::Shutdown()
{
    Execute<void>(detail::CommThreadShutdownTask<StackData>());
    WaitForThreadTermination();
}


template<typename StackData>
bool CommThread<StackData>::Active() const noexcept
{
    return m_active;
}


template<typename StackData>
void CommThread<StackData>::WaitForThreadTermination()
    {
        assert(m_active);
        assert(m_threadStatus.valid());
        try {
            const auto cleanup = coral::util::OnScopeExit([this] ()
            {
                m_active = false;
                m_socket.close();
#ifdef _MSC_VER
                // Visual Studio does not "reset" the future after get().
                // See:  http://stackoverflow.com/q/33899615
                m_threadStatus = std::future<void>{};
#endif
            });
            m_threadStatus.get();
        }
#ifdef _MSC_VER
        // Visual Studio (versions up to and including 2015, at least)
        // has a bug where std::current_exception() returns a null
        // exception pointer if it is called during stack unwinding.
        // We have to do the best we can in this case, and special-case
        // some well-known exceptions.
        // See: http://stackoverflow.com/q/29652438
        catch (const zmq::error_t& e) {
            if (std::current_exception()) {
                throw CommThreadDead(std::current_exception());
            } else {
                throw CommThreadDead(std::make_exception_ptr(e));
            }
        }
        catch (const std::runtime_error& e) {
            if (std::current_exception()) {
                throw CommThreadDead(std::current_exception());
            } else {
                throw CommThreadDead(std::make_exception_ptr(e));
            }
        }
        catch (const std::logic_error& e) {
            if (std::current_exception()) {
                throw CommThreadDead(std::current_exception());
            } else {
                throw CommThreadDead(std::make_exception_ptr(e));
            }
        }
        catch (const std::exception& e) {
            if (std::current_exception()) {
                throw CommThreadDead(std::current_exception());
            } else {
                throw CommThreadDead(std::make_exception_ptr(
                    std::runtime_error(e.what())));
            }
        }
#else
        catch (...) {
            throw CommThreadDead(std::current_exception());
        }
#endif
    }


#ifdef _MSC_VER
#   pragma warning(push)
#   pragma warning(disable: 4101) // Unreferenced local variable 'e'
#endif

template<typename StackData>
void CommThread<StackData>::DestroySilently() noexcept
{
    if (Active()) {
        try {
            Shutdown();
        } catch (const CommThreadDead& e) {
            try {
                std::rethrow_exception(e.OriginalException());
            } catch (const std::exception& e) {
                CORAL_LOG_DEBUG(
                    boost::format("Unexpected exception thrown in CommThread destructor: %s")
                    % e.what());
            }
        } catch (const std::exception& e) {
            CORAL_LOG_DEBUG(
                boost::format("Unexpected exception thrown in CommThread destructor: %s")
                % e.what());
        }
    }
    assert(!Active());
    assert(!static_cast<void*>(m_socket));
    assert(!m_threadStatus.valid());
}

#ifdef _MSC_VER
#   pragma warning(pop)
#endif


}} // namespace
#endif // header guard
