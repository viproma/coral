#include "dsb/async.hpp"

#include <thread>

#include "dsb/comm/util.hpp"
#include "dsb/log.hpp"
#include "dsb/util.hpp"

namespace dsb
{
namespace async
{

// =============================================================================
// CommThread
// =============================================================================

namespace
{
    // Note: The shared pointers are needed because VS2013 has a bug that
    // prevents the use of move-only objects as arguments to the thread
    // function.
    void BackgroundThread(
        std::shared_ptr<zmq::socket_t> backSocket,
        std::shared_ptr<std::promise<void>> statusNotifier,
        std::function<void(dsb::comm::Reactor&)> init,
        std::function<void(dsb::comm::Reactor&)>* sharedTask)
    {
        try {
            dsb::comm::Reactor reactor;

            // Add a handler for task notification messages
            reactor.AddSocket(
                *backSocket,
                [sharedTask] (dsb::comm::Reactor& r, zmq::socket_t& s) {
                    char dummy;
                    s.recv(&dummy, 1);

                    // Now, the foreground thread is blocked waiting for our
                    // reply, so we can freely access `sharedTask`.
                    auto myTask = std::function<void(dsb::comm::Reactor&)>{};
                    swap(*sharedTask, myTask);

                    // Unblock foreground thread again and then run the task.
                    s.send("", 0);
                    myTask(r);
                });

            // Run the initialisation procedure and then the reactor
            if (init) init(reactor);
            reactor.Run();

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
        backSocket->send("", 0);
    }
}


CommThread::CommThread(std::function<void(dsb::comm::Reactor&)> init)
    : m_active{true}
    , m_socket{dsb::comm::GlobalContext(), ZMQ_PAIR}
    , m_threadStatus{}
    , m_sharedTask{std::make_unique<std::function<void(dsb::comm::Reactor&)>>()}
{
    auto bgSocket = std::make_shared<zmq::socket_t>(
        dsb::comm::GlobalContext(),
        ZMQ_PAIR);
    const auto endpoint = "inproc://" + dsb::util::RandomUUID();
    bgSocket->bind(endpoint);
    m_socket.connect(endpoint);

    auto statusNotifier = std::make_shared<std::promise<void>>();
    m_threadStatus = statusNotifier->get_future();

    std::thread{&BackgroundThread,
            std::move(bgSocket),
            std::move(statusNotifier),
            std::move(init),
            m_sharedTask.get()
        }.detach();
}


CommThread::~CommThread() DSB_NOEXCEPT
{
    DestroySilently();
}


CommThread::CommThread(CommThread&& other) DSB_NOEXCEPT
    : m_active{other.m_active}
    , m_socket{std::move(other.m_socket)}
    , m_threadStatus{std::move(other.m_threadStatus)}
    , m_sharedTask{std::move(other.m_sharedTask)}
{
    other.m_active = false;
}


CommThread& CommThread::operator=(CommThread&& other) DSB_NOEXCEPT
{
    DestroySilently();

    m_active = other.m_active;
    m_socket = std::move(other.m_socket);
    m_threadStatus = std::move(other.m_threadStatus);
    m_sharedTask = std::move(other.m_sharedTask);
    other.m_active = false;

    return *this;
}


void CommThread::Shutdown()
{
    Execute<void>([] (dsb::comm::Reactor& r, std::promise<void> p)
    {
        r.Stop();
        p.set_value();
    }).get();
    m_active = false;
    try {
        m_threadStatus.get();
    } catch (...) {
        throw CommThreadDead(std::current_exception());
    }
}


bool CommThread::Active() const DSB_NOEXCEPT
{
    return m_active;
}


void CommThread::CheckThreadStatus()
{
    try {
        if (m_threadStatus.wait_for(std::chrono::seconds(0))
                == std::future_status::ready) {
            m_threadStatus.get();
            assert(!"Background communication thread has terminated silently");
        }
    } catch (...) {
        m_active = false;
        throw CommThreadDead(std::current_exception());
    }
}


void CommThread::DestroySilently() DSB_NOEXCEPT
{
    if (Active()) {
        try {
            Shutdown();
        } catch (...) {
            DSB_LOG_DEBUG("Unexpected exception thrown in CommThread destructor");
            // ok, not 100% silently then
        }
    }
}


// =============================================================================
// CommThreadDead
// =============================================================================


CommThreadDead::CommThreadDead(std::exception_ptr originalException) DSB_NOEXCEPT
    : m_originalException{originalException}
{
    assert(m_originalException);
}


std::exception_ptr CommThreadDead::OriginalException() const DSB_NOEXCEPT
{
    return m_originalException;
}


const char* CommThreadDead::what() const DSB_NOEXCEPT
{
    return "Background communication thread terminated due to an exception";
}


}}
