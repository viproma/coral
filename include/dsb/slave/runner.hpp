/**
\file
\brief Defines the dsb::slave::Runner class and related functionality.
*/
#ifndef DSB_SLAVE_RUNNER_HPP
#define DSB_SLAVE_RUNNER_HPP

#include <chrono>
#include <memory>
#include "dsb/config.h"
#include "dsb/net.hpp"
#include "dsb/slave/instance.hpp"


namespace dsb
{

// Forward declarations to avoid dependencies on internal classes.
namespace bus { class SlaveAgent; }
namespace net { class Reactor; }

namespace slave
{


class Runner
{
public:
    Runner(
        std::shared_ptr<Instance> slaveInstance,
        const dsb::net::Endpoint& controlEndpoint,
        const dsb::net::Endpoint& dataPubEndpoint,
        std::chrono::seconds commTimeout);

    Runner(Runner&&) DSB_NOEXCEPT;

    Runner& operator=(Runner&&) DSB_NOEXCEPT;

    ~Runner();

    dsb::net::Endpoint BoundControlEndpoint();
    dsb::net::Endpoint BoundDataPubEndpoint();

    void Run();

private:
    std::shared_ptr<Instance> m_slaveInstance;
    std::unique_ptr<dsb::net::Reactor> m_reactor;
    std::unique_ptr<dsb::bus::SlaveAgent> m_slaveAgent;
};


}}      // namespace
#endif  // header guard
