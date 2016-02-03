/**
\file
\brief Defines the dsb::bus::DomainData class.
*/
#ifndef DSB_BUS_DOMAIN_DATA_HPP
#define DSB_BUS_DOMAIN_DATA_HPP

#include <chrono>
#include <cstdint>
#include <map>
#include <vector>
#include <string>

#include "boost/range/sub_range.hpp"

#include "model.pb.h"


namespace dsb
{
namespace bus
{


/**
\brief A cache for the information received about the domain.

This class is used by the master to store information it receives about
the domain to which it is connected, for example the discovered slave providers
and the slave types they offer, as well as the last time we heard something
from a slave provider.
*/
class DomainData
{
    typedef std::map<
            std::string,
            std::vector<dsbproto::model::SlaveTypeDescription>
        > SlaveTypesByProviderContainer;

public:
    typedef boost::sub_range<const SlaveTypesByProviderContainer>
        SlaveTypesByProviderRange;

    /**
    \brief Constructor.

    \param [in] maxProtocol
        The maximum supported version of the domain protocol.
        See SlaveProviderProtocol() for more information.
    \param [in] slaveProviderTimeout
        An expiry time for cached slave provider data.
        See PurgeSlaveProviders() for more information.
    */
    DomainData(
        uint16_t maxProtocol,
        std::chrono::milliseconds slaveProviderTimeout);

    /// The number of known slave providers.
    size_t SlaveProviderCount() const;

    /**
    \brief  Adds a new slave provider or updates the "last seen" time of a
            known slave provider.

    This function is typically called upon receiving a heartbeat from a slave
    provider.

    \param [in] id              The slave provider ID.
    \param [in] protocol        The maximum protocol supported by the SP.
    \param [in] heartbeatTime   The time point at which we received the last
                                heartbeat from this SP.

    \returns `true` if this slave provider was *added*, `false` if it existed
        in the cache already and was merely updated.
    */
    bool UpdateSlaveProvider(
        const std::string& id,
        uint16_t protocol,
        std::chrono::steady_clock::time_point heartbeatTime);

    /**
    \brief  Removes information about "dead" slave providers from the cache.

    This function will remove all information about slave providers whose
    last heartbeat was received more than a certain duration before the given
    reference time.  This timeout is determined by the `slaveProviderTimeout`
    argument passed to the constructor.  The last heartbeat time is set by
    UpdateSlaveProvider().
    */
    void PurgeSlaveProviders(
        std::chrono::steady_clock::time_point referenceTime);

    /**
    \brief  Returns a list of all slave providers and the information known
            about them.

    This function returns a range whose elements are of type
    `std::pair<std::string,std::vector<dsbproto::model::SlaveTypeDescription>>`,
    where the `first` field of each pair is the slave provider ID and the
    `second` field is a list of the slaves it provides.

    \warning    This is quite ugly, and very much subject to future change.
    */
    SlaveTypesByProviderRange SlaveTypesByProvider() const;

    /// Updates the list of slave types offered by a specific slave provider.
    void UpdateSlaveTypes(
        const std::string& slaveProviderId,
        std::vector<dsbproto::model::SlaveTypeDescription> slaveTypes);

    /**
    \brief  Returns the protocol version we use to communicate with the given
            slave provider.

    This function takes into account the maximum version supported by the master
    (provided as `maxProtocol` to the constructor) and the slave provider
    (set by UpdateSlaveProvider()) and returns the smaller of the two.

    \throws std::out_of_range if `slaveProviderId` is unknown.
    */
    uint16_t SlaveProviderProtocol(const std::string& slaveProviderId) const;

    //TODO: Just for debugging purposes, remove later.
    void Dump() const;

private:
    struct SlaveProvider
    {
        uint16_t protocol;
        std::chrono::steady_clock::time_point lastHeartbeat;
    };

    uint16_t m_maxProtocol;
    std::chrono::milliseconds m_slaveProviderTimeout;

    std::map<std::string, SlaveProvider> m_slaveProviders;
    // Slave types, mapped to slave provider IDs.
    SlaveTypesByProviderContainer m_slaveTypes;
};


}}      // namespace
#endif  // header guard
