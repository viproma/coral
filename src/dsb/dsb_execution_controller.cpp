#include "dsb/execution/controller.hpp"

#include <algorithm> // for transform()
#include <cassert>
#include <cctype>
#include <chrono>
#include <exception>
#include <iostream>
#include <iterator> // for back_inserter()
#include <sstream>
#include <stdexcept>
#include <typeinfo>
#include <utility>
#include <vector>

#include "boost/foreach.hpp"
#include "boost/numeric/conversion/cast.hpp"

#include "zmq.hpp"

#include "dsb/bus/execution_manager.hpp"
#include "dsb/comm/messaging.hpp"
#include "dsb/comm/reactor.hpp"
#include "dsb/comm/util.hpp"
#include "dsb/error.hpp"
#include "dsb/inproc_rpc.hpp"
#include "dsb/protocol/glue.hpp"
#include "dsb/protobuf.hpp"
#include "dsb/util.hpp"

#include "broker.pb.h"
#include "execution_controller.pb.h"

namespace
{
    enum CallType
    {
        BEGIN_CONFIG_CALL,
        END_CONFIG_CALL,
        SET_SIMULATION_TIME_CALL,
        ADD_SLAVE_CALL,
        SET_VARIABLES_CALL,
        TERMINATE_CALL,
        ACCEPT_STEP_CALL,
        STEP_CALL,
    };

    dsb::model::VariableSetting FromProto(
        const dsbproto::execution_controller::VariableSetting& source)
    {
        if (source.has_value()) {
            if (source.has_connected_output()) {
                return dsb::model::VariableSetting(
                    boost::numeric_cast<dsb::model::VariableID>(source.variable_id()),
                    dsb::protocol::FromProto(source.value()),
                    dsb::protocol::FromProto(source.connected_output()));
            } else {
                return dsb::model::VariableSetting(
                    boost::numeric_cast<dsb::model::VariableID>(source.variable_id()),
                    dsb::protocol::FromProto(source.value()));
            }
        } else {
            if (source.has_connected_output()) {
                return dsb::model::VariableSetting(
                    boost::numeric_cast<dsb::model::VariableID>(source.variable_id()),
                    dsb::protocol::FromProto(source.connected_output()));
            } else {
                throw std::logic_error("Invalid VariableSetting object");
            }
        }
    }

    std::string ErrMsg(const std::string& message, std::error_code code)
    {
        return message + " (" + code.message() + ")";
    }


    // =========================================================================
    // ControllerBackend
    // =========================================================================

    /*
    This is the object which receives commands ("RPCs") from a
    dsb::execution::Controller (the "frontend") and forwards them to a
    dsb::bus::ExecutionManager which actually does the work.
    */
    class ControllerBackend
    {
    public:
        /*
        Constructor which attaches itself to a reactor to listen for incoming
        messages from the frontend.

        `rpcEndpoint` is the endpoint on which the frontend has bound its
        PAIR socket.
        */
        ControllerBackend(
            dsb::bus::ExecutionManager& execMgr,
            dsb::comm::Reactor& reactor,
            const std::string& rpcEndpoint);

    private:
        void HandleRPC(const std::vector<zmq::message_t>& msg);

        void BeginConfig(const std::vector<zmq::message_t>& msg);
        void EndConfig(const std::vector<zmq::message_t>& msg);
        void SetSimulationTime(const std::vector<zmq::message_t>& msg);
        void AddSlave(const std::vector<zmq::message_t>& msg);
        void SetVariables(const std::vector<zmq::message_t>& msg);
        void Step(const std::vector<zmq::message_t>& msg);
        void AcceptStep(const std::vector<zmq::message_t>& msg);

        dsb::bus::ExecutionManager& m_execMgr;
        dsb::comm::Reactor& m_reactor;
        zmq::socket_t m_rpcSocket;
    };


