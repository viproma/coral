#ifndef DSB_EXECUTION_VARIABLE_IO_HPP
#define DSB_EXECUTION_VARIABLE_IO_HPP

#include <memory>
#include <queue>
#include <string>
#include <unordered_map>

#include "boost/chrono/duration.hpp"

#include "dsb/model.hpp"


// Forward declaration to avoid dependency on ZMQ headers
namespace zmq { class socket_t; }


namespace dsb
{
namespace execution
{


/// A class which handles publishing of variable values on the network.
class VariablePublisher
{
public:
    /**
    \brief  Default constructor.

    Note that Connect() must be called before any variables may be published.
    */
    VariablePublisher();

    /**
    \brief  Connects to the remote endpoint to which variable values should be
            published, and set the slave ID used for the outgoing data.

    This is typically used to connect to an XPUB/XSUB broker.

    \param [in] endpoint    The address to an XSUB or SUB endpoint.
    \param [in] ownID       The slave ID with which to tag the outgoing data.

    \pre Connect() has not been called previously on this instance.
    */
    void Connect(const std::string& endpoint, dsb::model::SlaveID ownID);

    /**
    \brief  Publishes the value of a single variable.

    While this is not enforced by the present function, the recipient (i.e.,
    the VariableSubscriber) requires that all subscribed-to variables be
    published for any given time step, and that the time step ID never
    decreases.

    \param [in] stepID      Time step ID
    \param [in] variableID  Variable ID (which is paired with the slave ID
                            to form a "global" variable ID before sending)
    \param [in] value       The variable value

    \pre Connect() has been called successfully on this instance.
    */
    void Publish(
        dsb::model::StepID stepID,
        dsb::model::VariableID variableID,
        dsb::model::ScalarValue value);

private:
    dsb::model::SlaveID m_ownID;
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
    \brief  Connects to the remote endpoint from which variable values should
            be received.

    This is typically used to connect to an XPUB/XSUB broker.

    \param [in] endpoint    The address to an XPUB or PUB endpoint.

    \pre Connect() has not been called previously on this instance.
    */
    void Connect(const std::string& endpoint);

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
        boost::chrono::milliseconds timeout);

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
