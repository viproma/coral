#ifndef DSB_BUS_VARIABLE_IO_HPP_INCLUDED
#define DSB_BUS_VARIABLE_IO_HPP_INCLUDED

#include <chrono>
#include <memory>
#include <queue>
#include <string>
#include <unordered_map>

#include "dsb/model.hpp"
#include "dsb/net.hpp"


// Forward declaration to avoid dependency on ZMQ headers
namespace zmq { class socket_t; }


namespace dsb
{
namespace bus
{


/// A class which handles publishing of variable values on the network.
class VariablePublisher
{
public:
    /**
    \brief  Default constructor.

    Note that Bind() must be called before any variables may be published.
    */
    VariablePublisher();

    /**
    \brief  Binds to a local endpoint.

    \param [in] endpoint
        The endpoint, in the format `tcp://<interface>:<port>`, where
        "interface" may be "*" to signify all network interfaces, and
        "port" may be "*" to signify an OS-assigned (ephemeral) port.

    \pre Bind() has not been called previously on this instance.
    */
    void Bind(const dsb::net::Endpoint& endpoint);

    /**
    \brief  Returns the endpoint bound to by the last Bind() call.

    This is useful when the port number was specified as '*', as this will
    return the actual port number as part of the endpoint address.

    \pre Bind() has been called successfully on this instance.
    */
    dsb::net::Endpoint BoundEndpoint() const;

    /**
    \brief  Publishes the value of a single variable.

    While this is not enforced by the present function, the recipient (i.e.,
    the VariableSubscriber) requires that all subscribed-to variables be
    published for any given time step, and that the time step ID never
    decreases.

    \param [in] stepID      Time step ID
    \param [in] slaveID     Slave ID
    \param [in] variableID  Variable ID (which is paired with the slave ID
                            to form a "global" variable ID before sending)
    \param [in] value       The variable value

    \pre Bind() has been called successfully on this instance.
    */
    void Publish(
        dsb::model::StepID stepID,
        dsb::model::SlaveID slaveID,
        dsb::model::VariableID variableID,
        dsb::model::ScalarValue value);

private:
    std::unique_ptr<zmq::socket_t> m_socket;
};


/// A class which handles subscriptions to and receiving of variable values.
class VariableSubscriber
{
public:
    /**
    \brief  Default constructor.

    Note that Connect() must be called before any variables can be received.
    */
    VariableSubscriber();

    /**
    \brief  Connects to the remote endpoints from which variable values should
            be received.

    Every time this function is called, existing connections are broken and
    new ones are established.  Thus, *all* endpoints must be specified each
    time.

    \param [in] endpoints
        A pointer to an array of endpoints.
    \param [in] endpointsSize
        The size of the `endpoints` array.
    */
    void Connect(
        const dsb::net::Endpoint* endpoints,
        std::size_t endpointsSize);

    /**
    \brief Subscribes to the given variable.

    \pre Connect() has been called successfully on this instance.
    */
    void Subscribe(const dsb::model::Variable& variable);

    /**
    \brief Unsubscribes from the given variable.

    \pre Connect() has been called successfully on this instance.
    */
    void Unsubscribe(const dsb::model::Variable& variable);

    /**
    \brief  Waits until the values of all subscribed-to variables have been
            received for the given time step.

    \param [in] stepID      The timestep ID for which we should wait for
                            variable data.
    \param [in] timeout     How long to wait without receiving any data.

    \throws std::runtime_error if no data is received for the duration
        specified by `timeout`.
    \pre Connect() has been called successfully on this instance.
    */
    void Update(
        dsb::model::StepID stepID,
        std::chrono::milliseconds timeout);

    /**
    \brief  Returns the value of the given variable which was acquired with the
            last Update() call.

    This function may not be called if Update() has not been called yet, or if
    the last Update() call failed.  Furthermore, the returned reference is only
    guaranteed to be valid until the next Update() call.

    \param [in] variable    A variable identifier. The variable must be one
                            which has previously been subscribed to with
                            Subscribe().

    \pre Update() has been called successfully.
    */
    const dsb::model::ScalarValue& Value(const dsb::model::Variable& variable)
        const;

private:
    typedef std::queue<std::pair<dsb::model::StepID, dsb::model::ScalarValue>>
        ValueQueue;

    // A hash function for Variable objects, so we can put them in a
    // std::unordered_map (below)
    struct VariableHash
    {
        std::size_t operator()(const dsb::model::Variable& v) const
        {
            return static_cast<std::size_t>((v.Slave() << 16) + v.ID());
        }
    };

    dsb::model::StepID m_currentStepID;
    std::unique_ptr<zmq::socket_t> m_socket;
    std::unordered_map<dsb::model::Variable, ValueQueue, VariableHash> m_values;
};


}} // namespace
#endif // header guard