    ControllerBackend::ControllerBackend(
        dsb::bus::ExecutionManager& execMgr,
        dsb::comm::Reactor& reactor,
        const std::string& rpcEndpoint)
        : m_execMgr(execMgr),
          m_reactor(reactor),
          m_rpcSocket(dsb::comm::GlobalContext(), ZMQ_PAIR)
    {
        m_rpcSocket.connect(rpcEndpoint.c_str());
        reactor.AddSocket(m_rpcSocket, [this](dsb::comm::Reactor& r, zmq::socket_t& s) {
            assert(&s == &m_rpcSocket);
            std::vector<zmq::message_t> msg;
            dsb::comm::Receive(m_rpcSocket, msg);
            HandleRPC(msg);
        });
    }

    void ControllerBackend::HandleRPC(const std::vector<zmq::message_t>& msg)
    {
        try {
            switch (dsb::inproc_rpc::GetCallType(msg)) {
                case BEGIN_CONFIG_CALL:
                    BeginConfig(msg);
                    break;
                case END_CONFIG_CALL:
                    EndConfig(msg);
                    break;
                case SET_SIMULATION_TIME_CALL:
                    SetSimulationTime(msg);
                    break;
                case ADD_SLAVE_CALL:
                    AddSlave(msg);
                    break;
                case SET_VARIABLES_CALL:
                    SetVariables(msg);
                    break;
                case STEP_CALL:
                    Step(msg);
                    break;
                case ACCEPT_STEP_CALL:
                    AcceptStep(msg);
                    break;
                case TERMINATE_CALL:
                    m_execMgr.Terminate();
                    dsb::inproc_rpc::ReturnSuccess(m_rpcSocket);
                    m_reactor.Stop();
                    break;
                default:
                    assert (!"ControllerBackend received invalid message");
            }
        } catch (const std::logic_error& e) {
            dsb::inproc_rpc::ThrowLogicError(m_rpcSocket, e.what());
        } catch (const std::exception& e) {
            dsb::inproc_rpc::ThrowRuntimeError(m_rpcSocket, e.what());
        }
    }

    void ControllerBackend::BeginConfig(const std::vector<zmq::message_t>& msg)
    {
        m_execMgr.BeginConfig(
            [this] (const std::error_code& ec) {
                if (!ec) dsb::inproc_rpc::ReturnSuccess(m_rpcSocket);
                else {
                    dsb::inproc_rpc::ThrowRuntimeError(
                        m_rpcSocket,
                        ErrMsg("Failed to enter configuration mode", ec));
                }
            });
    }

    void ControllerBackend::EndConfig(const std::vector<zmq::message_t>& msg)
    {
        m_execMgr.EndConfig(
            [this] (const std::error_code& ec) {
                if (!ec) dsb::inproc_rpc::ReturnSuccess(m_rpcSocket);
                else {
                    dsb::inproc_rpc::ThrowRuntimeError(
                        m_rpcSocket,
                        ErrMsg("Failed to leave configuration mode", ec));
                }
            });
    }

    void ControllerBackend::SetSimulationTime(const std::vector<zmq::message_t>& msg)
    {
        dsbproto::execution_controller::SetSimulationTimeArgs args;
        dsb::inproc_rpc::UnmarshalArgs(msg, args);
        m_execMgr.SetSimulationTime(args.start_time(), args.stop_time());
        dsb::inproc_rpc::ReturnSuccess(m_rpcSocket);
    }

    void ControllerBackend::AddSlave(const std::vector<zmq::message_t>& msg)
    {
        dsbproto::execution_controller::AddSlaveArgs args;
        dsb::inproc_rpc::UnmarshalArgs(msg, args);
        auto promisedID = std::make_shared<std::promise<dsb::model::SlaveID>>();
        const auto slaveName = args.slave_name();
        m_execMgr.AddSlave(
            dsb::protocol::FromProto(args.slave_locator()),
            slaveName,
            m_reactor,
            std::chrono::milliseconds(args.timeout_ms()),
            [promisedID, slaveName, this] (const std::error_code& ec, dsb::model::SlaveID id) {
                std::clog << "addedSlave " << id << ": " << ec.message() << std::endl;
                if (!ec) {
                    promisedID->set_value(id);
                } else {
                    promisedID->set_exception(std::make_exception_ptr(std::runtime_error(
                        ErrMsg("Failed to add slave: " + slaveName, ec))));
                }
            });

        // Create a future on the heap and pass its pointer back to the
        // frontend (by casting to uint).  Frontend takes ownership.
        dsbproto::execution_controller::AddSlaveReturn ret;
        ret.set_slave_id_future_ptr(reinterpret_cast<std::uintptr_t>(
            new std::future<dsb::model::SlaveID>(promisedID->get_future())));
        dsb::inproc_rpc::ReturnSuccess(m_rpcSocket, &ret);
    }

