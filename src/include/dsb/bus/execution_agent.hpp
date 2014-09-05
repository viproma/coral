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

    template<typename T>
    void ChangeState(
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket);

    std::map<std::string, SlaveTracker> slaves;

private:
    void UpdateState();
    std::unique_ptr<IExecutionState> m_state;
    std::unique_ptr<IExecutionState> m_nextState;
};


// =============================================================================

template<typename T>
void ExecutionAgent::ChangeState(
    zmq::socket_t& userSocket,
    zmq::socket_t& slaveSocket)
{
    m_nextState = std::make_unique<T>();
    m_nextState->StateEntered(*this, userSocket, slaveSocket);
}


}}      // namespace
#endif  // header guard
