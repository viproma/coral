/**
\file
\brief Functionality for starting and controlling a simulation domain.
*/
#ifndef DSB_DOMAIN_CONTROLLER_HPP
#define DSB_DOMAIN_CONTROLLER_HPP

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
namespace domain
{


/**
\brief  Domain interface.

This class is used to connect to a domain, inquire about the slave providers
available on the domain and the slave types they offer, and instantiate slaves
for executions.

\remark
When an object of this class is created, it will spawn a background thread that
performs the actual communication with other domain participants.  To ensure
that there is a one-to-one relationship between an object of this class and
its underlying communication thread, the objects are noncopyable (but movable),
and will attempt to shut down the thread on destruction.
*/
class Controller
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
    Controller(
        const std::string& networkInterface,
        std::uint16_t discoveryPort);

    /// Destructor.
    ~Controller() DSB_NOEXCEPT;

    // Disable copying
    Controller(const Controller&) = delete;
    Controller& operator=(const Controller&) = delete;

    /// Move constructor
    Controller(Controller&&) DSB_NOEXCEPT;

    /// Move assignment operator
    Controller& operator=(Controller&&) DSB_NOEXCEPT;

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
