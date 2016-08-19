/**
\file
\brief  Module header for dsb::async
*/
#ifndef DSB_ASYNC_HPP
#define DSB_ASYNC_HPP

#include <cassert>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <utility>

#include "zmq.hpp"

#include "dsb/config.h"
#include "dsb/comm/reactor.hpp"
#include "dsb/error.hpp"


namespace dsb
{
/// Facilities for asynchronous function execution
namespace async
{


/**
\brief  Creates and controls a background communications thread.

On construction, an object of this class will create a new thread whose
lifetime is tied to that of the object.  (The destructor will wait for the
background thread to complete before returning.)  This thread can be used
to execute arbitrary code, but its primary design purpose is to run
event-based communications code based on dsb::comm::Reactor.  The thread
therefore has a dedicated reactor object which is passed to all functions
that are executed in it.

To execute functions in the background thread, use the Execute() method.
Return values and exceptions from such functions should be transferred to
the foreground thread using the std::future / std::promise mechanism.

Any exceptions that are *thrown* in the background thread (as opposed to
being reported using a promise) will cause the thread to terminate.
This is true for exceptions that escape the initialisation routine,
exceptions that escape functions which are executed with Execute(),
and exceptions thrown in code which is executed indirectly as a
result of reactor events.
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
*/
class CommThread
{
public:
    /**
    \brief  Creates the background thread and optionally executes an
            initialisation routine in it.

    The initialisation routine will typically be used to register sockets,
    timers, etc. with the reactor, a reference to which is passed as an
    argument to the `init` function.  If the `init` function throws, a
    subsequent call to Execute() or Shutdown() will throw a CommThreadDead
    exception, and the CommThread object is no longer usable.

    \param [in] init
        An initialisation function, or an empty function object if no
        initialisation is needed.

    \post
        `Active() == true`
    */
    explicit CommThread(std::function<void(dsb::comm::Reactor&)> init
        = std::function<void(dsb::comm::Reactor&)>{});

    /**
    \brief  If the CommThread object is still active, shuts down the
            background thread and waits for it to terminate.

    The destructor calls Shutdown() to terminate the thread, but it will
    silently ignore any  exceptions thrown by that function.
    Therefore, it is usually recommended to call Shutdown() manually
    before destruction, in order to catch any errors that might have
    occurred in the background thread.
    */
    ~CommThread() DSB_NOEXCEPT;

    // Copying is disabled
    CommThread(const CommThread&) = delete;
    CommThread& operator=(const CommThread&) = delete;

    /**
    \brief Move constructor.
    \post `other.Active() == false`
    */
    CommThread(CommThread&& other) DSB_NOEXCEPT;

    /**
    \brief Move assignment operator.
    \post `other.Active() == false`
    */
    CommThread& operator=(CommThread&& other) DSB_NOEXCEPT;

    /**
    \brief  Executes a task asynchronously in the background thread.

    This function returns an std::future that shares its state with the
    std::promise object passed to the `task` function.  The promise should
    be used to report results or errors.  This may be done immediately,
    in the body of the `task` function, or it may be done at a later
    time by registering a reactor event.

    A reference to the background thread's dsb::comm::Reactor object is
    also passed to `task`.  Do *not* call dsb::comm::Reactor::Stop() on
    this object to terminate the thread; this will lead to unspecified
    behaviour.  Instead, use Shutdown() to terminate the thread in a
    controlled manner.

    Any exceptions that are thrown and allowed to escape from `task`
    will cause the background thread to terminate, and a subsequent
    call to Execute() or Shutdown() will throw a CommThreadDead exception,
    rendering the CommThread object in an inactive state.
    
    \tparam R
        The type of the function's "return value".  The function will
        receive an object of type `std::promise<R>` which it can use
        to return the value (or an exception).  `R` may be `void`,
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
    template<typename R>
    std::future<R> Execute(
        std::function<void(dsb::comm::Reactor&, std::promise<R>)> task);

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
    bool Active() const DSB_NOEXCEPT;

private:
    void CheckThreadStatus();
    void DestroySilently() DSB_NOEXCEPT;

    bool m_active;
    zmq::socket_t m_socket;
    std::future<void> m_threadStatus;
    std::unique_ptr<std::function<void(dsb::comm::Reactor&)>> m_sharedTask;
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
    CommThreadDead(std::exception_ptr originalException) DSB_NOEXCEPT;

    /// Returns a pointer to the exception that caused the thread to terminate
    std::exception_ptr OriginalException() const DSB_NOEXCEPT;

    /// Returns a generic error message saying that the background thread died
    const char* what() const DSB_NOEXCEPT override;

private:
    std::exception_ptr m_originalException;
};


// =============================================================================
// Templates
// =============================================================================

template<typename R>
std::future<R> CommThread::Execute(
    std::function<void(dsb::comm::Reactor&, std::promise<R>)> task)
{
    DSB_PRECONDITION_CHECK(Active());
    DSB_INPUT_CHECK(task);
    CheckThreadStatus();

    auto promise = std::make_shared<std::promise<R>>();
    auto future = promise->get_future();

    assert(!(*m_sharedTask));
    *m_sharedTask = [task, promise] (dsb::comm::Reactor& reactor)
    {
        task(reactor, std::move(*promise));
    };

    // Notify background thread that a task is ready and wait for it
    // to acknowledge it.
    m_socket.send("", 0);
    char dummy;
    m_socket.recv(&dummy, 1);
    assert(!(*m_sharedTask));

    return future;
}


}} // namespace
#endif // header guard
