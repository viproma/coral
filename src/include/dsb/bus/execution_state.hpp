#ifndef DSB_BUS_EXECUTION_STATE_HPP
#define DSB_BUS_EXECUTION_STATE_HPP

#include <deque>
#include "zmq.hpp"


namespace dsb
{
namespace bus
{


class ExecutionAgent;
class SlaveTracker;


/**
\brief  An interface for classes that represent a state of an execution.

The execution is modelled as a state machine, with ExecutionAgent as the
central class.  All incoming messages to the ExecutionAgent are forwarded
to an object of type IExecutionState, which is responsible for taking the
actions that are appropriate for the current state.
*/
class IExecutionState
{
public:
    /**
    \brief  Called upon entering the state.

    \param [in] self            The ExecutionAgent.
    \param [in] userSocket      The socket for the "user" channel.
    \param [in] slaveSocket     The socket for the "slave" channel.
    */
    virtual void StateEntered(
        ExecutionAgent& self,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket) = 0;

    /**
    \brief  Called when a message has been received on the "user" channel.

    \param [in] self            The ExecutionAgent.
    \param [in] msg             The received message.
    \param [in] userSocket      The socket for the "user" channel.
    \param [in] slaveSocket     The socket for the "slave" channel.
    */
    virtual void UserMessage(
        ExecutionAgent& self,
        std::deque<zmq::message_t>& msg,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket) = 0;

    /**
    \brief  Called when a message has been received on the "slave" channel,
            and the slave is waiting for a reply.

    Messages from slaves are first sent to the SlaveTracker object for the slave
    in question.  The SlaveTracker object may be able to handle the request and
    send the appropriate reply on its own, in which case the SlaveWaiting()
    function is never called.  In some cases, however, the reply may be delayed
    pending some action from the user or from other slaves.  In this case, the
    SlaveTracker does not send a reply, and the SlaveWaiting() function is
    called instead.

    \param [in] self            The ExecutionAgent.
    \param [in] slaveHandler    The slave which received the message.
    \param [in] userSocket      The socket for the "user" channel.
    \param [in] slaveSocket     The socket for the "slave" channel.
    */
    virtual void SlaveWaiting(
        ExecutionAgent& self,
        SlaveTracker& slaveHandler,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket) = 0;
};


/// Implementation of the "execution initializing" state.
class ExecutionInitializing : public IExecutionState
{
    void StateEntered(
        ExecutionAgent& self,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket) override;

    void UserMessage(
        ExecutionAgent& self,
        std::deque<zmq::message_t>& msg,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket) override;

    void SlaveWaiting(
        ExecutionAgent& self,
        SlaveTracker& slaveHandler,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket) override;
};


/// Implementation of the "all slaves are ready" state.
class ExecutionReady : public IExecutionState
{
    void StateEntered(
        ExecutionAgent& self,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket) override;

    void UserMessage(
        ExecutionAgent& self,
        std::deque<zmq::message_t>& msg,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket) override;

    void SlaveWaiting(
        ExecutionAgent& self,
        SlaveTracker& slaveHandler,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket) override;
};


/// Implementation of the "some slaves are still stepping" state.
class ExecutionStepping : public IExecutionState
{
    void StateEntered(
        ExecutionAgent& self,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket) override;

    void UserMessage(
        ExecutionAgent& self,
        std::deque<zmq::message_t>& msg,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket) override;

    void SlaveWaiting(
        ExecutionAgent& self,
        SlaveTracker& slaveHandler,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket) override;
};


/// Implementation of the "all slaves have published" state.
class ExecutionPublished : public IExecutionState
{
    void StateEntered(
        ExecutionAgent& self,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket) override;

    void UserMessage(
        ExecutionAgent& self,
        std::deque<zmq::message_t>& msg,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket) override;

    void SlaveWaiting(
        ExecutionAgent& self,
        SlaveTracker& slaveHandler,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket) override;
};


/// Implementation of the "execution terminating" state.
class ExecutionTerminating : public IExecutionState
{
public:
    ExecutionTerminating();

    void StateEntered(
        ExecutionAgent& self,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket) override;

    void UserMessage(
        ExecutionAgent& self,
        std::deque<zmq::message_t>& msg,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket) override;

    void SlaveWaiting(
        ExecutionAgent& self,
        SlaveTracker& slaveHandler,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket) override;

private:
    std::deque<zmq::message_t> m_termMsg;
};


}}      // namespace
#endif  // header guard
