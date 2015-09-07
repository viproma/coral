/**
\file
\brief Functionality for starting and controlling a simulation domain.
*/
#ifndef DSB_DOMAIN_CONTROLLER_HPP
#define DSB_DOMAIN_CONTROLLER_HPP

#include <memory>
#include <string>
#include <vector>

#include "boost/thread.hpp"
#include "zmq.hpp"

#include "dsb/config.h"
#include "dsb/model.hpp"
#include "dsb/net.hpp"


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
    */
    dsb::net::SlaveLocator InstantiateSlave(
        const std::string& slaveTypeUUID,
        const std::string& provider = std::string());

private:
    // NOTE: When adding members here, remember to update the move constructor
    // and the move assignment operator!
    zmq::socket_t m_rpcSocket;
    zmq::socket_t m_destroySocket; // for sending termination command from destructor.
    bool m_active;
    boost::thread m_thread;
};


}}      //namespace
#endif  // header guard
