/**
\file
\brief Defines the coral::provider::SlaveProvider class and related functionality.
\copyright
    Copyright 2013-present, SINTEF Ocean.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifndef CORAL_PROVIDER_PROVIDER_HPP_INCLUDED
#define CORAL_PROVIDER_PROVIDER_HPP_INCLUDED

#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <coral/config.h>
#include <coral/provider/slave_creator.hpp>


// Forward declaration to avoid dependency on ZMQ headers
namespace zmq { class socket_t; }


namespace coral
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
        const coral::net::ip::Address& networkInterface,
        coral::net::ip::Port discoveryPort,
        std::function<void(std::exception_ptr)> exceptionHandler = nullptr);

    SlaveProvider(const SlaveProvider&) = delete;
    SlaveProvider& operator=(const SlaveProvider&) = delete;

    CORAL_DEFINE_DEFAULT_MOVE(SlaveProvider, m_killSocket, m_thread);

    /**
    \brief  Destructs the SlaveProvider object; requires that Stop() has been
            called first.

    If the background thread has not been terminated with Stop() when the
    destructor runs, std::terminate() is called.
    */
    ~SlaveProvider() noexcept;

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
