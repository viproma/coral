#include "dsb/execution/controller.hpp"

#include <exception>
#include <stdexcept>
#include <utility>

#include "zmq.hpp"

#include "dsb/async.hpp"
#include "dsb/bus/execution_manager.hpp"
#include "dsb/comm/reactor.hpp"

// For SpawnExecution() only
#include "dsb/comm/messaging.hpp"
#include "dsb/comm/util.hpp"
#include "dsb/protobuf.hpp"
#include "dsb/util.hpp"

#include "broker.pb.h"


// =============================================================================
// Controller
// =============================================================================


namespace
{
    std::string ErrMsg(const std::string& message, std::error_code code)
    {
        return message + " (" + code.message() + ")";
    }

    template<typename T, typename E>
    void SetException(std::promise<T>& promise, E exception)
    {
        promise.set_exception(std::make_exception_ptr(std::move(exception)));
    }

    std::function<void(const std::error_code&)> SimpleHandler(
        std::promise<void> promise,
        const std::string& errMsg)
    {
        // Note: This is because VS2013 doesn't support moving into lambdas.
        auto sharedPromise =
            std::make_shared<decltype(promise)>(std::move(promise));
        return [=] (const std::error_code& ec)
        {
            if (ec) {
                SetException(*sharedPromise, std::runtime_error(ErrMsg(errMsg, ec)));
            } else {
                sharedPromise->set_value();
            }
        };
    }
}


class dsb::execution::Controller::Private
{
public:
    /// Constructor.
    explicit Private(const dsb::net::ExecutionLocator& locator)
        : m_execLocator(locator)
        , m_execMgr{locator}
        , m_thread{}
    {
    }


    ~Private() DSB_NOEXCEPT { }


    Private(const Private&) = delete;
    Private& operator=(const Private&) = delete;
    Private(Private&&) = delete;
    Private& operator=(Private&&) = delete;


    void Terminate()
    {
        m_thread.Execute<void>(
            [this] (dsb::comm::Reactor&, std::promise<void> promise)
            {
                try {
                    m_execMgr.Terminate();
                    promise.set_value();
                } catch (...) {
                    promise.set_exception(std::current_exception());
                }
            }
        ).get();
        m_thread.Shutdown();

        // Terminate the execution broker
        auto execTerminationSocket = zmq::socket_t(dsb::comm::GlobalContext(), ZMQ_REQ);
        execTerminationSocket.connect(m_execLocator.ExecTerminationEndpoint().c_str());
        std::vector<zmq::message_t> termMsg;
        termMsg.push_back(dsb::comm::ToFrame("TERMINATE_EXECUTION"));
        termMsg.push_back(zmq::message_t());
        dsbproto::broker::TerminateExecutionData teData;
        teData.set_execution_name(m_execLocator.ExecName());
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
    }


    void BeginConfig()
    {
        m_thread.Execute<void>(
            [this] (dsb::comm::Reactor&, std::promise<void> promise)
            {
                try {
                    m_execMgr.BeginConfig(
                        SimpleHandler(
                            std::move(promise),
                            "Failed to enter configuration mode"));
                } catch (...) {
                    promise.set_exception(std::current_exception());
                }
            }
        ).get();
    }


    void EndConfig()
    {
        m_thread.Execute<void>(
            [this] (dsb::comm::Reactor&, std::promise<void> promise)
            {
                try {
                    m_execMgr.EndConfig(
                        SimpleHandler(
                            std::move(promise),
                            "Failed to leave configuration mode"));
                } catch (...) {
                    promise.set_exception(std::current_exception());
                }
            }
        ).get();
    }


    void SetSimulationTime(
        dsb::model::TimePoint startTime,
        dsb::model::TimePoint stopTime)
    {
        m_thread.Execute<void>(
            [=] (dsb::comm::Reactor&, std::promise<void> promise)
            {
                try {
                    m_execMgr.SetSimulationTime(startTime, stopTime);
                    promise.set_value();
                } catch (...) {
                    promise.set_exception(std::current_exception());
                }
            }
        ).get();
    }


    std::future<dsb::model::SlaveID> AddSlave(
        dsb::net::SlaveLocator slaveLocator,
        const std::string& slaveName,
        std::chrono::milliseconds commTimeout)
    {
        return m_thread.Execute<dsb::model::SlaveID>(
            [=] (
                dsb::comm::Reactor& reactor,
                std::promise<dsb::model::SlaveID> promise)
            {
                auto sharedPromise =
                    std::make_shared<decltype(promise)>(std::move(promise));
                m_execMgr.AddSlave(
                    slaveLocator,
                    slaveName,
                    reactor,
                    commTimeout,
                    [slaveName, sharedPromise]
                        (const std::error_code& ec, dsb::model::SlaveID id)
                    {
                        if (!ec) {
                            sharedPromise->set_value(id);
                        } else {
                            SetException(
                                *sharedPromise,
                                std::runtime_error(ErrMsg(
                                    "Failed to add slave: " + slaveName,
                                    ec)));
                        }
                    });
            }
        );
    }


