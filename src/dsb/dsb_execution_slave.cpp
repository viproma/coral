#include "dsb/execution/slave.hpp"

#include <deque>
#include <utility>

#include "zmq.hpp"

#include "dsb/bus/slave_agent.hpp"
#include "dsb/comm/messaging.hpp"
#include "dsb/comm/util.hpp"
#include "dsb/comm/p2p.hpp"
#include "dsb/util.hpp"


namespace dsb
{
namespace execution
{


SlaveRunner::SlaveRunner(
    std::shared_ptr<ISlaveInstance> slaveInstance,
    std::string bindURL,
    boost::chrono::seconds commTimeout)
    : m_slaveInstance(slaveInstance),
      m_reactor(std::make_unique<dsb::comm::Reactor>()),
      m_slaveAgent(std::make_unique<dsb::bus::SlaveAgent>(
        *m_reactor,
        *slaveInstance,
        dsb::comm::P2PEndpoint(bindURL),
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


std::string SlaveRunner::BoundEndpoint()
{
    return m_slaveAgent->BoundEndpoint().URL();
}


void SlaveRunner::Run()
{
    m_reactor->Run();
}


}} // namespace
