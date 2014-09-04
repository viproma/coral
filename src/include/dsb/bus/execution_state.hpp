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


class IExecutionState
{
public:
    virtual void StateEntered(
        ExecutionAgent& self,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket) = 0;

    virtual void UserMessage(
        ExecutionAgent& self,
        std::deque<zmq::message_t>& msg,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket) = 0;

    virtual void SlaveWaiting(
        ExecutionAgent& self,
        SlaveTracker& slaveHandler,
        std::deque<zmq::message_t>& msg,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket) = 0;
};


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
        std::deque<zmq::message_t>& msg,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket) override;
};


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
        std::deque<zmq::message_t>& msg,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket) override;
};


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
        std::deque<zmq::message_t>& msg,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket) override;
};


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
        std::deque<zmq::message_t>& msg,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket) override;
};


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
        std::deque<zmq::message_t>& msg,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket) override;

private:
    std::deque<zmq::message_t> m_termMsg;
};


}}      // namespace
#endif  // header guard
