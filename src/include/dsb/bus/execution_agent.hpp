#ifndef DSB_BUS_EXECUTION_AGENT_HPP
#define DSB_BUS_EXECUTION_AGENT_HPP

#include <deque>
#include <map>
#include <memory>

#include "zmq.hpp"

#include "dsb/bus/execution_state.hpp"
#include "dsb/bus/slave_tracker.hpp"
#include "dsb/compat_helpers.hpp"


namespace dsb
{
namespace bus
{


/**
\brief  Data and methods which are internal to an ExecutionAgent but should
        be made available to, and shared between, the different state objects.
*/
class ExecutionAgentPrivate
{
public:
    ExecutionAgentPrivate();

    /// Switches to a different state.
    template<typename T>
    void ChangeState(zmq::socket_t& userSocket, zmq::socket_t& slaveSocket);

    /// The slaves which have been added to the execution.
    std::map<uint16_t, SlaveTracker> slaves;

    /// RPC calls that may cross state boundaries.
    enum UserRPC
    {
        NO_RPC,
        WAIT_FOR_READY_RPC,
        STEP_RPC,
    };

    /// The RPC call currently in progress.
    UserRPC rpcInProgress;

private:
    friend class ExecutionAgent;
    void UpdateState();
    std::unique_ptr<IExecutionState> m_state;
    std::unique_ptr<IExecutionState> m_nextState;

};


/**
\brief  Interprets and handles incoming messages to an execution.

This class receives incoming messages on both the "user" channel and the "slave"
channel, and takes appropriate action based on the contents of the messages
and the state of the execution.  This may include sending new messages on
either channel, which is why all functions require the corresponding socket
objects.
*/
class ExecutionAgent
{
public:
    /**
    \brief  Constructor.

    \param [in] userSocket      The socket for the "user" channel.
    \param [in] slaveSocket     The socket for the "slave" channel.
    */
    ExecutionAgent(
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket);

    /**
    \brief  Handles an incoming message on the "user" channel.

    \param [in] msg             The incoming message. Empty on return.
    \param [in] userSocket      The socket for the "user" channel.
    \param [in] slaveSocket     The socket for the "slave" channel.
    */
    void UserMessage(
        std::deque<zmq::message_t>& msg,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket);

    /**
    \brief  Handles an incoming message on the "control" channel.

    \param [in] msg             The incoming message. Empty on return.
    \param [in] userSocket      The socket for the "user" channel.
    \param [in] slaveSocket     The socket for the "slave" channel.
    */
    void SlaveMessage(
        std::deque<zmq::message_t>& msg,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket);

private:
    ExecutionAgentPrivate m_data;
};


// =============================================================================

template<typename T>
void ExecutionAgentPrivate::ChangeState(
    zmq::socket_t& userSocket,
    zmq::socket_t& slaveSocket)
{
    m_nextState = std::make_unique<T>();
    m_nextState->StateEntered(*this, userSocket, slaveSocket);
}


}}      // namespace
#endif  // header guard
