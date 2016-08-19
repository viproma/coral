/**
\file
\brief Defines the dsb::bus::SlaveAgent class.
*/
#ifndef DSB_BUS_SLAVE_AGENT_HPP
#define DSB_BUS_SLAVE_AGENT_HPP

#include <chrono>
#include <exception>
#include <string>
#include <vector>

#include "boost/bimap.hpp"
#include "boost/bimap/multiset_of.hpp"
#include "zmq.hpp"

#include "dsb/comm/p2p.hpp"
#include "dsb/comm/reactor.hpp"
#include "dsb/config.h"
#include "dsb/execution/slave.hpp"
#include "dsb/execution/variable_io.hpp"
#include "dsb/model.hpp"
#include "dsb/net.hpp"
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
    \param [in] controlEndpoint
        The endpoint to which the slave should bind to receive an incoming
        connection from a master.
    \param [in] dataPubEndpoint
        The endpoint to which the slave should bind and publish its output
        data.
    \param [in] commTimeout
        A time after which communication with the master is assumed to be
        broken.  When this happens, a dsb::execution::TimeoutException will
        be thrown from the "incoming message" handler, and will propagate
        out through dsb::comm::Reactor::Run().
    */
    SlaveAgent(
        dsb::comm::Reactor& reactor,
        dsb::execution::ISlaveInstance& slaveInstance,
        const dsb::net::Endpoint& controlEndpoint,
        const dsb::net::Endpoint& dataPubEndpoint,
        std::chrono::milliseconds commTimeout);

    /**
    \brief  The endpoint on which the slave is listening for incoming messages
            from the master.

    This is useful if the `controlEndpoint` argument passed to the constructor
    contains a wildcard port number, in which case this function will return
    the actual port used.
    */
    dsb::net::Endpoint BoundControlEndpoint() const;

    /**
    \brief  The endpoint to which the slave is publishing its output data.

    This is useful if the `dataPubEndpoint` argument passed to the constructor
    contains a wildcard port number, in which case this function will return
    the actual port used.
    */
    dsb::net::Endpoint BoundDataPubEndpoint() const;

private:
    /*
    \brief  Responds to a message from the master.

    On input, `msg` must be the message received from master, and on output,
    it will contain the slave's reply.  Internally, the function forwards to
    the handler function that corresponds to the slave's current state.
    */
    void RequestReply(std::vector<zmq::message_t>& msg);

    // Each of these functions correspond to one of the slave's possible states.
    // On input, `msg` is a message from the master node, and when the function
    // returns, `msg` must contain the reply.  If the message triggers a state
    // change, the handler function must update m_stateHandler to point to the
    // function for the new state.
    void NotConnectedHandler(std::vector<zmq::message_t>& msg);
    void ConnectedHandler(std::vector<zmq::message_t>& msg);
    void ReadyHandler(std::vector<zmq::message_t>& msg);
    void PublishedHandler(std::vector<zmq::message_t>& msg);
    void StepFailedHandler(std::vector<zmq::message_t>& msg);

    // Performs the "describe" operation, including filling `msg` with a
    // reply message.
    void HandleDescribe(std::vector<zmq::message_t>& msg);

    // Performs the "set variables" operation for ReadyHandler(), including
    // filling `msg` with a reply message.
    void HandleSetVars(std::vector<zmq::message_t>& msg);

    // Performs the "set peers" operation for ReadyHandler(), including,
    // filling `msg` with a reply message.
    void HandleSetPeers(std::vector<zmq::message_t>& msg);

    // Performs the time step for ReadyHandler()
    bool Step(const dsbproto::execution::StepData& stepData);

    // A pointer to the handler function for the current state.
    void (SlaveAgent::* m_stateHandler)(std::vector<zmq::message_t>&);


    // A less-than comparison functor for Variable objects, so we can put
    // them in a std::map.
    struct VariableLess
    {
        bool operator()(const dsb::model::Variable& a, const dsb::model::Variable& b) const
        {
            return ((a.Slave() << 16) + a.ID()) < ((b.Slave() << 16) + b.ID());
        }
    };

    // A class which keeps track of connections to our input variables and the
    // values we receive for them.
    class Connections
    {
    public:
        // Connects to the publisher endpoints
        void Connect(
            const dsb::net::Endpoint* endpoints,
            std::size_t endpointsSize);

        // Establishes a connection between a remote output variable and one of
        // our input variables, breaking any existing connections to that input.
        void Couple(
            dsb::model::Variable remoteOutput,
            dsb::model::VariableID localInput);

        // Waits until all data has been received for the time step specified
        // by `stepID` and updates the slave instance with the new values.
        void Update(
            dsb::execution::ISlaveInstance& slaveInstance,
            dsb::model::StepID stepID,
            std::chrono::milliseconds timeout);

    private:
        // Breaks a connection to a local input variable, if any.
        void Decouple(dsb::model::VariableID localInput);

        // A bidirectional mapping between output variables and input variables.
        typedef boost::bimap<
            boost::bimaps::multiset_of<dsb::model::Variable, VariableLess>,
            dsb::model::VariableID>
            ConnectionBimap;

        ConnectionBimap m_connections;
        dsb::execution::VariableSubscriber m_subscriber;
    };

    dsb::execution::ISlaveInstance& m_slaveInstance;
    std::chrono::milliseconds m_commTimeout;

    dsb::comm::P2PRepSocket m_control;
    dsb::execution::VariablePublisher m_publisher;
    Connections m_connections;
    dsb::model::SlaveID m_id; // The slave's ID number in the current execution

    dsb::model::StepID m_currentStepID; // ID of ongoing or just completed step
};


/// Exception thrown when the slave receives a TERMINATE command.
class Shutdown : public std::exception
{
public:
    const char* what() const DSB_NOEXCEPT override { return "Normal shutdown requested by master"; }
};


}}      // namespace
#endif  // header guard
