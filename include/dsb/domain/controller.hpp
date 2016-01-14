/**
\file
\brief Functionality for starting and controlling a simulation domain.
*/
#ifndef DSB_DOMAIN_CONTROLLER_HPP
#define DSB_DOMAIN_CONTROLLER_HPP

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "dsb/config.h"
#include "dsb/model.hpp"
#include "dsb/net.hpp"


// Forward declaration to avoid dependency on ZMQ headers
namespace zmq { class socket_t; }


namespace dsb
{
namespace domain
{


/**
\brief  Domain interface.

This class is used to connect to a domain, inquire about the slave providers
available on the domain and the slave types they offer, and instantiate slaves
for executions.

\remark
When an object of this class is created, it will spawn a background thread that
performs the actual communication with other domain participants.  To ensure
that there is a one-to-one relationship between an object of this class and
its underlying communication thread, the objects are noncopyable (but movable),
and will attempt to shut down the thread on destruction.
*/
class Controller
{
public:
    /// Information about a slave type.
    struct SlaveType
    {
        std::string name;
        std::string uuid;
        std::string description;
        std::string author;
        std::string version;
        std::vector<dsb::model::VariableDescription> variables;
        std::vector<std::string> providers;
    };

    /// Constructor.
    explicit Controller(const dsb::net::DomainLocator& locator);

    /// Move constructor.
    Controller(Controller&& other) DSB_NOEXCEPT;

    /// Move assignment operator.
    Controller& operator=(Controller&& other) DSB_NOEXCEPT;

    /// Destructor.
    ~Controller();

    /**
    \brief  Returns available slave types.
    */
    std::vector<SlaveType> GetSlaveTypes();

    /**
    \brief  Instantiates a slave and connects it to an execution.

    If no slave provider is specified, and the specified slave type is offered
    by more than one slave provider, an arbitrary one of them will be used.

    `timeout` specifies both how long the slave provider should wait for the
    slave to start up before assuming it has crashed or frozen, as well as how
    long the master should wait for the slave provider to report that the slave
    has been successfully instantiated before it assumes the slave provider
    itself has crashed or the connection has been lost.  In any case, an
    exception is thrown if this timeout is reached.
    */
    dsb::net::SlaveLocator InstantiateSlave(
        const std::string& slaveTypeUUID,
        std::chrono::milliseconds timeout,
        const std::string& provider = std::string());

private:
    // NOTE: When adding members here, remember to update the move constructor
    // and the move assignment operator!
    std::unique_ptr<zmq::socket_t> m_rpcSocket;
    std::unique_ptr<zmq::socket_t> m_destroySocket; // for sending termination command from destructor.
    bool m_active;
    std::thread m_thread;
};


}}      //namespace
#endif  // header guard