    void ControllerBackend::SetVariables(const std::vector<zmq::message_t>& msg)
    {
        dsbproto::execution_controller::SetVariablesArgs args;
        dsb::inproc_rpc::UnmarshalArgs(msg, args);

        std::vector<dsb::model::VariableSetting> settings;
        std::transform(
            args.variable_setting().begin(),
            args.variable_setting().end(),
            std::back_inserter(settings),
            &FromProto);

        const auto slaveID = boost::numeric_cast<dsb::model::SlaveID>(args.slave_id());
        auto promise = std::make_shared<std::promise<void>>();
        m_execMgr.SetVariables(
            slaveID,
            settings,
            std::chrono::milliseconds(args.timeout_ms()),
            [slaveID, promise, this] (const std::error_code& ec) {
                if (!ec) {
                    promise->set_value();
                } else {
                    promise->set_exception(std::make_exception_ptr(std::runtime_error(ErrMsg(
                        "Failed to set and/or connect variable(s) for slave: "
                        + m_execMgr.SlaveName(slaveID), ec))));
                }
            });

        // Create a future on the heap and pass its pointer back to the
        // frontend (by casting to uint).  Frontend takes ownership.
        dsbproto::execution_controller::SetVariablesReturn ret;
        ret.set_future_ptr(reinterpret_cast<std::uintptr_t>(
            new std::future<void>(promise->get_future())));
        dsb::inproc_rpc::ReturnSuccess(m_rpcSocket, &ret);
    }

    void ControllerBackend::Step(const std::vector<zmq::message_t>& msg)
    {
        dsbproto::execution_controller::StepArgs args;
        dsb::inproc_rpc::UnmarshalArgs(msg, args);

        auto ret = std::make_shared<dsbproto::execution_controller::StepReturn>();
        m_execMgr.Step(
            args.step_size(),
            std::chrono::milliseconds(args.timeout_ms()),
            [ret, this] (const std::error_code& ec) {
                if (!ec || ec == dsb::error::sim_error::cannot_perform_timestep) {
                    ret->set_result(ec == dsb::error::sim_error::cannot_perform_timestep
                        ? dsbproto::execution_controller::StepReturn_StepResult_STEP_FAILED
                        : dsbproto::execution_controller::StepReturn_StepResult_STEP_COMPLETE);
                    dsb::inproc_rpc::ReturnSuccess(m_rpcSocket, ret.get());
                } else {
                    dsb::inproc_rpc::ThrowRuntimeError(
                        m_rpcSocket,
                        ErrMsg("Failed to perform time step", ec));
                }
            },
            [ret] (const std::error_code& ec, dsb::model::SlaveID slaveID) {
                if (!ec) {
                    auto sr = ret->add_slave_result();
                    sr->set_slave_id(slaveID);
                    sr->set_result(dsbproto::execution_controller::StepReturn_StepResult_STEP_COMPLETE);
                } else if (ec == dsb::error::sim_error::cannot_perform_timestep) {
                    auto sr = ret->add_slave_result();
                    sr->set_slave_id(slaveID);
                    sr->set_result(dsbproto::execution_controller::StepReturn_StepResult_STEP_FAILED);
                }
            });
    }

