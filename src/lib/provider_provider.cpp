/*
Copyright 2013-present, SINTEF Ocean.
This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#include <coral/provider/provider.hpp>

#include <algorithm>
#include <cassert>

#include <boost/numeric/conversion/cast.hpp>
#include <zmq.hpp>

#include <coral/bus/slave_provider_comm.hpp>
#include <coral/error.hpp>
#include <coral/net/reactor.hpp>
#include <coral/net/service.hpp>
#include <coral/net/zmqx.hpp>
#include <coral/util.hpp>


namespace coral
{
namespace provider
{


namespace
{
    class MySlaveProviderOps : public coral::bus::SlaveProviderOps
    {
    public:
        MySlaveProviderOps(
            std::vector<std::unique_ptr<SlaveCreator>>&& slaveTypes)
            : m_slaveTypes(std::move(slaveTypes))
        {
        }

        int GetSlaveTypeCount() const noexcept override
        {
            return boost::numeric_cast<int>(m_slaveTypes.size());
        }

        coral::model::SlaveTypeDescription GetSlaveType(int index) const override
        {
            return m_slaveTypes.at(index)->Description();
        }

        coral::net::SlaveLocator InstantiateSlave(
            const std::string& slaveTypeUUID,
            std::chrono::milliseconds timeout) override
        {
            const auto st = std::find_if(
                begin(m_slaveTypes),
                end(m_slaveTypes),
                [&] (const decltype(m_slaveTypes)::value_type& e) {
                    return e->Description().UUID() == slaveTypeUUID;
                });
            if (st == end(m_slaveTypes)) {
                throw std::runtime_error("Unknown slave type");
            }
            coral::net::SlaveLocator loc;
            if (!(*st)->Instantiate(timeout, loc)) {
                throw std::runtime_error((*st)->InstantiationFailureDescription());
            }
            return loc;
        }

    private:
        const std::vector<std::unique_ptr<SlaveCreator>> m_slaveTypes;
    };


    // Ok, this is all a bit ugly, but it's for a good cause, namely to handle
    // as many errors as possible in the foreground thread (see below).
    struct BackgroundThreadData
    {
        // Note that the order of declarations matters here, because the
        // other members depend on the reactor being kept alive.
        std::shared_ptr<coral::net::Reactor> reactor;
        std::shared_ptr<zmq::socket_t> killSocket;
        std::shared_ptr<coral::net::reqrep::Server> server;
        std::shared_ptr<coral::net::service::Beacon> beacon;
    };

    void BackgroundThreadFunction(
        BackgroundThreadData objects,
        std::function<void(std::exception_ptr)> exceptionHandler)
    {
        try {
            objects.reactor->Run();
            objects.beacon->Stop();
        } catch (...) {
            if (exceptionHandler) {
                exceptionHandler(std::current_exception());
            } else {
                throw;
            }
        }
    }
}


SlaveProvider::SlaveProvider(
    const std::string& slaveProviderID,
    std::vector<std::unique_ptr<SlaveCreator>>&& slaveTypes,
    const coral::net::ip::Address& networkInterface,
    coral::net::ip::Port discoveryPort,
    std::function<void(std::exception_ptr)> exceptionHandler)
{
    CORAL_INPUT_CHECK(!slaveProviderID.empty());

    // We do as much as setup as possible in the "foreground" thread,
    // so that exceptions are most likely to be thrown here.
    BackgroundThreadData bg;
    bg.reactor = std::make_shared<coral::net::Reactor>();

    const auto killEndpoint = "inproc://" + coral::util::RandomUUID();
    m_killSocket = std::make_unique<zmq::socket_t>(
        coral::net::zmqx::GlobalContext(), ZMQ_PAIR);
    m_killSocket->bind(killEndpoint);
    bg.killSocket = std::make_shared<zmq::socket_t>(
        coral::net::zmqx::GlobalContext(), ZMQ_PAIR);
    bg.killSocket->connect(killEndpoint);
    bg.reactor->AddSocket(
        *bg.killSocket,
        [] (coral::net::Reactor& r, zmq::socket_t&) { r.Stop(); });

    bg.server = std::make_shared<coral::net::reqrep::Server>(
        *bg.reactor,
        coral::net::ip::Endpoint{networkInterface, "*"}.ToEndpoint("tcp"));
    coral::bus::MakeSlaveProviderServer(
        *bg.server,
        std::make_shared<MySlaveProviderOps>(std::move(slaveTypes)));

    char beaconPayload[2];
    coral::util::EncodeUint16(
        coral::net::zmqx::EndpointPort(bg.server->BoundEndpoint().URL()),
        beaconPayload);
    bg.beacon = std::make_shared<coral::net::service::Beacon>(
        0,
        "no.sintef.viproma.coral.slave_provider",
        slaveProviderID,
        beaconPayload,
        sizeof(beaconPayload),
        std::chrono::seconds(1),
        networkInterface,
        discoveryPort);

    m_thread = std::thread{&BackgroundThreadFunction, bg, exceptionHandler};
}


// This is just here so we can declare m_killSocket to be a std::unique_ptr
// to an incomplete type in the header.
SlaveProvider::~SlaveProvider() noexcept { }


void SlaveProvider::Stop()
{
    if (m_thread.joinable()) {
        char dummy = 0;
        m_killSocket->send(&dummy, 0, ZMQ_DONTWAIT);
        m_killSocket->recv(&dummy, 1, ZMQ_DONTWAIT);
        m_thread.join();
    }
}


}} // namespace
