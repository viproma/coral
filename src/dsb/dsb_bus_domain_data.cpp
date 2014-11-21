#include "dsb/bus/domain_data.hpp"

#include <iterator>
#include <iostream> //TODO: For debugging purposes; remove later.
#include "boost/foreach.hpp"


namespace dsb
{
namespace bus
{


DomainData::DomainData(
    uint16_t maxProtocol,
    boost::chrono::milliseconds slaveProviderTimeout)
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
    boost::chrono::steady_clock::time_point heartbeatTime)
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
    boost::chrono::steady_clock::time_point referenceTime)
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
    const dsbproto::domain::SlaveTypeList& slaveTypes)
{
    m_slaveTypes[slaveProviderId] = slaveTypes;
}


uint16_t DomainData::SlaveProviderProtocol(const std::string& slaveProviderId) const
{
    return m_slaveProviders.at(slaveProviderId).protocol;
}


//TODO: Just for debugging purposes, remove later.
void DomainData::Dump() const
{
    std::clog << "Slave providers:" << std::endl;
    BOOST_FOREACH (const auto sp, m_slaveProviders) {
        std::clog << "  " << sp.first << std::endl;
    }
    std::clog << "Slave types:" << std::endl;
    BOOST_FOREACH (const auto st, m_slaveTypes) {
        std::clog << "  " << st.first << std::endl;
        for (int i = 0; i < st.second.slave_type_size(); ++i) {
            std::clog << "    " << st.second.slave_type(i).name()
                        << " (" << st.second.slave_type(i).uuid() << ')'
                        << std::endl;
        }
    }
}


}}      // namespace