    void ControllerBackend::AcceptStep(const std::vector<zmq::message_t>& msg)
    {
        // TODO: Report slave errors asynchronously.
        dsbproto::execution_controller::AcceptStepArgs args;
        dsb::inproc_rpc::UnmarshalArgs(msg, args);

        m_execMgr.AcceptStep(
            std::chrono::milliseconds(args.timeout_ms()),
            [this] (const std::error_code& ec) {
                if (!ec) dsb::inproc_rpc::ReturnSuccess(m_rpcSocket);
                else dsb::inproc_rpc::ThrowRuntimeError(
                    m_rpcSocket,
                    ErrMsg("Failed to complete time step", ec));
            });
    }


    // =========================================================================
    // ControllerLoop()
    // =========================================================================

    // This is the main function for the background thread.
    void ControllerLoop(
        std::shared_ptr<std::string> rpcEndpoint,
        std::shared_ptr<dsb::net::ExecutionLocator> execLocator) DSB_NOEXCEPT
    {
        try {
            // Main messaging loop
            dsb::comm::Reactor reactor;
            dsb::bus::ExecutionManager execMgr(*execLocator);
            ControllerBackend backend(execMgr, reactor, *rpcEndpoint);
            reactor.Run();

            // Terminate the execution broker
            auto execTerminationSocket = zmq::socket_t(dsb::comm::GlobalContext(), ZMQ_REQ);
            execTerminationSocket.connect(execLocator->ExecTerminationEndpoint().c_str());
            std::vector<zmq::message_t> termMsg;
            termMsg.push_back(dsb::comm::ToFrame("TERMINATE_EXECUTION"));
            termMsg.push_back(zmq::message_t());
            dsbproto::broker::TerminateExecutionData teData;
            teData.set_execution_name(execLocator->ExecName());
            dsb::protobuf::SerializeToFrame(teData, termMsg.back());
            dsb::comm::Send(execTerminationSocket, termMsg);
            // TODO: The following receive was just added to force ZMQ to send the
            // message. We don't really care about the reply.  We've tried closing
            // the socket and even the context manually, but then the message just
            // appears to be dropped altoghether.  This sucks, and we need to figure
            // out what is going on at some point.  See ZeroMQ issue 1264,
            // https://github.com/zeromq/libzmq/issues/1264
            char temp;
            execTerminationSocket.recv(&temp, 1);
        } catch (const std::exception& e) {
            // TODO: Figure out if there is a better way to handle this
            // situation, e.g. by preemptively sending a third kind of
            // "exception result" (besides runtime and logic error) to the
            // frontend, and making the frontend throw a BackendDiedException
            // or similar when it receives such a result after an RPC call.
            std::cerr << "Fatal: Unexpected exception thrown in execution "
                         "controller backend thread. Exception type: "
                      << typeid(e).name() << ". Exception message: " << e.what()
                      << std::endl;
            std::terminate();
        }
    }
}


// =============================================================================
// Controller
// =============================================================================

dsb::execution::Controller::Controller(const dsb::net::ExecutionLocator& locator)
    : m_rpcSocket(std::make_unique<zmq::socket_t>(dsb::comm::GlobalContext(), ZMQ_PAIR)),
      m_active(true)
{
    auto rpcEndpoint =
        std::make_shared<std::string>("inproc://" + dsb::util::RandomUUID());
    m_rpcSocket->bind(rpcEndpoint->c_str());
    m_thread = std::thread(ControllerLoop,
        rpcEndpoint,
        std::make_shared<dsb::net::ExecutionLocator>(locator));
}


dsb::execution::Controller::Controller(Controller&& other) DSB_NOEXCEPT
    : m_rpcSocket(std::move(other.m_rpcSocket)),
      m_active(dsb::util::MoveAndReplace(other.m_active, false)),
      m_thread(std::move(other.m_thread))
{
}


dsb::execution::Controller& dsb::execution::Controller::operator=(Controller&& other)
    DSB_NOEXCEPT
{
    m_rpcSocket         = std::move(other.m_rpcSocket);
    m_active            = dsb::util::MoveAndReplace(other.m_active, false);
    m_thread            = std::move(other.m_thread);
    return *this;
}


dsb::execution::Controller::~Controller()
{
    if (m_active) try {
        Terminate();
    } catch (...) {
        assert (!"dsb::execution::Controller::~Controller(): Terminate() threw");
    }
    m_thread.join();
}


