/**
\file
\brief Defines the coral::bus::SlaveAgent class.
*/
#ifndef CORAL_BUS_SLAVE_AGENT_HPP
#define CORAL_BUS_SLAVE_AGENT_HPP

#include <chrono>
#include <exception>
#include <string>
#include <vector>

#include "boost/bimap.hpp"
#include "boost/bimap/multiset_of.hpp"
#include "zmq.hpp"

#include "coral/config.h"
#include "coral/bus/variable_io.hpp"
#include "coral/model.hpp"
#include "coral/net.hpp"
#include "coral/net/reactor.hpp"
#include "coral/net/zmqx.hpp"
#include "coral/slave/instance.hpp"
#include "execution.pb.h"


namespace coral
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
        broken.  When this happens, a coral::slave::TimeoutException will
        be thrown from the "incoming message" handler, and will propagate
        out through coral::net::Reactor::Run().
    */
    SlaveAgent(
        coral::net::Reactor& reactor,
        coral::slave::Instance& slaveInstance,
        const coral::net::Endpoint& controlEndpoint,
        const coral::net::Endpoint& dataPubEndpoint,
        std::chrono::milliseconds commTimeout);

    /**
    \brief  The endpoint on which the slave is listening for incoming messages
            from the master.

    This is useful if the `controlEndpoint` argument passed to the constructor
    contains a wildcard port number, in which case this function will return
    the actual port used.
    */
    coral::net::Endpoint BoundControlEndpoint() const;

    /**
    \brief  The endpoint to which the slave is publishing its output data.

    This is useful if the `dataPubEndpoint` argument passed to the constructor
    contains a wildcard port number, in which case this function will return
    the actual port used.
    */
    coral::net::Endpoint BoundDataPubEndpoint() const;

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

    // Performs the "set peers" operation for ReadyHandler(), including
    // filling `msg` with a reply message.
    void HandleSetPeers(std::vector<zmq::message_t>& msg);

    // Performs the time step for ReadyHandler()
    bool Step(const coralproto::execution::StepData& stepData);

    // Publishes all variable values (used by Step()).
    void PublishAll();

    // A pointer to the handler function for the current state.
    void (SlaveAgent::* m_stateHandler)(std::vector<zmq::message_t>&);


    // A less-than comparison functor for Variable objects, so we can put
    // them in a std::map.
    struct VariableLess
    {
        bool operator()(const coral::model::Variable& a, const coral::model::Variable& b) const
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
            const coral::net::Endpoint* endpoints,
            std::size_t endpointsSize);

        // Establishes a connection between a remote output variable and one of
        // our input variables, breaking any existing connections to that input.
        void Couple(
            coral::model::Variable remoteOutput,
            coral::model::VariableID localInput);

        // Waits until all data has been received for the time step specified
        // by `stepID` and updates the slave instance with the new values.
        void Update(
            coral::slave::Instance& slaveInstance,
            coral::model::StepID stepID,
            std::chrono::milliseconds timeout);

    private:
        // Breaks a connection to a local input variable, if any.
        void Decouple(coral::model::VariableID localInput);

        // A bidirectional mapping between output variables and input variables.
        typedef boost::bimap<
            boost::bimaps::multiset_of<coral::model::Variable, VariableLess>,
            coral::model::VariableID>
            ConnectionBimap;

        ConnectionBimap m_connections;
        coral::bus::VariableSubscriber m_subscriber;
    };

    coral::slave::Instance& m_slaveInstance;
    std::chrono::milliseconds m_commTimeout;

    coral::net::zmqx::RepSocket m_control;
    coral::bus::VariablePublisher m_publisher;
    Connections m_connections;
    coral::model::SlaveID m_id; // The slave's ID number in the current execution

    coral::model::StepID m_currentStepID; // ID of ongoing or just completed step
};


/// Exception thrown when the slave receives a TERMINATE command.
class Shutdown : public std::exception
{
public:
    const char* what() const CORAL_NOEXCEPT override { return "Normal shutdown requested by master"; }
};


}}      // namespace
#endif  // header guard
