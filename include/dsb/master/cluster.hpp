/**
\file
\brief Defines the dsb::master::Cluster class and related functionality.
*/
#ifndef DSB_MASTER_CLUSTER_HPP
#define DSB_MASTER_CLUSTER_HPP

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "dsb/config.h"
#include "dsb/model.hpp"
#include "dsb/net.hpp"


namespace dsb
{
namespace master
{


/**
\brief  A common communication interface to a cluster of slave providers.

This class represents a common interface to several slave providers in a
network.  It can be used to get information about the available slave types
and to instantiate slaves on specific providers.

\remark
When an object of this class is created, it will spawn a background thread that
performs the actual communication with the slave providers.  To ensure
that there is a one-to-one relationship between an object of this class and
its underlying communication thread, the objects are noncopyable (but movable),
and will attempt to shut down the thread on destruction.
*/
class Cluster
{
public:
    /// Information about a slave type.
    struct SlaveType
    {
        dsb::model::SlaveTypeDescription description;
        std::vector<std::string> providers;
    };

    /**
    \brief  Constructor.

    \param [in] networkInterface
        The name or IP address (in dot-decimal format) of the network
        interface that should be used, or "*" for all available interfaces.
    \param [in] discoveryPort
        The UDP port used for discovering other entities such as slave
        providers.
    */
    Cluster(
        const std::string& networkInterface,
        std::uint16_t discoveryPort);

    /// Destructor.
    ~Cluster() DSB_NOEXCEPT;

    // Disable copying
    Cluster(const Cluster&) = delete;
    Cluster& operator=(const Cluster&) = delete;

    /// Move constructor
    Cluster(Cluster&&) DSB_NOEXCEPT;

    /// Move assignment operator
    Cluster& operator=(Cluster&&) DSB_NOEXCEPT;

    /**
    \brief  Returns available slave types.

    \param [in] timeout
        Maximum time to wait for replies from known slave providers.
    */
    std::vector<SlaveType> GetSlaveTypes(std::chrono::milliseconds timeout);

    /**
    \brief  Instantiates a slave.

    `timeout` specifies how long the slave provider should wait for the
    slave to start up before assuming it has crashed or frozen.  The master
    will wait twice as long as this for the slave provider to report that the
    slave has been successfully instantiated before it assumes the slave
    provider itself has crashed or the connection has been lost.
    In both cases, an exception is thrown.

    \param [in] slaveProviderID
        The ID of the slave provider that should instantiate the slave.
    \param [in] slaveTypeUUID
        The UUID that identifies the type of the slave that is to be
        instantiated.
    \param [in] timeout
        How much time the slave gets to start up.

    \returns
        An object that contains the information needed to locate the slave.
    */
    dsb::net::SlaveLocator InstantiateSlave(
        const std::string& slaveProviderID,
        const std::string& slaveTypeUUID,
        std::chrono::milliseconds timeout);

private:
    class Private;
    std::unique_ptr<Private> m_private;
};


}}      //namespace
#endif  // header guard