void dsb::execution::Controller::Terminate()
{
    assert (m_active);
    dsb::inproc_rpc::Call(*m_rpcSocket, TERMINATE_CALL);
    m_active = false;
}


void dsb::execution::Controller::BeginConfig()
{
    dsb::inproc_rpc::Call(*m_rpcSocket, BEGIN_CONFIG_CALL);
}


void dsb::execution::Controller::EndConfig()
{
    dsb::inproc_rpc::Call(*m_rpcSocket, END_CONFIG_CALL);
}


void dsb::execution::Controller::SetSimulationTime(
    dsb::model::TimePoint startTime,
    dsb::model::TimePoint stopTime)
{
    dsbproto::execution_controller::SetSimulationTimeArgs args;
    args.set_start_time(startTime);
    args.set_stop_time(stopTime);
    dsb::inproc_rpc::Call(*m_rpcSocket, SET_SIMULATION_TIME_CALL, &args);
}


std::future<dsb::model::SlaveID> dsb::execution::Controller::AddSlave(
    dsb::net::SlaveLocator slaveLocator,
    const std::string& slaveName,
    std::chrono::milliseconds commTimeout)
{
    dsbproto::execution_controller::AddSlaveArgs args;
    dsb::protocol::ConvertToProto(slaveLocator, *args.mutable_slave_locator());
    args.set_slave_name(slaveName);
    args.set_timeout_ms(boost::numeric_cast<std::int32_t>(commTimeout.count()));
    dsbproto::execution_controller::AddSlaveReturn ret;
    dsb::inproc_rpc::Call(*m_rpcSocket, ADD_SLAVE_CALL, &args, &ret);

    // Take ownership of the future whose pointer is now stored in `ret`,
    // knowing that the backend has relinquished it.
    const auto futurePtr = std::unique_ptr<std::future<dsb::model::SlaveID>>(
        reinterpret_cast<std::future<dsb::model::SlaveID>*>(ret.slave_id_future_ptr()));
    auto future = std::move(*futurePtr);
    return future;
}


std::future<void> dsb::execution::Controller::SetVariables(
    dsb::model::SlaveID slave,
    const std::vector<dsb::model::VariableSetting>& variableSettings,
    std::chrono::milliseconds timeout)
{
    dsbproto::execution_controller::SetVariablesArgs args;
    args.set_slave_id(slave);
    for (auto it = begin(variableSettings); it != end(variableSettings); ++it) {
        const auto n = args.add_variable_setting();
        n->set_variable_id(it->Variable());
        if (it->HasValue())    dsb::protocol::ConvertToProto(it->Value(), *n->mutable_value());
        if (it->IsConnected()) dsb::protocol::ConvertToProto(it->ConnectedOutput(), *n->mutable_connected_output());
    }
    args.set_timeout_ms(boost::numeric_cast<std::int32_t>(timeout.count()));
    dsbproto::execution_controller::SetVariablesReturn ret;
    dsb::inproc_rpc::Call(*m_rpcSocket, SET_VARIABLES_CALL, &args, &ret);

    // Take ownership of the future whose pointer is now stored in `ret`,
    // knowing that the backend has relinquished it.
    const auto futurePtr = std::unique_ptr<std::future<void>>(
        reinterpret_cast<std::future<void>*>(ret.future_ptr()));
    auto future = std::move(*futurePtr);
    return future;
}


namespace
{
    dsb::execution::StepResult ConvStepResult(
        dsbproto::execution_controller::StepReturn_StepResult sr)
    {
        if (sr == dsbproto::execution_controller::StepReturn_StepResult_STEP_COMPLETE) {
            return dsb::execution::STEP_COMPLETE;
        } else {
            assert(sr == dsbproto::execution_controller::StepReturn_StepResult_STEP_FAILED);
            return dsb::execution::STEP_FAILED;
        }
    }

    std::pair<dsb::model::SlaveID, dsb::execution::StepResult> ConvSlaveResult(
        const dsbproto::execution_controller::StepReturn_SlaveResult& pb)
    {
        return std::make_pair(
            boost::numeric_cast<dsb::model::SlaveID>(pb.slave_id()),
            ConvStepResult(pb.result()));
    }
}

