/**
\file
\brief  Defines the coral::bus::SlaveControlMessengerV0 class
\copyright
    Copyright 2013-2017, SINTEF Ocean and the Coral contributors.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifndef CORAL_BUS_SLAVE_CONTROL_MESSENGER_V0_HPP
#define CORAL_BUS_SLAVE_CONTROL_MESSENGER_V0_HPP

#include <chrono>
#include <memory>

#include <coral/config.h>
#include <coral/bus/slave_control_messenger.hpp>
#include <coral/bus/slave_setup.hpp>
#include <coral/model.hpp>
#include <coral/net.hpp>
#include <coral/net/reactor.hpp>
#include <coral/net/zmqx.hpp>

#include <boost/variant.hpp>


// Forward declaration to avoid header dependency
namespace google { namespace protobuf { class MessageLite; } }


namespace coral
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
        coral::net::Reactor& reactor,
        coral::net::zmqx::ReqSocket socket,
        coral::model::SlaveID slaveID,
        const std::string& slaveName,
        const SlaveSetup& setup,
        std::chrono::milliseconds timeout,
        MakeSlaveControlMessengerHandler onComplete);

    ~SlaveControlMessengerV0() CORAL_NOEXCEPT;

    SlaveState State() const CORAL_NOEXCEPT override;

    void Close() override;

    void GetDescription(
        std::chrono::milliseconds timeout,
        GetDescriptionHandler onComplete) override;

    void SetVariables(
        const std::vector<coral::model::VariableSetting>& settings,
        std::chrono::milliseconds timeout,
        SetVariablesHandler onComplete) override;

    void SetPeers(
        const std::vector<coral::net::Endpoint>& peers,
        std::chrono::milliseconds timeout,
        SetPeersHandler onComplete) override;

    void ResendVars(
        std::chrono::milliseconds timeout,
        ResendVarsHandler onComplete) override;

    void Step(
        coral::model::StepID stepID,
        coral::model::TimePoint currentT,
        coral::model::TimeDuration deltaT,
        std::chrono::milliseconds timeout,
        StepHandler onComplete) override;

    void AcceptStep(
        std::chrono::milliseconds timeout,
        AcceptStepHandler onComplete) override;

    void Terminate() override;

private:
    typedef boost::variant<VoidHandler, GetDescriptionHandler> AnyHandler;

    void Setup(
        coral::model::SlaveID slaveID,
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
    void ResendVarsReplyReceived(
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

    coral::net::Reactor& m_reactor;
    coral::net::zmqx::ReqSocket m_socket;

    // State information
    SlaveState m_state;
    bool m_attachedToReactor;
    int m_currentCommand;
    AnyHandler m_onComplete;
    int m_replyTimeoutTimerId;
};


}} // namespace
#endif // header guard
