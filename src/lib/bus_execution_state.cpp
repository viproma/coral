/*
Copyright 2013-2017, SINTEF Ocean and the Coral contributors.
This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#define NOMINMAX
#include "coral/bus/execution_state.hpp"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "coral/bus/execution_manager_private.hpp"
#include "coral/bus/slave_control_messenger.hpp"
#include "coral/bus/slave_controller.hpp"
#include "coral/log.hpp"
#include "coral/util.hpp"


namespace coral
{
namespace bus
{


namespace
{
    std::string DataTypeName(coral::model::DataType dt)
    {
        switch (dt) {
            case coral::model::REAL_DATATYPE:    return "real";
            case coral::model::INTEGER_DATATYPE: return "integer";
            case coral::model::BOOLEAN_DATATYPE: return "boolean";
            case coral::model::STRING_DATATYPE:  return "string";
        }
        return std::string();
    }

    std::string CausalityName(coral::model::Causality c)
    {
        switch (c) {
            case coral::model::PARAMETER_CAUSALITY:            return "parameter";
            case coral::model::CALCULATED_PARAMETER_CAUSALITY: return "calculated parameter";
            case coral::model::INPUT_CAUSALITY:                return "input";
            case coral::model::OUTPUT_CAUSALITY:               return "output";
            case coral::model::LOCAL_CAUSALITY:                return "local";
        }
        return std::string();
    }

    void VerifyDataTypeMatch(
        coral::model::DataType expected,
        coral::model::DataType actual,
        const std::string& slaveName,
        const std::string& varName,
        const char* action)
    {
        if (expected != actual) {
            std::stringstream sst;
            sst << "Failed to " << action << ' ' << slaveName << '.' << varName
                << " due to data type mismatch. Expected: "
                << DataTypeName(expected) << ". Actual: "
                << DataTypeName(actual);
            throw std::runtime_error(sst.str());
        }
    }

    void VerifyCausalityMatch(
        coral::model::Causality inputCausality,
        coral::model::Causality outputCausality,
        const std::string& slaveName,
        const std::string& varName)
    {
        std::string reason;
        if ((inputCausality == coral::model::INPUT_CAUSALITY
                && (outputCausality == coral::model::OUTPUT_CAUSALITY
                    || outputCausality == coral::model::CALCULATED_PARAMETER_CAUSALITY))
            || (inputCausality == coral::model::PARAMETER_CAUSALITY
                && outputCausality == coral::model::CALCULATED_PARAMETER_CAUSALITY)) {
            // All right, these causalities are compatible
            return;
        }
        std::stringstream sst;
        sst << "Failed to connect " << slaveName << '.' << varName
            << " due to causality incompatibility. Cannot make a connection from a variable with causality \""
            << CausalityName(outputCausality) << "\" to a variable with causality \""
            << CausalityName(inputCausality) << '"';
        throw std::runtime_error(sst.str());
    }

    void VerifyVariableSetting(
        const ExecutionManagerPrivate& self,
        coral::model::SlaveID slaveID,
        const coral::model::VariableSetting& setting)
    {
        const auto sit = self.slaves.find(slaveID);
        if (sit == self.slaves.end()) {
            throw std::runtime_error("Invalid slave ID: " + std::to_string(slaveID));
        }
        const auto& slaveDesc = sit->second.description;
        const auto& slaveType = slaveDesc.TypeDescription();
        const auto& varDesc = slaveType.Variable(setting.Variable());
        if (setting.HasValue()) {
            VerifyDataTypeMatch(
                varDesc.DataType(),
                coral::model::DataTypeOf(setting.Value()),
                slaveDesc.Name(),
                varDesc.Name(),
                "set value of");
        }
        if (setting.IsConnected()) {
            const auto oit = self.slaves.find(setting.ConnectedOutput().Slave());
            if (oit == self.slaves.end()) {
                throw std::runtime_error(
                    "Failed to connect " + slaveDesc.Name() + '.' + varDesc.Name()
                    + " due to an invalid slave ID");
            }
            const auto& otherDesc = oit->second.description;
            const auto& otherType = otherDesc.TypeDescription();
            const coral::model::VariableDescription* otherVarDesc = nullptr;
            try {
                otherVarDesc = &otherType.Variable(setting.ConnectedOutput().ID());
            } catch (const std::out_of_range&) {
                throw std::runtime_error(
                    "Failed to connect " + slaveDesc.Name() + '.' + varDesc.Name()
                    + " due to an invalid variable ID");
            }
            VerifyDataTypeMatch(
                varDesc.DataType(),
                otherVarDesc->DataType(),
                slaveDesc.Name(),
                varDesc.Name(),
                "connect");
            VerifyCausalityMatch(
                varDesc.Causality(),
                otherVarDesc->Causality(),
                slaveDesc.Name(),
                varDesc.Name());
        }
    }

    void VerifyVariableSettings(
        const ExecutionManagerPrivate& self,
        coral::model::SlaveID slaveID,
        const std::vector<coral::model::VariableSetting>& settings)
    {
        for (const auto& setting : settings) {
            VerifyVariableSetting(self, slaveID, setting);
        }
    }
}


void ReadyExecutionState::Reconstitute(
    ExecutionManagerPrivate& self,
    const std::vector<AddedSlave>& slavesToAdd,
    std::chrono::milliseconds commTimeout,
    ExecutionManager::ReconstituteHandler onComplete,
    ExecutionManager::SlaveReconstituteHandler onSlaveComplete)
{
    if (slavesToAdd.empty()) {
        onComplete(std::error_code{});
        return;
    }
    self.SwapState(std::make_unique<ReconstitutingExecutionState>(
        slavesToAdd,
        commTimeout,
        std::move(onComplete),
        std::move(onSlaveComplete)));
}


void ReadyExecutionState::Reconfigure(
    ExecutionManagerPrivate& self,
    const std::vector<SlaveConfig>& slaveConfigs,
    std::chrono::milliseconds commTimeout,
    ExecutionManager::ReconstituteHandler onComplete,
    ExecutionManager::SlaveReconstituteHandler onSlaveComplete)
{
    if (slaveConfigs.empty()) {
        onComplete(std::error_code{});
        return;
    }
    // TODO: Maybe hoist this check to ExecutionManagerPrivate?
    for (const auto& sc : slaveConfigs) {
        VerifyVariableSettings(self, sc.slaveID, sc.variableSettings);
    }
    self.SwapState(std::make_unique<ReconfiguringExecutionState>(
        slaveConfigs,
        commTimeout,
        std::move(onComplete),
        std::move(onSlaveComplete)));
}


void ReadyExecutionState::ResendVars(
    ExecutionManagerPrivate& self,
    int maxAttempts,
    std::chrono::milliseconds commTimeout,
    std::function<void(const std::error_code&)> onComplete)
{
    self.SwapState(std::make_unique<PrimingExecutionState>(
        maxAttempts, commTimeout, onComplete));
}


void ReadyExecutionState::Step(
    ExecutionManagerPrivate& self,
    coral::model::TimeDuration stepSize,
    std::chrono::milliseconds timeout,
    ExecutionManager::StepHandler onComplete,
    ExecutionManager::SlaveStepHandler onSlaveStepComplete)
{
    self.SwapState(std::make_unique<SteppingExecutionState>(
        stepSize, timeout, std::move(onComplete), std::move(onSlaveStepComplete)));
}


void ReadyExecutionState::Terminate(ExecutionManagerPrivate& self)
{
    self.DoTerminate();
}


// =============================================================================

namespace
{
    // Small struct to keep track of the number of ongoing and failed per-slave
    // operations in the Reconstitute and Reconfigure states.
    struct OpTally
    {
        int ongoing = 0;
        int failed = 0;
    };

    /*
    Helper function for ReconstitutingExecutionState which performs the
    following tasks for ONE slave:

     1. Creates a SlaveController object and thus initiates the connection.

     2. When the SlaveController is connected, calls its GetDescription()
        function to populate the ExecutionManager's slave description cache.

     3. Once GetDescription() has completed, calls the given onComplete
        callback with the slave's assigned ID.

    If any of these operations fail, the callback will be called with an
    error code and an invalid ID, and any remaining operations will not be
    carried out.  The slave controller will remain in the slave list
    (ExecutionManagerPrivate::slaves), but it will be in a non-connected
    state.
    */
    coral::model::SlaveID AddSlave(
        ExecutionManagerPrivate& self,
        const AddedSlave& slave,
        std::chrono::milliseconds commTimeout,
        std::function<void(const std::error_code&, coral::model::SlaveID)> onComplete)
    {
        if (!slave.name.empty() && !coral::model::IsValidSlaveName(slave.name)) {
            throw std::invalid_argument(
                '"' + slave.name + "\" is not a valid slave name");
        }
        const auto id = ++self.lastSlaveID;
        assert(self.slaves.count(id) == 0);
        const auto& realName = slave.name.empty()
            ? "_slave" + std::to_string(id)
            : slave.name;
        for (const auto& s : self.slaves) {
            if (realName == s.second.description.Name()) {
                throw std::runtime_error("Duplicate slave name: " + realName);
            }
        }

        const auto selfPtr = &self; // For use in lambdas

        // Once we have connected the slave, we want to immediately request some
        // info from it.  We define the handler for that operation here, for
        // readability's sake.
        SlaveController::GetDescriptionHandler onDescriptionReceived =
            [selfPtr, onComplete, id]
            (const std::error_code& ec, const coral::model::SlaveDescription& sd)
        {
            if (!ec) {
                selfPtr->slaves.at(id).description
                    .SetTypeDescription(sd.TypeDescription());
                onComplete(ec, id);
            } else {
                selfPtr->slaves.at(id).slave->Terminate();
                onComplete(ec, coral::model::INVALID_SLAVE_ID);
            }
        };

        // This is the handler for the connection operation. Note that we pass
        // onDescriptionReceived into it and use it if the connection succeeds.
        SlaveController::ConnectHandler onConnected =
            [selfPtr, commTimeout, onComplete, id, onDescriptionReceived]
            (const std::error_code& ec)
        {
            if (!ec) {
                selfPtr->slaves.at(id).slave->GetDescription(
                    commTimeout,
                    std::move(onDescriptionReceived));
            } else {
                assert(selfPtr->slaves.at(id).slave->State() == SLAVE_NOT_CONNECTED);
                onComplete(ec, coral::model::INVALID_SLAVE_ID);
            }
        };

        // Initiate the connection and add the slave to the slave list
        auto slaveController = std::make_unique<coral::bus::SlaveController>(
            self.reactor,
            slave.locator,
            id,
            realName,
            self.slaveSetup,
            commTimeout,
            std::move(onConnected));
        self.slaves.insert(std::make_pair(
            id,
            ExecutionManagerPrivate::Slave(
                std::move(slaveController),
                slave.locator,
                coral::model::SlaveDescription(id, realName))));
        return id;
    }
}