dsb::execution::StepResult dsb::execution::Controller::Step(
    dsb::model::TimeDuration stepSize,
    std::chrono::milliseconds timeout,
    std::vector<std::pair<dsb::model::SlaveID, StepResult>>* slaveResults)
{
    dsbproto::execution_controller::StepArgs args;
    args.set_step_size(stepSize);
    args.set_timeout_ms(boost::numeric_cast<std::int32_t>(timeout.count()));
    dsbproto::execution_controller::StepReturn ret;
    dsb::inproc_rpc::Call(*m_rpcSocket, STEP_CALL, &args, &ret);

    if (slaveResults) {
        slaveResults->clear();
        std::transform(
            ret.slave_result().begin(),
            ret.slave_result().end(),
            std::back_inserter(*slaveResults),
            &ConvSlaveResult);
    }
    return ConvStepResult(ret.result());
}


void dsb::execution::Controller::AcceptStep(std::chrono::milliseconds timeout)
{
    dsbproto::execution_controller::AcceptStepArgs args;
    args.set_timeout_ms(boost::numeric_cast<std::int32_t>(timeout.count()));
    dsb::inproc_rpc::Call(*m_rpcSocket, ACCEPT_STEP_CALL, &args);
}


// =============================================================================
// SpawnExecution()
// =============================================================================


dsb::net::ExecutionLocator dsb::execution::SpawnExecution(
    const dsb::net::DomainLocator& domainLocator,
    const std::string& executionName,
    std::chrono::seconds commTimeout)
{
    if (commTimeout <= std::chrono::seconds(0)) {
        throw std::invalid_argument("Communications timeout interval is nonpositive");
    }
    const auto actualExecName = executionName.empty()
        ? dsb::util::Timestamp()
        : executionName;

    auto sck = zmq::socket_t(dsb::comm::GlobalContext(), ZMQ_REQ);
    sck.connect(domainLocator.ExecReqEndpoint().c_str());

    std::vector<zmq::message_t> msg;
    msg.push_back(dsb::comm::ToFrame("SPAWN_EXECUTION"));
    msg.push_back(zmq::message_t());
    dsbproto::broker::SpawnExecutionData seData;
    seData.set_execution_name(actualExecName);
    seData.set_comm_timeout_seconds(commTimeout.count());
    dsb::protobuf::SerializeToFrame(seData, msg.back());

    dsb::comm::Send(sck, msg);
    if (!dsb::comm::Receive(sck, msg, std::chrono::seconds(10))) {
        throw std::runtime_error("Failed to spawn execution (domain connection timed out)");
    }
    const auto reply = dsb::comm::ToString(msg.front());
    if (reply == "SPAWN_EXECUTION_OK" && msg.size() == 2) {
        dsbproto::broker::SpawnExecutionOkData seOkData;
        dsb::protobuf::ParseFromFrame(msg.back(), seOkData);

        const auto endpointBase = domainLocator.ExecReqEndpoint().substr(
            0,
            domainLocator.ExecReqEndpoint().rfind(':'));
        return dsb::net::ExecutionLocator(
            endpointBase + ':' + std::to_string(seOkData.master_port()),
            endpointBase + ':' + std::to_string(seOkData.slave_port()),
            endpointBase + ':' + std::to_string(seOkData.variable_pub_port()),
            endpointBase + ':' + std::to_string(seOkData.variable_sub_port()),
            domainLocator.ExecReqEndpoint(),
            actualExecName,
            commTimeout);
    } else if (reply == "SPAWN_EXECUTION_FAIL" && msg.size() == 2) {
        dsbproto::broker::SpawnExecutionFailData seFailData;
        dsb::protobuf::ParseFromFrame(msg.back(), seFailData);
        throw std::runtime_error(
            "Failed to spawn execution (" + seFailData.reason() + ')');
    } else {
        throw std::runtime_error("Failed to spawn execution (invalid response from domain)");
    }
}
