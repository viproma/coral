/**
\file
\brief  Defines the dsb::bus::SlaveControlMessengerV0 class
*/
#ifndef DSB_BUS_SLAVE_CONTROL_MESSENGER_V0_HPP
#define DSB_BUS_SLAVE_CONTROL_MESSENGER_V0_HPP

#include <memory>
#include "boost/chrono/duration.hpp"

#include "dsb/config.h"
#include "dsb/bus/slave_control_messenger.hpp"
#include "dsb/bus/slave_setup.hpp"
#include "dsb/comm/p2p.hpp"
#include "dsb/comm/reactor.hpp"
#include "dsb/model.hpp"
#include "dsb/net.hpp"


// Forward declaration to avoid header dependency
namespace google { namespace protobuf { class MessageLite; } }


namespace dsb
{
namespace bus
{


/**
\brief  An implementation of ISlaveControlMessenger for version 0 of the
        master/slave communication protocol.
*/
class SlaveControlMessengerV0 : public ISlaveControlMessenger
{
public:
    SlaveControlMessengerV0(
        dsb::comm::Reactor& reactor,
        dsb::comm::P2PReqSocket socket,
        dsb::model::SlaveID slaveID,
        const SlaveSetup& setup,
        boost::chrono::milliseconds timeout,
        MakeSlaveControlMessengerHandler onComplete);

    ~SlaveControlMessengerV0() DSB_NOEXCEPT;

    SlaveState State() const DSB_NOEXCEPT override;

    void Close() override;

    void SetVariables(
        const std::vector<dsb::model::VariableSetting>& settings,
        boost::chrono::milliseconds timeout,
        SetVariablesHandler onComplete) override;

    void Step(
        dsb::model::StepID stepID,
        dsb::model::TimePoint currentT,
        dsb::model::TimeDuration deltaT,
        boost::chrono::milliseconds timeout,
        StepHandler onComplete) override;

    void AcceptStep(
        boost::chrono::milliseconds timeout,
        AcceptStepHandler onComplete) override;

    void Terminate() override;

private:
    void Setup(
        dsb::model::SlaveID slaveID,
        const SlaveSetup& setup,
        boost::chrono::milliseconds timeout,
        VoidHandler onComplete);

    // Helper functions
    void Reset();
    void SendCommand(
        int command,
        const google::protobuf::MessageLite* data,
        boost::chrono::milliseconds timeout,
        VoidHandler onComplete);
    void PostSendCommand(
        int command,
        boost::chrono::milliseconds timeout,
        VoidHandler onComplete);
    void RegisterTimeout(boost::chrono::milliseconds timeout);
    void UnregisterTimeout();

    // Event handlers
    void OnReply();
    void OnReplyTimeout();

    // Reply parsing/handling
    void SetupReplyReceived(
        const std::vector<zmq::message_t>& msg,
        VoidHandler onComplete);
    void SetVarsReplyReceived(
        const std::vector<zmq::message_t>& msg,
        VoidHandler onComplete);
    void StepReplyReceived(
        const std::vector<zmq::message_t>& msg,
        VoidHandler onComplete);
    void AcceptStepReplyReceived(
        const std::vector<zmq::message_t>& msg,
        VoidHandler onComplete);

    // These guys perform the work which is common to several of the above
    // XyxReplyReceived() functions.
    void HandleExpectedReadyReply(
        const std::vector<zmq::message_t>& msg,
        VoidHandler onComplete);
    void HandleErrorReply(int reply, VoidHandler onComplete);

    // Class invariant checker
    void CheckInvariant() const;

    dsb::comm::Reactor& m_reactor;
    dsb::comm::P2PReqSocket m_socket;

    // State information
    SlaveState m_state;
    bool m_attachedToReactor;
    int m_currentCommand;
    VoidHandler m_onComplete;
    int m_replyTimeoutTimerId;
};


}} // namespace
#endif // header guard