ReconstitutingExecutionState::ReconstitutingExecutionState(
    const std::vector<AddedSlave>& slavesToAdd,
    std::chrono::milliseconds commTimeout,
    ExecutionManager::ReconstituteHandler onComplete,
    ExecutionManager::SlaveReconstituteHandler onSlaveComplete)
    : m_slavesToAdd(slavesToAdd)
    , m_commTimeout{commTimeout}
    , m_onComplete{std::move(onComplete)}
    , m_onSlaveComplete{std::move(onSlaveComplete)}
{
    assert(!slavesToAdd.empty());
}


void ReconstitutingExecutionState::StateEntered(
    ExecutionManagerPrivate& self)
{
    assert(std::numeric_limits<coral::model::SlaveID>::max() - self.lastSlaveID
            >= (int) m_slavesToAdd.size());

    // We call AddSlave() for each of the slaves to be added, using opTally
    // to keep track of how many such operations we have ongoing, and how
    // many have failed.  When the first of those reaches zero, we move to
    // the next stage by calling AllSlavesAdded(), unless the error count is
    // nonzero.
    //
    // m_addedSlaves contains the IDs of the slaves we have added.  If any
    // per-slave operations fail in the AddSlave() step, the per-slave handler
    // is called with an error code and the corresponding m_addedSlaves
    // element is reset to INVALID_SLAVE_ID.
    const auto selfPtr = &self;
    const auto opTally = std::make_shared<OpTally>();
    for (std::size_t index = 0; index < m_slavesToAdd.size(); ++index) {
        auto addedSlaveID = AddSlave(
            self,
            m_slavesToAdd[index],
            m_commTimeout,
            [selfPtr, opTally, index, this]
                (const std::error_code& ec, coral::model::SlaveID id)
            {
                --(opTally->ongoing);
                if (ec) {
                    ++(opTally->failed);
                    m_addedSlaves[index] = coral::model::INVALID_SLAVE_ID;
                    m_onSlaveComplete(ec, coral::model::INVALID_SLAVE_ID, index);
                }
                if (opTally->ongoing == 0) {
                    if (opTally->failed == 0) {
                        AllSlavesAdded(*selfPtr);
                    } else {
                        Failed(*selfPtr);
                    }
                }
            });
        ++(opTally->ongoing);
        m_addedSlaves.push_back(addedSlaveID);
    }
}


