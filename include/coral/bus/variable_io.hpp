/**
\file
\brief  Defines the coral::bus::VariablePublisher and coral::bus::VariableSubscriber
        classes.
\copyright
    Copyright 2013-2017, SINTEF Ocean and the Coral contributors.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifndef CORAL_BUS_VARIABLE_IO_HPP_INCLUDED
#define CORAL_BUS_VARIABLE_IO_HPP_INCLUDED

#include <chrono>
#include <memory>
#include <queue>
#include <string>
#include <unordered_map>

#include <coral/model.hpp>
#include <coral/net.hpp>


// Forward declaration to avoid dependency on ZMQ headers
namespace zmq { class socket_t; }


namespace coral
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
    void Bind(const coral::net::Endpoint& endpoint);

    /**
    \brief  Returns the endpoint bound to by the last Bind() call.

    This is useful when the port number was specified as '*', as this will
    return the actual port number as part of the endpoint address.

    \pre Bind() has been called successfully on this instance.
    */
    coral::net::Endpoint BoundEndpoint() const;

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
        coral::model::StepID stepID,
        coral::model::SlaveID slaveID,
        coral::model::VariableID variableID,
        coral::model::ScalarValue value);

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
        const coral::net::Endpoint* endpoints,
        std::size_t endpointsSize);

    /**
    \brief Subscribes to the given variable.

    \pre Connect() has been called successfully on this instance.
    */
    void Subscribe(const coral::model::Variable& variable);

    /**
    \brief Unsubscribes from the given variable.

    \pre Connect() has been called successfully on this instance.
    */
    void Unsubscribe(const coral::model::Variable& variable);

    /**
    \brief  Waits until the values of all subscribed-to variables have been
            received for the given time step.

    \param [in] stepID      The timestep ID for which we should wait for
                            variable data.
    \param [in] timeout     How long to wait without receiving any data.
                            A negative value means to wait indefinitely.

    \returns Whether a value has been received for all variables.
    \pre Connect() has been called successfully on this instance.
    */
    bool Update(
        coral::model::StepID stepID,
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
    const coral::model::ScalarValue& Value(const coral::model::Variable& variable)
        const;

private:
    typedef std::queue<std::pair<coral::model::StepID, coral::model::ScalarValue>>
        ValueQueue;

    // A hash function for Variable objects, so we can put them in a
    // std::unordered_map (below)
    struct VariableHash
    {
        std::size_t operator()(const coral::model::Variable& v) const
        {
            return static_cast<std::size_t>((v.Slave() << 16) + v.ID());
        }
    };

    coral::model::StepID m_currentStepID;
    std::unique_ptr<zmq::socket_t> m_socket;
    std::unordered_map<coral::model::Variable, ValueQueue, VariableHash> m_values;
};


}} // namespace
#endif // header guard
