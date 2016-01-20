#include "dsb/bus/domain_data.hpp"

#include <iostream> //TODO: For debugging purposes; remove later.
#include <utility>


namespace dsb
{
namespace bus
{


DomainData::DomainData(
    uint16_t maxProtocol,
    std::chrono::milliseconds slaveProviderTimeout)
    : m_maxProtocol(maxProtocol), m_slaveProviderTimeout(slaveProviderTimeout)
{
}


size_t DomainData::SlaveProviderCount() const
{
    return m_slaveProviders.size();
}


bool DomainData::UpdateSlaveProvider(
    const std::string& id,
    uint16_t protocol,
    std::chrono::steady_clock::time_point heartbeatTime)
{
    auto it = m_slaveProviders.find(id);
    const bool newProvider = (it == m_slaveProviders.end());
    if (newProvider) {
        SlaveProvider sp;
        it = m_slaveProviders.insert(std::make_pair(id, sp)).first;
        // std::clog << "Slave provider added: " << id << std::endl;
    } // else std::clog << "Slave provider updated: " << id << std::endl;
    it->second.protocol = std::min(protocol, m_maxProtocol);
    it->second.lastHeartbeat = heartbeatTime;
    return newProvider;
}


void DomainData::PurgeSlaveProviders(
    std::chrono::steady_clock::time_point referenceTime)
{
    for (auto it = std::begin(m_slaveProviders);
            it != std::end(m_slaveProviders);
            )
    {
        assert (referenceTime >= it->second.lastHeartbeat
                && "Some funky time travelling is going on here");
        if (referenceTime - it->second.lastHeartbeat > m_slaveProviderTimeout) {
            const auto d = it++;
            // std::clog << "Slave provider timeout: " << d->first << std::endl;
            m_slaveTypes.erase(d->first);
            m_slaveProviders.erase(d);
        } else ++it;
    }
}


DomainData::SlaveTypesByProviderRange DomainData::SlaveTypesByProvider() const
{
    return m_slaveTypes;
}


void DomainData::UpdateSlaveTypes(
    const std::string& slaveProviderId,
    std::vector<dsbproto::model::SlaveTypeDescription> slaveTypes)
{
    m_slaveTypes[slaveProviderId] = std::move(slaveTypes);
}


uint16_t DomainData::SlaveProviderProtocol(const std::string& slaveProviderId) const
{
    return m_slaveProviders.at(slaveProviderId).protocol;
}


//TODO: Just for debugging purposes, remove later.
void DomainData::Dump() const
{
    std::clog << "Slave providers:" << std::endl;
    for (const auto& sp : m_slaveProviders) {
        std::clog << "  " << sp.first << std::endl;
    }
    std::clog << "Slave types:" << std::endl;
    for (const auto& st : m_slaveTypes) {
        std::clog << "  " << st.first << std::endl;
        for (const auto& sd : st.second) {
            std::clog << "    " << sd.name() << " (" << sd.uuid() << ')' << std::endl;
        }
    }
}


}}      // namespace
