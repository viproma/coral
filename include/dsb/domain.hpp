/**
\file
\brief Main header file for dsb::domain.
*/
#ifndef DSB_DOMAIN_HPP
#define DSB_DOMAIN_HPP

#include <memory>
#include <string>
#include <vector>
#include "zmq.hpp"
#include "dsb/types.hpp"


namespace dsb
{

/**
\brief  Functions and classes for controlling and interacting with the
        simulation domain.
*/
namespace domain
{


/**
\brief  Domain interface.
*/
// TODO: Decide what happens when the Controller object goes out of scope!
class Controller
{
public:
    // Constructor.  (Explicitly not Doxygen-documented, since it should
    // only be called by SpawnExecution().
    Controller(
        std::shared_ptr<zmq::context_t> context,
        const std::string& reportEndpoint,
        const std::string& infoEndpoint);

    /// Move constructor.
    Controller(Controller&& other);

    /// Move assignment operator.
    Controller& operator=(Controller&& other);

    /**
    \brief  Returns available slave types.
    */
    std::vector<dsb::types::SlaveType> GetSlaveTypes();

private:
    zmq::socket_t m_rpcSocket;
};


}}      //namespace
#endif  // header guard