    std::future<void> SetVariables(
        dsb::model::SlaveID slave,
        const std::vector<dsb::model::VariableSetting>& variableSettings,
        std::chrono::milliseconds timeout)
    {
        return m_thread.Execute<void>(
            [=] (
                dsb::comm::Reactor& reactor,
                std::promise<void> promise)
            {
                auto sharedPromise =
                    std::make_shared<decltype(promise)>(std::move(promise));
                m_execMgr.SetVariables(
                    slave,
                    variableSettings,
                    timeout,
                    [slave, sharedPromise, this] (const std::error_code& ec)
                    {
                        if (!ec) {
                            sharedPromise->set_value();
                        } else {
                            SetException(
                                *sharedPromise,
                                std::runtime_error(ErrMsg(
                                    "Failed to set/connect variables for slave: "
                                    + m_execMgr.SlaveName(slave),
                                    ec)));
                        }
                    });
            }
        );
    }


    StepResult Step(
        dsb::model::TimeDuration stepSize,
        std::chrono::milliseconds timeout,
        std::vector<std::pair<dsb::model::SlaveID, StepResult>>* slaveResults)
    {
        return m_thread.Execute<StepResult>(
            [=] (dsb::comm::Reactor&, std::promise<StepResult> promise)
            {
                // Only define a per-slave handler if we have somewhere to
                // put the results.
                std::function<void(const std::error_code&, dsb::model::SlaveID)>
                    perSlaveHandler;
                if (slaveResults) {
                    perSlaveHandler = [slaveResults]
                        (const std::error_code& ec, dsb::model::SlaveID slaveID)
                    {
                        if (!ec) {
                            slaveResults->push_back(
                                std::make_pair(slaveID, STEP_COMPLETE));
                        } else if (ec == dsb::error::sim_error::cannot_perform_timestep) {
                            slaveResults->push_back(
                                std::make_pair(slaveID, STEP_FAILED));
                        }
                    };
                }

                auto sharedPromise =
                    std::make_shared<decltype(promise)>(std::move(promise));
                m_execMgr.Step(
                    stepSize,
                    timeout,
                    [sharedPromise] (const std::error_code& ec)
                    {
                        if (!ec || ec == dsb::error::sim_error::cannot_perform_timestep) {
                            sharedPromise->set_value(ec == dsb::error::sim_error::cannot_perform_timestep
                                ? STEP_FAILED
                                : STEP_COMPLETE);
                        } else {
                            SetException(
                                *sharedPromise,
                                std::runtime_error(
                                    ErrMsg("Failed to perform time step", ec)));
                        }
                    },
                    std::move(perSlaveHandler));
            }
        ).get();
    }


    void AcceptStep(std::chrono::milliseconds timeout)
    {
        m_thread.Execute<void>(
            [=] (dsb::comm::Reactor&, std::promise<void> promise)
            {
                try {
                    m_execMgr.AcceptStep(
                        timeout,
                        SimpleHandler(
                            std::move(promise),
                            "Failed to complete time step"));
                } catch (...) {
                    promise.set_exception(std::current_exception());
                }
            }
        ).get();
    }

private:
    dsb::net::ExecutionLocator m_execLocator;

    // For use in background thread
    dsb::bus::ExecutionManager m_execMgr;

    // Background thread
    dsb::async::CommThread m_thread;
};



dsb::execution::Controller::Controller(
    const dsb::net::ExecutionLocator& locator)
    : m_private{std::make_unique<Private>(locator)}
{
}


dsb::execution::Controller::~Controller() DSB_NOEXCEPT
{
}


dsb::execution::Controller::Controller(Controller&& other) DSB_NOEXCEPT
    : m_private(std::move(other.m_private))
{
}


dsb::execution::Controller& dsb::execution::Controller::operator=(Controller&& other)
    DSB_NOEXCEPT
{
    m_private = std::move(other.m_private);
    return *this;
}


void dsb::execution::Controller::Terminate()
{
    m_private->Terminate();
}


void dsb::execution::Controller::BeginConfig()
{
    m_private->BeginConfig();
}


void dsb::execution::Controller::EndConfig()
{
    m_private->EndConfig();
}


void dsb::execution::Controller::SetSimulationTime(
    dsb::model::TimePoint startTime,
    dsb::model::TimePoint stopTime)
{
    m_private->SetSimulationTime(startTime, stopTime);
}


std::future<dsb::model::SlaveID> dsb::execution::Controller::AddSlave(
    dsb::net::SlaveLocator slaveLocator,
    const std::string& slaveName,
    std::chrono::milliseconds commTimeout)
{
    return m_private->AddSlave(slaveLocator, slaveName, commTimeout);
}


std::future<void> dsb::execution::Controller::SetVariables(
    dsb::model::SlaveID slave,
    const std::vector<dsb::model::VariableSetting>& variableSettings,
    std::chrono::milliseconds timeout)
{
    return m_private->SetVariables(slave, variableSettings, timeout);
}


dsb::execution::StepResult dsb::execution::Controller::Step(
    dsb::model::TimeDuration stepSize,
    std::chrono::milliseconds timeout,
    std::vector<std::pair<dsb::model::SlaveID, StepResult>>* slaveResults)
{
    return m_private->Step(stepSize, timeout, slaveResults);
}


void dsb::execution::Controller::AcceptStep(std::chrono::milliseconds timeout)
{
    return m_private->AcceptStep(timeout);
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
