/**
\file
\brief Defines the dsb::bus::SlaveAgent class.
*/
#ifndef DSB_BUS_SLAVE_AGENT_HPP
#define DSB_BUS_SLAVE_AGENT_HPP

#include <cstdint>
#include <deque>
#include <exception>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "boost/chrono/duration.hpp"
#include "zmq.hpp"
#include "dsb/comm/p2p.hpp"
#include "dsb/comm/reactor.hpp"
#include "dsb/config.h"
#include "dsb/execution/slave.hpp"
#include "dsb/model.hpp"
#include "execution.pb.h"


namespace dsb
{
namespace bus
{


/**
\brief  A class which contains the state of the slave and takes care of
        responding to requests from the master node in an appropriate manner.
*/
class SlaveAgent
{
public:
    /**
    \brief  Constructs a new SlaveAgent.

    \param [in] reactor
        The Reactor which should be used to listen for incoming messages.
    \param [in] slaveInstance
        The slave itself.
    \param [in] bindpoint
        The endpoint on which the slave should listen for incoming messages
        from the master.
    \param [in] commTimeout
        A time after which communication with the master is assumed to be
        broken.  When this happens, a dsb::execution::TimeoutException will
        be thrown from the "incoming message" handler, and will propagate
        out through dsb::comm::Reactor::Run().
    */
    SlaveAgent(
        dsb::comm::Reactor& reactor,
        dsb::execution::ISlaveInstance& slaveInstance,
        const dsb::comm::P2PEndpoint& bindpoint,
        boost::chrono::milliseconds commTimeout);

    /**
    \brief  The endpoint on which the slave is listening for incoming messages
            from the master.

    This is useful if the `bindpoint` argument passed to the constructor
    contains a wildcard port number, in which case this function will return
    the actual port used.
    */
    const dsb::comm::P2PEndpoint& BoundEndpoint() const;

private:
    /*
    \brief  Responds to a message from the master.
    
    On input, `msg` must be the message received from master, and on output,
    it will contain the slave's reply.  Internally, the function forwards to
    the handler function that corresponds to the slave's current state.
    */
    void RequestReply(std::deque<zmq::message_t>& msg);

    // Each of these functions correspond to one of the slave's possible states.
    // On input, `msg` is a message from the master node, and when the function
    // returns, `msg` must contain the reply.  If the message triggers a state
    // change, the handler function must update m_stateHandler to point to the
    // function for the new state.
    void NotConnectedHandler(std::deque<zmq::message_t>& msg);
    void ConnectedHandler(std::deque<zmq::message_t>& msg);
    void ReadyHandler(std::deque<zmq::message_t>& msg);
    void PublishedHandler(std::deque<zmq::message_t>& msg);
    void StepFailedHandler(std::deque<zmq::message_t>& msg);

    // Performs the "set variables" operation for ReadyHandler(), including
    // filling `msg` with a reply message.
    void HandleSetVars(std::deque<zmq::message_t>& msg);

    // Performs the time step for ReadyHandler()
    bool Step(const dsbproto::execution::StepData& stepData);

    // A pointer to the handler function for the current state.
    void (SlaveAgent::* m_stateHandler)(std::deque<zmq::message_t>&);

    dsb::execution::ISlaveInstance& m_slaveInstance;
    boost::chrono::milliseconds m_commTimeout;
    dsb::comm::P2PRepSocket m_control;
    std::unique_ptr<zmq::socket_t> m_dataSub;
    std::unique_ptr<zmq::socket_t> m_dataPub;
    dsb::model::SlaveID m_id; // The slave's ID number in the current execution
    double m_currentTime;
    double m_lastStepSize;

    struct RemoteVariable
    {
        uint16_t slave;
        uint16_t var;
        bool operator<(const RemoteVariable& other) const {
            if (slave < other.slave) return true;
            else if (slave == other.slave) return var < other.var;
            else return false;
        }
    };
    std::map<RemoteVariable, uint16_t> m_connections;
};


/// Exception thrown when the slave receives a TERMINATE command.
class Shutdown : public std::exception
{
public:
    const char* what() const DSB_NOEXCEPT override { return "Normal shutdown requested by master"; }
};


}}      // namespace
#endif  // header guard