void ReconstitutingExecutionState::AllSlavesAdded(
    ExecutionManagerPrivate& self)
{
    // Build a list that contains the endpoints on which the slaves
    // publish their variable values.
    std::vector<coral::net::Endpoint> peers;
    for (const auto& slave : self.slaves) {
        if (slave.second.slave->State() != SLAVE_NOT_CONNECTED) {
            peers.push_back(slave.second.locator.DataPubEndpoint());
        }
    }

    // Send that list to all the slaves.  We use opTally to keep track of
    // the number of ongoing operations as well as the number of failed
    // operations.  The latter is needed because any failure should be
    // counted as fatal -- the simulation is not likely to run if one of
    // the slaves is not in contact with the others.
    //
    // When all per-slave operations are done, we move to the final stage
    // by calling Completed() if all went well, otherwise we call Failed().
    const auto selfPtr = &self;
    const auto opTally = std::make_shared<OpTally>();
    for (auto& slave : self.slaves) {
        const auto slaveName = slave.second.description.Name();
        slave.second.slave->SetPeers(
            peers,
            m_commTimeout,
            [selfPtr, opTally, slaveName, this] (const std::error_code& ec)
            {
                --(opTally->ongoing);
                if (ec) {
                    ++(opTally->failed);
                    coral::log::Log(coral::log::error,
                        boost::format("Failed to send SET_PEERS command to slave '%s': %s")
                        % slaveName % ec.message());
                }
                if (opTally->ongoing == 0) {
                    if (opTally->failed == 0) {
                        Completed(*selfPtr);
                    } else {
                        Failed(*selfPtr);
                    }
                }
            });
        ++(opTally->ongoing);
    }
}


