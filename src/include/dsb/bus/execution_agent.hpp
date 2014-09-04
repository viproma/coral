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


class ExecutionAgent
{
public:
    ExecutionAgent(
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket);

    void UserMessage(
        std::deque<zmq::message_t>& msg,
        zmq::socket_t& userSocket,
        zmq::socket_t& slaveSocket);

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
