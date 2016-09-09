/**
\file
\brief Defines the dsb::provider::SlaveProvider class and related functionality.
*/
#ifndef DSB_PROVIDER_PROVIDER_HPP_INCLUDED
#define DSB_PROVIDER_PROVIDER_HPP_INCLUDED

#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "dsb/config.h"
#include "dsb/provider/slave_creator.hpp"


// Forward declaration to avoid dependency on ZMQ headers
namespace zmq { class socket_t; }


namespace dsb
{
namespace provider
{


/// A slave provider that runs in a background thread.
class SlaveProvider
{
public:
    /**
    \brief  Creates a background thread and runs a slave provider in it.

    \param [in] slaveProviderID
        A string which is used to uniquely identify the slave provider.
        Expect trouble if two slave providers have the same ID.
    \param [in] slaveTypes
        The slave types offered by the slave provider.
    \param [in] networkInterface
        The name or IP address (in dot-decimal format) of the network
        interface that should be used, or "*" for all available interfaces.
    \param [in] discoveryPort
        The UDP port used by others to discover this slave provider.
    \param [in] exceptionHandler
        A function that will be called if an exception is thrown in the
        background thread.  If no handler is provided, or if the handler itself
        throws, std::terminate() will be called.  If the handler returns without
        throwing, the background thread will simply terminate.  (In this case,
        it is still necessary to call Stop() in the foreground thread before
        the SlaveProvider object is destroyed.)
        Note that the exception handler will be called *in* the background
        thread, so care should be taken not to implement it in a thread-unsafe
        manner.
    */
    SlaveProvider(
        const std::string& slaveProviderID,
        std::vector<std::unique_ptr<SlaveCreator>>&& slaveTypes,
        const std::string& networkInterface,
        std::uint16_t discoveryPort,
        std::function<void(std::exception_ptr)> exceptionHandler = nullptr);

    SlaveProvider(const SlaveProvider&) = delete;
    SlaveProvider& operator=(const SlaveProvider&) = delete;

    DSB_DEFINE_DEFAULT_MOVE(SlaveProvider, m_killSocket, m_thread);

    /**
    \brief  Destructs the SlaveProvider object; requires that Stop() has been
            called first.

    If the background thread has not been terminated with Stop() when the
    destructor runs, std::terminate() is called.
    */
    ~SlaveProvider() DSB_NOEXCEPT;

    /**
    \brief  Stops the slave provider.

    This will send a signal to the background thread that triggers a shutdown
    of the slave provider.  The function blocks until the background thread has
    terminated.
    */
    void Stop();

private:
    std::unique_ptr<zmq::socket_t> m_killSocket;
    std::thread m_thread;
};


}}      // namespace
#endif  // header guard