void ReconstitutingExecutionState::Completed(
    ExecutionManagerPrivate& self)
{
    // Call the remaining per-slave handlers.  Those elements in
    // m_addedSlave for which the handler callback has already been called
    // have already been set to INVALID_SLAVE_ID.
    for (std::size_t index = 0; index < m_addedSlaves.size(); ++index) {
        const auto id = m_addedSlaves[index];
        if (id != coral::model::INVALID_SLAVE_ID) {
            m_onSlaveComplete(std::error_code{}, id, index);
        }
    }
    m_onComplete(std::error_code{});
    self.SwapState(std::make_unique<ReadyExecutionState>());
}


void ReconstitutingExecutionState::Failed(
    ExecutionManagerPrivate& self)
{
    // Call the remaining per-slave handlers.  Those elements in
    // m_addedSlave for which the handler callback has already been called
    // have already been set to INVALID_SLAVE_ID.
    for (std::size_t index = 0; index < m_addedSlaves.size(); ++index) {
        const auto id = m_addedSlaves[index];
        if (id != coral::model::INVALID_SLAVE_ID) {
            m_onSlaveComplete(
                make_error_code(coral::error::generic_error::operation_failed),
                coral::model::INVALID_SLAVE_ID,
                index);
        }
    }
    m_onComplete(make_error_code(coral::error::generic_error::operation_failed));
    self.SwapState(std::make_unique<FatalErrorExecutionState>());
}


// =============================================================================


ReconfiguringExecutionState::ReconfiguringExecutionState(
    const std::vector<SlaveConfig>& slaveConfigs,
    std::chrono::milliseconds commTimeout,
    ExecutionManager::ReconstituteHandler onComplete,
    ExecutionManager::SlaveReconstituteHandler onSlaveComplete)
    : m_slaveConfigs(slaveConfigs)
    , m_commTimeout{commTimeout}
    , m_onComplete{std::move(onComplete)}
    , m_onSlaveComplete{std::move(onSlaveComplete)}
{
    assert(!slaveConfigs.empty());
}


void ReconfiguringExecutionState::StateEntered(
    ExecutionManagerPrivate& self)
{
    const auto selfPtr = &self;
    const auto opTally = std::make_shared<OpTally>();
    for (std::size_t index = 0; index < m_slaveConfigs.size(); ++index) {
        const auto slaveID = m_slaveConfigs[index].slaveID;
        self.slaves.at(slaveID).slave->SetVariables(
            m_slaveConfigs[index].variableSettings,
            m_commTimeout,
            [selfPtr, opTally, index, slaveID, this] (const std::error_code& ec)
            {
                --(opTally->ongoing);
                if (ec) {
                    ++(opTally->failed);
                }
                m_onSlaveComplete(ec, slaveID, index);
                if (opTally->ongoing == 0) {
                    // All per-slave calls complete
                    if (opTally->failed == 0) {
                        // No errors
                        m_onComplete(std::error_code{});
                        selfPtr->SwapState(std::make_unique<ReadyExecutionState>());
                    } else {
                        m_onComplete(make_error_code(
                            coral::error::generic_error::operation_failed));
                        selfPtr->SwapState(std::make_unique<FatalErrorExecutionState>());
                    }
                }
            });
        ++(opTally->ongoing);
    }
}


