/**
\file
\brief  Slave provider client/server communication classes.
\copyright
    Copyright 2013-2017, SINTEF Ocean and the Coral contributors.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifndef CORAL_BUS_SP_INFO_CLIENT_HPP
#define CORAL_BUS_SP_INFO_CLIENT_HPP

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <system_error>

#include <coral/config.h>
#include <coral/model.hpp>
#include <coral/net.hpp>
#include <coral/net/reactor.hpp>
#include <coral/net/reqrep.hpp>


namespace coral
{
namespace bus
{


/**
\brief  A class for communicating with a single slave provider.
*/
class SlaveProviderClient
{
public:
    /**
    \brief  Constructor.

    \param [in] reactor
        Used to listen for incoming messages from the slave provider.
    \param [in] address
        The IP address or hostname of the slave provider.
    \param [in] port
        The port on which the slave provider is accepting connections.
    */
    SlaveProviderClient(
        coral::net::Reactor& reactor,
        const coral::net::ip::Endpoint& endpoint);

    /// Destructor
    ~SlaveProviderClient() CORAL_NOEXCEPT;

    SlaveProviderClient(const SlaveProviderClient&) = delete;
    SlaveProviderClient& operator=(const SlaveProviderClient&) = delete;

    /// Move constructor
    SlaveProviderClient(SlaveProviderClient&&) CORAL_NOEXCEPT;

    /// Move assignment operator
    SlaveProviderClient& operator=(SlaveProviderClient&&) CORAL_NOEXCEPT;

    /// Completion handler type for GetSlaveTypes().
    typedef std::function<void(
            const std::error_code& error,
            const coral::model::SlaveTypeDescription* slaveTypes,
            std::size_t slaveTypeCount)>
        GetSlaveTypesHandler;

    /**
    \brief  Requests a list of slave types provided.

    \param [in] onComplete
        Function which is called when the result is ready, or with an error
        code in case of failure.
    \param [in] timeout
        Maximum time allowed for the request to complete.
        A negative value means that there is no time limit.
    */
    void GetSlaveTypes(
        GetSlaveTypesHandler onComplete,
        std::chrono::milliseconds timeout);

    /// Completion handler type for InstantiateSlave().
    typedef std::function<void(
            const std::error_code& ec,
            const coral::net::SlaveLocator& slaveLocator,
            const std::string& errorMessage)>
        InstantiateSlaveHandler;

    /**
    \brief  Requests the instantiation of a slave.

    \param [in] slaveTypeUUID
        The slave type identifier.
    \param [in] instantiationTimeout
        The max allowed time for the slave to start up.
        A negative value means that there is no time limit (which is somewhat
        risky, because it means that the entire slave provider will freeze
        if the slave hangs during startup).
    \param [in] requestTimeout
        Additional time allowed for the whole request to complete.
        A negative value means that there is no time limit.
    \param [in] onComplete
        Function which is called with the slave address when the slave has
        been instantiated, or with an error code and message in case of failure.
    */
    void InstantiateSlave(
        const std::string& slaveTypeUUID,
        std::chrono::milliseconds instantiationTimeout,
        std::chrono::milliseconds requestTimeout,
        InstantiateSlaveHandler onComplete);

private:
    class Private;
    std::unique_ptr<Private> m_private;
};


/**
\brief  An interface for the services offered by a slave provider,
        for use with MakeSlaveProviderServer().
*/
class SlaveProviderOps
{
public:
    /// Returns the number of slave types provided.
    virtual int GetSlaveTypeCount() const CORAL_NOEXCEPT = 0;

    /// Returns a description of the `index`th slave type.
    virtual coral::model::SlaveTypeDescription GetSlaveType(int index) const = 0;

    /// Instantiates a slave.
    virtual coral::net::SlaveLocator InstantiateSlave(
        const std::string& slaveTypeUUID,
        std::chrono::milliseconds timeout) = 0;

    virtual ~SlaveProviderOps() CORAL_NOEXCEPT { }
};


/**
\brief  Creates a server to be used by slave providers to handle incoming
        requests from a SlaveProviderClient.

\param [in] server
    The server that will handle the requests.  The function will add an
    appropriate protocol handler to this server.
\param [in] slaveProvider
    The object that will carry out any incoming requests.
*/
void MakeSlaveProviderServer(
    coral::net::reqrep::Server& server,
    std::shared_ptr<SlaveProviderOps> slaveProvider);


}} // namespace
#endif // header guard
