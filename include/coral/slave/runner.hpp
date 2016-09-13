/**
\file
\brief Defines the coral::slave::Runner class and related functionality.
*/
#ifndef CORAL_SLAVE_RUNNER_HPP
#define CORAL_SLAVE_RUNNER_HPP

#include <chrono>
#include <memory>
#include "coral/config.h"
#include "coral/net.hpp"
#include "coral/slave/instance.hpp"


namespace coral
{

// Forward declarations to avoid dependencies on internal classes.
namespace bus { class SlaveAgent; }
namespace net { class Reactor; }

namespace slave
{


/// A class for running a slave instance
class Runner
{
public:
    Runner(
        std::shared_ptr<Instance> slaveInstance,
        const coral::net::Endpoint& controlEndpoint,
        const coral::net::Endpoint& dataPubEndpoint,
        std::chrono::seconds commTimeout);

    Runner(Runner&&) CORAL_NOEXCEPT;

    Runner& operator=(Runner&&) CORAL_NOEXCEPT;

    ~Runner();

    coral::net::Endpoint BoundControlEndpoint();
    coral::net::Endpoint BoundDataPubEndpoint();

    void Run();

private:
    std::shared_ptr<Instance> m_slaveInstance;
    std::unique_ptr<coral::net::Reactor> m_reactor;
    std::unique_ptr<coral::bus::SlaveAgent> m_slaveAgent;
};


}}      // namespace
#endif  // header guard