// =============================================================================


namespace
{
    struct ResendVarsOpTally
    {
        int ongoing = 0;
        int timeouts = 0;
        int otherFailures = 0;
    };
}


PrimingExecutionState::PrimingExecutionState(
    int maxAttempts,
    std::chrono::milliseconds commTimeout,
    std::function<void(const std::error_code&)> onComplete)
    : m_maxAttempts{maxAttempts}
    , m_commTimeout{commTimeout}
    , m_onComplete{onComplete}
{
}


void PrimingExecutionState::StateEntered(ExecutionManagerPrivate& self)
{
    Try(self, m_maxAttempts);
}


void PrimingExecutionState::Try(ExecutionManagerPrivate& self, int attemptsLeft)
{
    const auto selfPtr = &self;
    auto opTally = std::make_shared<ResendVarsOpTally>();
    for (const auto& slave : self.slaves) {
        slave.second.slave->ResendVars(
            m_commTimeout,
            [attemptsLeft, selfPtr, opTally, this]
                (const std::error_code& ec)
            {
                --(opTally->ongoing);
                if (ec == coral::error::sim_error::data_timeout) {
                    ++(opTally->timeouts);
                } else if (ec) {
                    ++(opTally->otherFailures);
                }

                if (opTally->ongoing == 0) {
                    if (opTally->otherFailures > 0) {
                        CORAL_LOG_TRACE("RESEND_VARS failed");
                        Fail(*selfPtr, make_error_code(coral::error::generic_error::operation_failed));
                    } else if (opTally->timeouts > 0 && attemptsLeft == 1) {
                        CORAL_LOG_TRACE("RESEND_VARS operation timed out, no attempts left");
                        Fail(*selfPtr, make_error_code(coral::error::sim_error::data_timeout));
                    } else if (opTally->timeouts > 0) {
                        CORAL_LOG_TRACE("RESEND_VARS operation timed out, retrying");
                        Try(*selfPtr, attemptsLeft - 1);
                    } else {
                        CORAL_LOG_TRACE("All RESEND_VARS operations succeeded");
                        Succeed(*selfPtr);
                    }
                }
            });
        ++(opTally->ongoing);
    }
}


void PrimingExecutionState::Fail(
    ExecutionManagerPrivate& self,
    const std::error_code& ec)
{
    auto keepMeAlive =
        self.SwapState(std::make_unique<FatalErrorExecutionState>());
    m_onComplete(ec);
}


void PrimingExecutionState::Succeed(ExecutionManagerPrivate& self)
{
    auto keepMeAlive = self.SwapState(std::make_unique<ReadyExecutionState>());
    m_onComplete(std::error_code{});
}


// =============================================================================


SteppingExecutionState::SteppingExecutionState(
    coral::model::TimeDuration stepSize,
    std::chrono::milliseconds timeout,
    ExecutionManager::StepHandler onComplete,
    ExecutionManager::SlaveStepHandler onSlaveStepComplete)
    : m_stepSize(stepSize),
      m_timeout(timeout),
      m_onComplete(std::move(onComplete)),
      m_onSlaveStepComplete(std::move(onSlaveStepComplete))
{
}


