/*
Copyright 2013-present, SINTEF Ocean.
This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#include <coral/slave/runner.hpp>

#include <utility>
#include <vector>

#include <zmq.hpp>

#include <coral/bus/slave_agent.hpp>
#include <coral/net/zmqx.hpp>
#include <coral/util.hpp>


namespace coral
{
namespace slave
{


Runner::Runner(
    std::shared_ptr<Instance> slaveInstance,
    const coral::net::Endpoint& controlEndpoint,
    const coral::net::Endpoint& dataPubEndpoint,
    std::chrono::seconds commTimeout)
    : m_slaveInstance(slaveInstance),
      m_reactor(std::make_unique<coral::net::Reactor>()),
      m_slaveAgent(std::make_unique<coral::bus::SlaveAgent>(
        *m_reactor,
        *slaveInstance,
        controlEndpoint,
        dataPubEndpoint,
        commTimeout))
{
}


Runner::Runner(Runner&& other) noexcept
    : m_slaveInstance(std::move(other.m_slaveInstance)),
      m_reactor(std::move(other.m_reactor)),
      m_slaveAgent(std::move(other.m_slaveAgent))
{
}
Runner& Runner::operator=(Runner&& other) noexcept
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


coral::net::Endpoint Runner::BoundControlEndpoint()
{
    return m_slaveAgent->BoundControlEndpoint();
}


coral::net::Endpoint Runner::BoundDataPubEndpoint()
{
    return m_slaveAgent->BoundDataPubEndpoint();
}


void Runner::Run()
{
    m_reactor->Run();
}


}} // namespace
