#include "dsb/slave/runner.hpp"

#include <utility>
#include <vector>

#include "zmq.hpp"

#include "dsb/bus/slave_agent.hpp"
#include "dsb/net/zmqx.hpp"
#include "dsb/util.hpp"


namespace dsb
{
namespace slave
{


Runner::Runner(
    std::shared_ptr<Instance> slaveInstance,
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


Runner::Runner(Runner&& other) DSB_NOEXCEPT
    : m_slaveInstance(std::move(other.m_slaveInstance)),
      m_reactor(std::move(other.m_reactor)),
      m_slaveAgent(std::move(other.m_slaveAgent))
{
}
Runner& Runner::operator=(Runner&& other) DSB_NOEXCEPT
{
    m_slaveInstance = std::move(other.m_slaveInstance);
    m_reactor = std::move(other.m_reactor);
    m_slaveAgent = std::move(other.m_slaveAgent);
    return *this;
}


// The destructor doesn't actually do anything, we just needed to declare
// it explicitly in the header to be able to use std::unique_ptr with Reactor
// as an incomplete type.
Runner::~Runner() { }


dsb::net::Endpoint Runner::BoundControlEndpoint()
{
    return m_slaveAgent->BoundControlEndpoint();
}


dsb::net::Endpoint Runner::BoundDataPubEndpoint()
{
    return m_slaveAgent->BoundDataPubEndpoint();
}


void Runner::Run()
{
    m_reactor->Run();
}


}} // namespace