void SteppingExecutionState::StateEntered(ExecutionManagerPrivate& self)
{
    const auto stepID = self.NextStepID();
    const auto selfPtr = &self; // Because we can't capture references in lambdas
    for (auto it = begin(self.slaves); it != end(self.slaves); ++it) {
        const auto slaveID = it->first;
        it->second.slave->Step(
            stepID,
            self.CurrentSimTime(),
            m_stepSize,
            m_timeout,
            [selfPtr, slaveID, this] (const std::error_code& ec) {
                const auto onExit = coral::util::OnScopeExit([=]() {
                    selfPtr->SlaveOpComplete();
                });
                if (m_onSlaveStepComplete) m_onSlaveStepComplete(ec, slaveID);
            });
        self.SlaveOpStarted();
    }
    self.WhenAllSlaveOpsComplete([selfPtr, this] (const std::error_code& ec) {
        assert(!ec);
        bool stepFailed = false;
        bool fatalError = false;
        for (auto it = begin(selfPtr->slaves); it != end(selfPtr->slaves); ++it) {
            if (it->second.slave->State() == SLAVE_STEP_OK) {
                // do nothing
            } else if (it->second.slave->State() == SLAVE_STEP_FAILED) {
                stepFailed = true;
            } else {
                assert(it->second.slave->State() == SLAVE_NOT_CONNECTED);
                fatalError = true;
                break; // because there's no point in continuing
            }
        }
        if (fatalError) {
            const auto keepMeAlive = selfPtr->SwapState(
                std::make_unique<FatalErrorExecutionState>());
            assert(keepMeAlive.get() == this);
            m_onComplete(make_error_code(coral::error::generic_error::operation_failed));
        } else if (stepFailed) {
            const auto keepMeAlive = selfPtr->SwapState(
                std::make_unique<StepFailedExecutionState>());
            assert(keepMeAlive.get() == this);
            m_onComplete(coral::error::sim_error::cannot_perform_timestep);
        } else {
            const auto keepMeAlive = selfPtr->SwapState(
                std::make_unique<StepOkExecutionState>(m_stepSize));
            assert(keepMeAlive.get() == this);
            m_onComplete(std::error_code());
        }
    });
}


// =============================================================================


StepOkExecutionState::StepOkExecutionState(coral::model::TimeDuration stepSize)
    : m_stepSize(stepSize)
{
}


void StepOkExecutionState::Terminate(ExecutionManagerPrivate& self)
{
    self.DoTerminate();
}


void StepOkExecutionState::AcceptStep(
    ExecutionManagerPrivate& self,
    std::chrono::milliseconds timeout,
    ExecutionManager::AcceptStepHandler onComplete,
    ExecutionManager::SlaveAcceptStepHandler onSlaveAcceptStepComplete)
{
    self.AdvanceSimTime(m_stepSize);
    self.SwapState(std::make_unique<AcceptingExecutionState>(
        timeout, std::move(onComplete), std::move(onSlaveAcceptStepComplete)));
}


// =============================================================================


AcceptingExecutionState::AcceptingExecutionState(
    std::chrono::milliseconds timeout,
    ExecutionManager::AcceptStepHandler onComplete,
    ExecutionManager::SlaveAcceptStepHandler onSlaveAcceptStepComplete)
    : m_timeout(timeout),
      m_onComplete(std::move(onComplete)),
      m_onSlaveAcceptStepComplete(std::move(onSlaveAcceptStepComplete))
{
}


void AcceptingExecutionState::StateEntered(ExecutionManagerPrivate& self)
{
    const auto selfPtr = &self; // Because we can't capture references in lambdas
    for (auto it = begin(self.slaves); it != end(self.slaves); ++it) {
        const auto slaveID = it->first;
        it->second.slave->AcceptStep(
            m_timeout,
            [selfPtr, slaveID, this] (const std::error_code& ec) {
                const auto onExit = coral::util::OnScopeExit([=]() {
                    selfPtr->SlaveOpComplete();
                });
                if (m_onSlaveAcceptStepComplete) {
                    m_onSlaveAcceptStepComplete(ec, slaveID);
                }
            });
        self.SlaveOpStarted();
    }
    self.WhenAllSlaveOpsComplete([selfPtr, this] (const std::error_code& ec) {
        assert(!ec);
        bool error = false;
        for (auto it = begin(selfPtr->slaves); it != end(selfPtr->slaves); ++it) {
            if (it->second.slave->State() != SLAVE_READY) {
                assert(it->second.slave->State() == SLAVE_NOT_CONNECTED);
                error = true;
                break;
            }
        }
        if (error) {
            const auto keepMeAlive = selfPtr->SwapState(
                std::make_unique<FatalErrorExecutionState>());
            assert(keepMeAlive.get() == this);
            m_onComplete(make_error_code(coral::error::generic_error::operation_failed));
            return;
        } else {
            const auto keepMeAlive = selfPtr->SwapState(
                std::make_unique<ReadyExecutionState>());
            assert(keepMeAlive.get() == this);
            m_onComplete(std::error_code());
        }
    });
}


// =============================================================================


void StepFailedExecutionState::Terminate(ExecutionManagerPrivate& self)
{
    self.DoTerminate();
}


// =============================================================================


void FatalErrorExecutionState::Terminate(ExecutionManagerPrivate& self)
{
    self.DoTerminate();
}


// =============================================================================


void TerminatedExecutionState::Terminate(ExecutionManagerPrivate& self)
{
    // Do nothing, we're already here.
}


}} // namespace
