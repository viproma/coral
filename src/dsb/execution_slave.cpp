#include "dsb/execution/slave.hpp"

#include <utility>
#include <vector>

#include "zmq.hpp"

#include "dsb/bus/slave_agent.hpp"
#include "dsb/net/zmqx.hpp"
#include "dsb/util.hpp"


namespace dsb
{
namespace execution
{


SlaveRunner::SlaveRunner(
    std::shared_ptr<ISlaveInstance> slaveInstance,
    const dsb::net::Endpoint& controlEndpoint,
    const dsb::net::Endpoint& dataPubEndpoint,
    std::chrono::seconds commTimeout)
    : m_slaveInstance(slaveInstance),
      m_reactor(std::make_unique<dsb::net::Reactor>()),
      m_slaveAgent(std::make_unique<dsb::bus::SlaveAgent>(
        *m_reactor,
        *slaveInstance,
        controlEndpoint,
        dataPubEndpoint,
        commTimeout))
{
}


SlaveRunner::SlaveRunner(SlaveRunner&& other) DSB_NOEXCEPT
    : m_slaveInstance(std::move(other.m_slaveInstance)),
      m_reactor(std::move(other.m_reactor)),
      m_slaveAgent(std::move(other.m_slaveAgent))
{
}
SlaveRunner& SlaveRunner::operator=(SlaveRunner&& other) DSB_NOEXCEPT
{
    m_slaveInstance = std::move(other.m_slaveInstance);
    m_reactor = std::move(other.m_reactor);
    m_slaveAgent = std::move(other.m_slaveAgent);
    return *this;
}


// The destructor doesn't actually do anything, we just needed to declare
// it explicitly in the header to be able to use std::unique_ptr with Reactor
// as an incomplete type.
SlaveRunner::~SlaveRunner() { }


dsb::net::Endpoint SlaveRunner::BoundControlEndpoint()
{
    return m_slaveAgent->BoundControlEndpoint();
}


dsb::net::Endpoint SlaveRunner::BoundDataPubEndpoint()
{
    return m_slaveAgent->BoundDataPubEndpoint();
}


void SlaveRunner::Run()
{
    m_reactor->Run();
}


}} // namespace
