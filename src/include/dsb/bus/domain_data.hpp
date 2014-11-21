#ifndef DSB_BUS_DOMAIN_DATA_HPP
#define DSB_BUS_DOMAIN_DATA_HPP

#include <cstdint>
#include <map>
#include <string>

#include "boost/chrono.hpp"
#include "boost/range/sub_range.hpp"

#include "domain.pb.h"


namespace dsb
{
namespace bus
{


class DomainData
{
    typedef std::map<std::string, dsbproto::domain::SlaveTypeList>
        SlaveTypesByProviderContainer;
public:
    typedef boost::sub_range<const SlaveTypesByProviderContainer>
        SlaveTypesByProviderRange;

    DomainData(
        uint16_t maxProtocol,
        boost::chrono::milliseconds slaveProviderTimeout);

    size_t SlaveProviderCount() const;

    bool UpdateSlaveProvider(
        const std::string& id,
        uint16_t protocol,
        boost::chrono::steady_clock::time_point heartbeatTime);

    void PurgeSlaveProviders(
        boost::chrono::steady_clock::time_point referenceTime);

    SlaveTypesByProviderRange SlaveTypesByProvider() const;

    void UpdateSlaveTypes(
        const std::string& slaveProviderId,
        const dsbproto::domain::SlaveTypeList& slaveTypes);

    /**
    \brief  Returns the protocol version we use to communicate with the given
            slave provider.
    \throws std::out_of_range if `slaveProviderId` is unknown.
    */
    uint16_t SlaveProviderProtocol(const std::string& slaveProviderId) const;

    //TODO: Just for debugging purposes, remove later.
    void Dump() const;

private:
    struct SlaveProvider
    {
        uint16_t protocol;
        boost::chrono::steady_clock::time_point lastHeartbeat;
    };

    uint16_t m_maxProtocol;
    boost::chrono::milliseconds m_slaveProviderTimeout;

    std::map<std::string, SlaveProvider> m_slaveProviders;
    // Slave types, mapped to slave provider IDs.
    SlaveTypesByProviderContainer m_slaveTypes;
};


}}      // namespace
#endif  // header guard
