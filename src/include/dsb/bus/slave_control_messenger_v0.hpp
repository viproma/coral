/**
\file
\brief  Defines the dsb::bus::SlaveControlMessengerV0 class
*/
#ifndef DSB_BUS_SLAVE_CONTROL_MESSENGER_V0_HPP
#define DSB_BUS_SLAVE_CONTROL_MESSENGER_V0_HPP

#include <chrono>
#include <memory>

#include "dsb/config.h"
#include "dsb/bus/slave_control_messenger.hpp"
#include "dsb/bus/slave_setup.hpp"
#include "dsb/model.hpp"
#include "dsb/net.hpp"
#include "dsb/net/reactor.hpp"
#include "dsb/net/zmqx.hpp"

#include "boost/variant.hpp"


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
        dsb::net::Reactor& reactor,
        dsb::net::zmqx::ReqSocket socket,
        dsb::model::SlaveID slaveID,
        const std::string& slaveName,
        const SlaveSetup& setup,
        std::chrono::milliseconds timeout,
        MakeSlaveControlMessengerHandler onComplete);

    ~SlaveControlMessengerV0() DSB_NOEXCEPT;

    SlaveState State() const DSB_NOEXCEPT override;

    void Close() override;

    void GetDescription(
        std::chrono::milliseconds timeout,
        GetDescriptionHandler onComplete) override;

    void SetVariables(
        const std::vector<dsb::model::VariableSetting>& settings,
        std::chrono::milliseconds timeout,
        SetVariablesHandler onComplete) override;

    void SetPeers(
        const std::vector<dsb::net::Endpoint>& peers,
        std::chrono::milliseconds timeout,
        SetPeersHandler onComplete) override;

    void Step(
        dsb::model::StepID stepID,
        dsb::model::TimePoint currentT,
        dsb::model::TimeDuration deltaT,
        std::chrono::milliseconds timeout,
        StepHandler onComplete) override;

    void AcceptStep(
        std::chrono::milliseconds timeout,
        AcceptStepHandler onComplete) override;

    void Terminate() override;

private:
    typedef boost::variant<VoidHandler, GetDescriptionHandler> AnyHandler;

    void Setup(
        dsb::model::SlaveID slaveID,
        const std::string& slaveName,
        const SlaveSetup& setup,
        std::chrono::milliseconds timeout,
        VoidHandler onComplete);

    // Helper functions
    void Reset();
    void SendCommand(
        int command,
        const google::protobuf::MessageLite* data,
        std::chrono::milliseconds timeout,
        AnyHandler onComplete);
    void PostSendCommand(
        int command,
        std::chrono::milliseconds timeout,
        AnyHandler onComplete);
    void RegisterTimeout(std::chrono::milliseconds timeout);
    void UnregisterTimeout();

    // Event handlers
    void OnReply();
    void OnReplyTimeout();

    // Reply parsing/handling
    void SetupReplyReceived(
        const std::vector<zmq::message_t>& msg,
        VoidHandler onComplete);
    void DescribeReplyReceived(
        const std::vector<zmq::message_t>& msg,
        GetDescriptionHandler onComplete);
    void SetPeersReplyReceived(
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
    void HandleErrorReply(int reply, AnyHandler onComplete);

    // Class invariant checker
    void CheckInvariant() const;

    dsb::net::Reactor& m_reactor;
    dsb::net::zmqx::ReqSocket m_socket;

    // State information
    SlaveState m_state;
    bool m_attachedToReactor;
    int m_currentCommand;
    AnyHandler m_onComplete;
    int m_replyTimeoutTimerId;
};


}} // namespace
#endif // header guard
