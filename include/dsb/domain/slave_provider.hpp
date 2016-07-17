/**
\file
\brief Slave provider functionality.
*/
#ifndef DSB_DOMAIN_SLAVE_PROVIDER_HPP
#define DSB_DOMAIN_SLAVE_PROVIDER_HPP

#include <chrono>
#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "dsb/config.h"
#include "dsb/model.hpp"
#include "dsb/net.hpp"


// Forward declaration to avoid dependency on ZMQ headers
namespace zmq { class socket_t; }


namespace dsb
{
namespace domain
{


/// An interface for classes that represent slave types.
class ISlaveType
// TODO: Rename this to ISlaveFactory or similar?
{
public:
    /// A description of this slave type.
    virtual const dsb::model::SlaveTypeDescription& Description() const = 0;

    /**
    \brief  Creates a new instance of this slave type.

    This function must report whether a slave was successfully instantiated.
    For example, the slave may represent a particular piece of hardware (e.g.
    a human interface device), of which there is only one.  The function would
    then return `false` if multiple instantiations are attempted.

    If the function returns `true`, it must also update `slaveLocator` with
    information about the new slave.  `slaveLocator.Endpoint()` may then have
    one of three forms:

      1. "Normal", i.e. `transport://address`
      2. Empty, which means that the slave is accessible through the same
         endpoint as the slave provider (typically a proxy), except of course
         with a different identity.
      3. Only a port specification starting with a colon, e.g. `:12345`.
         This may be used if the slave provider is bound to a TCP endpoint,
         and the slave is accessible on the same hostname but with a different
         port number.

    If the function returns `false`, InstantiationFailureDescription() must
    return a textual description of the reasons for this.  `slaveLocator` must
    then be left untouched.

    \param [in] timeout
        How long the master will wait for the slave to start up.  If possible,
        instantiation should be aborted and considered "failed" after this
        time has passed.
    \param [out] slaveLocator
        An object that describes how to connect to the slave.  See the list
        above for different endpoint formats.

    \returns `true` if a slave was successfully instantiated, `false` otherwise.
    */
    virtual bool Instantiate(
        std::chrono::milliseconds timeout,
        dsb::net::SlaveLocator& slaveLocator) = 0;

    /**
    \brief  A textual description of why a previous Instantiate() call failed.

    This function is only called if Instantiate() has returned `false`.
    */
    virtual std::string InstantiationFailureDescription() const = 0;

    // Virtual destructor to allow deletion through base class reference.
    virtual ~ISlaveType() { }
};


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
        std::vector<std::unique_ptr<dsb::domain::ISlaveType>>&& slaveTypes,
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
