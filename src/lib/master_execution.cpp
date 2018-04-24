/*
Copyright 2013-2017, SINTEF Ocean and the Coral contributors.
This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#include <coral/master/execution.hpp>

#include <exception>
#include <stdexcept>
#include <utility>

#include <zmq.hpp>

#include <coral/async.hpp>
#include <coral/bus/execution_manager.hpp>
#include <coral/net/reactor.hpp>
#include <coral/log.hpp>


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


class coral::master::Execution::Private
{
public:
    /// Constructor.
    Private(
        const std::string& executionName,
        const ExecutionOptions& options)
        : m_thread{}
    {
        m_thread.Execute<void>(
            [&] (
                coral::net::Reactor& reactor,
                ExecMgr& execMgr,
                std::promise<void> status)
            {
                try {
                    execMgr = std::make_unique<coral::bus::ExecutionManager>(
                        reactor,
                        executionName,
                        options);
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
                coral::net::Reactor&,
                ExecMgr& execMgr,
                std::promise<void> promise)
            {
                const auto sharedPromise =
                    std::make_shared<decltype(promise)>(std::move(promise));
                try {
                    std::vector<coral::bus::AddedSlave> slavesToAdd2;
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
                            (const std::error_code& ec, coral::model::SlaveID id, std::size_t index)
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
                coral::net::Reactor&,
                ExecMgr& execMgr,
                std::promise<void> promise)
            {
                const auto sharedPromise =
                    std::make_shared<decltype(promise)>(std::move(promise));
                try {
                    std::vector<coral::bus::SlaveConfig> slaveConfigs2;
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
                            (const std::error_code& ec, coral::model::SlaveID id, std::size_t index)
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
        coral::model::TimeDuration stepSize,
        std::chrono::milliseconds timeout,
        std::vector<std::pair<coral::model::SlaveID, StepResult>>* slaveResults)
    {
        return m_thread.Execute<StepResult>(
            [=] (
                coral::net::Reactor&,
                ExecMgr& execMgr,
                std::promise<StepResult> promise)
            {
                std::function<void(const std::error_code&, coral::model::SlaveID)>
                    perSlaveHandler = [slaveResults]
                        (const std::error_code& ec, coral::model::SlaveID slaveID)
                    {
                        if (!ec) {
                            if (slaveResults) {
                                slaveResults->push_back(
                                    std::make_pair(slaveID, StepResult::completed));
                            }
                        } else if (ec == coral::error::sim_error::cannot_perform_timestep
                                && slaveResults) {
                            slaveResults->push_back(
                                std::make_pair(slaveID, StepResult::failed));
                        } else {
                            coral::log::Log(
                                coral::log::error,
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
                        if (!ec || ec == coral::error::sim_error::cannot_perform_timestep) {
                            sharedPromise->set_value(ec == coral::error::sim_error::cannot_perform_timestep
                                ? StepResult::failed
                                : StepResult::completed);
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
                coral::net::Reactor&,
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
            [] (coral::net::Reactor&, ExecMgr& execMgr, std::promise<void> promise)
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
    using ExecMgr = std::unique_ptr<coral::bus::ExecutionManager>;
    coral::async::CommThread<ExecMgr> m_thread;
};



coral::master::Execution::Execution(
    const std::string& executionName,
    const ExecutionOptions& options)
    : m_private{std::make_unique<Private>(executionName, options)}
{
}


coral::master::Execution::~Execution() noexcept
{
}


coral::master::Execution::Execution(Execution&& other) noexcept
    : m_private(std::move(other.m_private))
{
}


coral::master::Execution& coral::master::Execution::operator=(Execution&& other)
    noexcept
{
    m_private = std::move(other.m_private);
    return *this;
}


void coral::master::Execution::Reconstitute(
    std::vector<AddedSlave>& slavesToAdd,
    std::chrono::milliseconds commTimeout)
{
    m_private->Reconstitute(slavesToAdd, commTimeout);
}


void coral::master::Execution::Reconfigure(
    std::vector<SlaveConfig>& slaveConfigs,
    std::chrono::milliseconds commTimeout)
{
    m_private->Reconfigure(slaveConfigs, commTimeout);
}


coral::master::StepResult coral::master::Execution::Step(
    coral::model::TimeDuration stepSize,
    std::chrono::milliseconds timeout,
    std::vector<std::pair<coral::model::SlaveID, StepResult>>* slaveResults)
{
    return m_private->Step(stepSize, timeout, slaveResults);
}


void coral::master::Execution::AcceptStep(std::chrono::milliseconds timeout)
{
    return m_private->AcceptStep(timeout);
}


void coral::master::Execution::Terminate()
{
    m_private->Terminate();
}
