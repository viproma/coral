#ifndef DSB_BUS_DOMAIN_DATA_HPP
#define DSB_BUS_DOMAIN_DATA_HPP

#include <cstdint>
#include <map>
#include <string>

#include "boost/chrono.hpp"
#include "domain.pb.h"


namespace dsb
{
namespace bus
{


class DomainData
{
public:
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

    void UpdateSlaveTypes(
        const std::string& slaveProviderId,
        const dsbproto::domain::SlaveTypeList& slaveTypes);

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
    std::map<std::string, dsbproto::domain::SlaveTypeList> m_slaveTypes;
};


}}      // namespace
#endif  // header guard
