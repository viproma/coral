/**
\file
\brief Defines the coral::master::ProviderCluster class and related functionality.
\copyright
    Copyright 2013-present, SINTEF Ocean.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifndef CORAL_MASTER_CLUSTER_HPP
#define CORAL_MASTER_CLUSTER_HPP

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <coral/config.h>
#include <coral/model.hpp>
#include <coral/net.hpp>


namespace coral
{
namespace master
{


/**
 *  \brief
 *  A common communication interface to a cluster of slave providers.
 *
 *  This class represents a common interface to several slave providers in a
 *  network.  It can be used to get information about the available slave types
 *  and to instantiate slaves on specific providers.
 *
 *  Slave providers are discovered automatically by listening for UDP
 *  broadcast messages that they broadcast periodically.
 *
 *  \remark
 *  When an object of this class is created, it will spawn a background thread
 *  that performs the actual communication with the slave providers.  To ensure
 *  that there is a one-to-one relationship between an object of this class and
 *  its underlying communication thread, the objects are noncopyable (but
 *  movable), and will attempt to shut down the thread on destruction.
*/
class ProviderCluster
{
public:
    /// Information about a slave type.
    struct SlaveType
    {
        /// A description of the slave type.
        coral::model::SlaveTypeDescription description;

        /// A list of IDs of slave providers that offer this slave type.
        std::vector<std::string> providers;
    };

    /**
     *  \brief
     *  Constructor.
     *
     *  \param [in] networkInterface
     *      The name or IP address of the network interface that should be used,
     *      or "*" for all available interfaces.
     *  \param [in] discoveryPort
     *      The UDP port used for discovering other entities such as slave
     *      providers.
     */
    ProviderCluster(
        const coral::net::ip::Address& networkInterface,
        coral::net::ip::Port discoveryPort);

    /// Destructor.
    ~ProviderCluster() noexcept;

    // Disable copying
    ProviderCluster(const ProviderCluster&) = delete;
    ProviderCluster& operator=(const ProviderCluster&) = delete;

    /// Move constructor
    ProviderCluster(ProviderCluster&&) noexcept;

    /// Move assignment operator
    ProviderCluster& operator=(ProviderCluster&&) noexcept;

    /**
     *  \brief
     *  Returns the slave types which are offered by all slave providers
     *  discovered so far.
     *
     *  \warning
     *      After an object of this class has been constructed, it may
     *      take some time for it to discover all slave providers.
     *
     *  \param [in] timeout
     *      The communications timeout used to detect loss of communication
     *      with slave providers.  A negative value means no timeout.
    */
    std::vector<SlaveType> GetSlaveTypes(std::chrono::milliseconds timeout);

    /**
     *  \brief
     *  Requests that a slave be spawned by a specific slave provider.
     *
     *  `timeout` specifies how long the slave provider should wait for
     *  the slave to start up before assuming it has crashed or frozen.
     *  The function will wait twice as long as this for the slave provider
     *  to report that the slave has been successfully instantiated before
     *  it assumes that the slave provider itself has crashed or the
     *  connection has been lost.  In both cases, an exception is thrown.
     *
     *  \param [in] slaveProviderID
     *      The ID of the slave provider that should instantiate the slave.
     *  \param [in] slaveTypeUUID
     *      The UUID that identifies the type of the slave that is to be
     *      instantiated.
     *  \param [in] timeout
     *      How much time the slave gets to start up.
     *      A negative value means no limit.
     *
     *  \returns
     *      An object that contains the information needed to connect to
     *      the slave, which can be passed to `Execution::Reconstitute()`.
     */
    coral::net::SlaveLocator InstantiateSlave(
        const std::string& slaveProviderID,
        const std::string& slaveTypeUUID,
        std::chrono::milliseconds timeout);

private:
    class Private;
    std::unique_ptr<Private> m_private;
};


}}      //namespace
#endif  // header guard
