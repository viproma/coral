#ifndef DSB_BUS_SP_INFO_CLIENT_HPP
#define DSB_BUS_SP_INFO_CLIENT_HPP

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <system_error>

#include "dsb/config.h"
#include "dsb/comm/reactor.hpp"
#include "dsb/model.hpp"
#include "dsb/net.hpp"
#include "dsb/protocol/req_rep.hpp"


namespace dsb
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
        dsb::comm::Reactor& reactor,
        const dsb::net::InetEndpoint& endpoint);

    /// Destructor
    ~SlaveProviderClient() DSB_NOEXCEPT;

    SlaveProviderClient(const SlaveProviderClient&) = delete;
    SlaveProviderClient& operator=(const SlaveProviderClient&) = delete;

    /// Move constructor
    SlaveProviderClient(SlaveProviderClient&&) DSB_NOEXCEPT;

    /// Move assignment operator
    SlaveProviderClient& operator=(SlaveProviderClient&&) DSB_NOEXCEPT;

    /// Completion handler type for GetSlaveTypes().
    typedef std::function<void(
            const std::error_code& error,
            const dsb::model::SlaveTypeDescription* slaveTypes,
            std::size_t slaveTypeCount)>
        GetSlaveTypesHandler;

    /**
    \brief  Requests a list of slave types provided.

    \param [in] onComplete
        Function which is called when the result is ready, or with an error
        code in case of failure.
    \param [in] timeout
        Maximum time allowed for the request to complete.
    */
    void GetSlaveTypes(
        GetSlaveTypesHandler onComplete,
        std::chrono::milliseconds timeout);

    /// Completion handler type for InstantiateSlave().
    typedef std::function<void(
            const std::error_code& ec,
            const dsb::net::SlaveLocator& slaveLocator,
            const std::string& errorMessage)>
        InstantiateSlaveHandler;

    /**
    \brief  Requests the instantiation of a slave.

    \param [in] slaveTypeUUID
        The slave type identifier.
    \param [in] instantiationTimeout
        The max allowed time for the slave to start up.
    \param [in] onComplete
        Function which is called with the slave address when the slave has
        been instantiated, or with an error code and message in case of failure.
    \param [in] requestTimeout
        Maximum time allowed for the request to complete, which must of course
        be greater than `instantiationTimeout`.
    */
    void InstantiateSlave(
        const std::string& slaveTypeUUID,
        std::chrono::milliseconds instantiationTimeout,
        InstantiateSlaveHandler onComplete,
        std::chrono::milliseconds requestTimeout = std::chrono::milliseconds(0));

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
    virtual int GetSlaveTypeCount() const DSB_NOEXCEPT = 0;

    /// Returns a description of the `index`th slave type.
    virtual dsb::model::SlaveTypeDescription GetSlaveType(int index) const = 0;

    /// Instantiates a slave.
    virtual dsb::net::SlaveLocator InstantiateSlave(
        const std::string& slaveTypeUUID,
        std::chrono::milliseconds timeout) = 0;

    virtual ~SlaveProviderOps() DSB_NOEXCEPT { }
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
    dsb::protocol::RRServer& server,
    std::shared_ptr<SlaveProviderOps> slaveProvider);


}} // namespace
#endif // header guard
