#include "dsb/execution/controller.hpp"

#include <exception>
#include <stdexcept>
#include <utility>

#include "zmq.hpp"

#include "dsb/async.hpp"
#include "dsb/bus/execution_manager.hpp"
#include "dsb/net/reactor.hpp"
#include "dsb/log.hpp"


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
    Private(
        const std::string& executionName,
        dsb::model::TimePoint startTime,
        dsb::model::TimePoint maxTime)
        : m_thread{}
    {
        m_thread.Execute<void>(
            [&] (
                dsb::net::Reactor& reactor,
                ExecMgr& execMgr,
                std::promise<void> status)
            {
                try {
                    execMgr = std::make_unique<dsb::bus::ExecutionManager>(
                        reactor,
                        executionName,
                        startTime,
                        maxTime);
                    status.set_value();
                } catch (...) {
                    status.set_exception(std::current_exception());
                }
            }).get();
    }

    Private(const Private&) = delete;
    Private& operator=(const Private&) = delete;
    Private(Private&&) = delete;
    Private& operator=(Private&&) = delete;

    void Reconstitute(
        std::vector<AddedSlave>& slavesToAdd,
        std::chrono::milliseconds timeout)
    {
        m_thread.Execute<void>(
            [&slavesToAdd, timeout] (
                dsb::net::Reactor&,
                ExecMgr& execMgr,
                std::promise<void> promise)
            {
                const auto sharedPromise =
                    std::make_shared<decltype(promise)>(std::move(promise));
                try {
                    std::vector<dsb::bus::AddedSlave> slavesToAdd2;
                    for (const auto& sta : slavesToAdd) {
                        slavesToAdd2.emplace_back(sta.locator, sta.name);
                    }
                    execMgr->Reconstitute(
                        slavesToAdd2,
                        timeout,
                        [sharedPromise] (const std::error_code& ec) {
                            if (!ec) {
                                sharedPromise->set_value();
                            } else {
                                SetException(
                                    *sharedPromise,
                                    std::runtime_error(
                                        ErrMsg("Failed to perform reconstitution", ec)));
                            }
                        },
                        [&slavesToAdd]
                            (const std::error_code& ec, dsb::model::SlaveID id, std::size_t index)
                        {
                            slavesToAdd[index].error = ec;
                            slavesToAdd[index].id = id;
                        });
                } catch (...) {
                    sharedPromise->set_exception(std::current_exception());
                }
            }
        ).get();
    }

    void Reconfigure(
        std::vector<SlaveConfig>& slaveConfigs,
        std::chrono::milliseconds timeout)
    {
        m_thread.Execute<void>(
            [&slaveConfigs, timeout] (
                dsb::net::Reactor&,
                ExecMgr& execMgr,
                std::promise<void> promise)
            {
                const auto sharedPromise =
                    std::make_shared<decltype(promise)>(std::move(promise));
                try {
                    std::vector<dsb::bus::SlaveConfig> slaveConfigs2;
                    for (const auto& sc : slaveConfigs) {
                        slaveConfigs2.emplace_back(
                            sc.slaveID,
                            sc.variableSettings);
                    }
                    execMgr->Reconfigure(
                        slaveConfigs2,
                        timeout,
                        [sharedPromise] (const std::error_code& ec) {
                            if (!ec) {
                                sharedPromise->set_value();
                            } else {
                                SetException(
                                    *sharedPromise,
                                    std::runtime_error(
                                        ErrMsg("Failed to perform reconfiguration", ec)));
                            }
                        },
                        [&slaveConfigs]
                            (const std::error_code& ec, dsb::model::SlaveID id, std::size_t index)
                        {
                            assert(slaveConfigs[index].slaveID = id);
                            slaveConfigs[index].error = ec;
                        });
                } catch (...) {
                    sharedPromise->set_exception(std::current_exception());
                }
            }
        ).get();
    }


    StepResult Step(
        dsb::model::TimeDuration stepSize,
        std::chrono::milliseconds timeout,
        std::vector<std::pair<dsb::model::SlaveID, StepResult>>* slaveResults)
    {
        return m_thread.Execute<StepResult>(
            [=] (
                dsb::net::Reactor&,
                ExecMgr& execMgr,
                std::promise<StepResult> promise)
            {
                std::function<void(const std::error_code&, dsb::model::SlaveID)>
                    perSlaveHandler = [slaveResults]
                        (const std::error_code& ec, dsb::model::SlaveID slaveID)
                    {
                        if (!ec) {
                            if (slaveResults) {
                                slaveResults->push_back(
                                    std::make_pair(slaveID, STEP_COMPLETE));
                            }
                        } else if (ec == dsb::error::sim_error::cannot_perform_timestep
                                && slaveResults) {
                            slaveResults->push_back(
                                std::make_pair(slaveID, STEP_FAILED));
                        } else {
                            dsb::log::Log(
                                dsb::log::error,
                                boost::format("Slave %d failed to perform time step (%s)")
                                    % slaveID
                                    % ec.message());
                        }
                    };

                auto sharedPromise =
                    std::make_shared<decltype(promise)>(std::move(promise));
                execMgr->Step(
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
            [timeout] (
                dsb::net::Reactor&,
                ExecMgr& execMgr,
                std::promise<void> promise)
            {
                try {
                    execMgr->AcceptStep(
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


    void Terminate()
    {
        m_thread.Execute<void>(
            [] (dsb::net::Reactor&, ExecMgr& execMgr, std::promise<void> promise)
            {
                try {
                    execMgr->Terminate();
                    promise.set_value();
                } catch (...) {
                    promise.set_exception(std::current_exception());
                }
            }
        ).get();
        m_thread.Shutdown();
    }

private:
    // TODO: Replace std::unique_ptr with boost::optional (when we no longer
    //       need to support Boost < 1.56) or std::optional (when all our
    //       compilers support it).
    using ExecMgr = std::unique_ptr<dsb::bus::ExecutionManager>;
    dsb::async::CommThread<ExecMgr> m_thread;
};



dsb::execution::Controller::Controller(
    const std::string& executionName,
    dsb::model::TimePoint startTime,
    dsb::model::TimePoint maxTime)
    : m_private{std::make_unique<Private>(executionName, startTime, maxTime)}
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


void dsb::execution::Controller::Reconstitute(
    std::vector<AddedSlave>& slavesToAdd,
    std::chrono::milliseconds commTimeout)
{
    m_private->Reconstitute(slavesToAdd, commTimeout);
}


void dsb::execution::Controller::Reconfigure(
    std::vector<SlaveConfig>& slaveConfigs,
    std::chrono::milliseconds commTimeout)
{
    m_private->Reconfigure(slaveConfigs, commTimeout);
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


void dsb::execution::Controller::Terminate()
{
    m_private->Terminate();
}
