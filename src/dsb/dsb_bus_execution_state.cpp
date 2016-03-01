#define NOMINMAX
#include "dsb/bus/execution_state.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <stdexcept>
#include <string>

#include "dsb/bus/execution_manager_private.hpp"
#include "dsb/bus/slave_control_messenger.hpp"
#include "dsb/bus/slave_controller.hpp"
#include "dsb/util.hpp"


namespace dsb
{
namespace bus
{


// =============================================================================


void ConfigExecutionState::Terminate(ExecutionManagerPrivate& self)
{
    self.DoTerminate();
}


void ConfigExecutionState::BeginConfig(
    ExecutionManagerPrivate& /*self*/,
    ExecutionManager::BeginConfigHandler onComplete)
{
    // Do nothing, we're already here.
    onComplete(std::error_code());
}


void ConfigExecutionState::EndConfig(
    ExecutionManagerPrivate& self,
    ExecutionManager::BeginConfigHandler onComplete)
{
    self.SwapState(
        std::make_unique<PrimingExecutionState>(std::move(onComplete)));
}


void ConfigExecutionState::SetSimulationTime(
    ExecutionManagerPrivate& self,
    dsb::model::TimePoint startTime,
    dsb::model::TimePoint stopTime)
{
    DSB_PRECONDITION_CHECK(self.slaves.empty());
    DSB_INPUT_CHECK(startTime <= stopTime);
    self.slaveSetup.startTime = startTime;
    self.slaveSetup.stopTime = stopTime;
}


namespace
{
    // Checks that s matches the regex [a-zA-Z][0-9a-zA-Z_]*
    bool IsValidSlaveName(const std::string& s)
    {
        if (s.empty()) return false;
        if (!std::isalpha(s.front())) return false;
        for (char c : s) if (!std::isalnum(c) && c != '_') return false;
        return true;
    }
}

// NOTE:
// None of the per-slave operation completion handlers in the CONFIG state
// should capture the 'this' pointer of the ConfigExecutionState object.
// The reason is that if the user calls EndConfig() while operations are still
// pending, the operations will not complete in this state, but in the PRIMING
// state.  Hence, the object will have been deleted and replaced with a
// PrimingExecutionState object.

dsb::model::SlaveID ConfigExecutionState::AddSlave(
    ExecutionManagerPrivate& self,
    const dsb::net::SlaveLocator& slaveLocator,
    const std::string& slaveName,
    dsb::comm::Reactor& reactor,
    std::chrono::milliseconds timeout,
    ExecutionManager::AddSlaveHandler onComplete)
{
    namespace dm = dsb::model;
    DSB_INPUT_CHECK(!!onComplete);
    if (!slaveName.empty() && !IsValidSlaveName(slaveName)) {
        throw std::runtime_error('"' + slaveName + "\" is not a valid slave name");
    }
    if (self.lastSlaveID == std::numeric_limits<dm::SlaveID>::max()) {
        throw std::length_error("Maximum number of slaves reached");
    }
    const auto id = ++self.lastSlaveID;
    const auto& realName = slaveName.empty() ? "_slave" + std::to_string(id) : slaveName;
    for (const auto& s : self.slaves) {
        if (realName == s.second.description.Name()) {
            throw std::runtime_error("Duplicate slave name: " + realName);
        }
    }
    const auto selfPtr = &self;

    // Once we have connected the slave, we want to immediately request some
    // info from it.  We define the handler for that operation here, for
    // readability's sake.
    SlaveController::GetDescriptionHandler gotDescription =
        [selfPtr, onComplete, id]
        (const std::error_code& ec, const dm::SlaveDescription& sd)
    {
        if (!ec) {
            selfPtr->slaves.at(id).description
                .SetTypeDescription(sd.TypeDescription());
            onComplete(ec, id);
        } else {
            selfPtr->slaves.at(id).slave->Close();
            onComplete(ec, dm::INVALID_SLAVE_ID);
        }
        selfPtr->SlaveOpComplete();
    };

    // This is the handler for the connection operation. Note that we pass
    // gotDescription into it.
    SlaveController::ConnectHandler connected =
        [selfPtr, timeout, onComplete, id, gotDescription]
        (const std::error_code& ec)
    {
        if (!ec) {
            // The slave is now connected. Next, we request some info
            // from it.
            selfPtr->slaves.at(id).slave->GetDescription(
                timeout, std::move(gotDescription));
        } else {
            onComplete(ec, dsb::model::INVALID_SLAVE_ID);
            selfPtr->SlaveOpComplete();
        }
    };

    auto slave = std::make_unique<dsb::bus::SlaveController>(
        reactor, slaveLocator, id, slaveName, self.slaveSetup,
        timeout, std::move(connected));
    self.slaves.insert(std::make_pair(
        id,
        ExecutionManagerPrivate::Slave(
            std::move(slave),
            dsb::model::SlaveDescription(id, realName))));
    selfPtr->SlaveOpStarted();
    return id;
}


namespace
{
    std::string DataTypeName(dsb::model::DataType dt)
    {
        switch (dt) {
            case dsb::model::REAL_DATATYPE:    return "real";
            case dsb::model::INTEGER_DATATYPE: return "integer";
            case dsb::model::BOOLEAN_DATATYPE: return "boolean";
            case dsb::model::STRING_DATATYPE:  return "string";
        }
        return std::string();
    }

    std::string CausalityName(dsb::model::Causality c)
    {
        switch (c) {
            case dsb::model::PARAMETER_CAUSALITY:            return "parameter";
            case dsb::model::CALCULATED_PARAMETER_CAUSALITY: return "calculated parameter";
            case dsb::model::INPUT_CAUSALITY:                return "input";
            case dsb::model::OUTPUT_CAUSALITY:               return "output";
            case dsb::model::LOCAL_CAUSALITY:                return "local";
        }
        return std::string();
    }

    void VerifyDataTypeMatch(
        dsb::model::DataType expected,
        dsb::model::DataType actual,
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
        dsb::model::Causality inputCausality,
        dsb::model::Causality outputCausality,
        const std::string& slaveName,
        const std::string& varName)
    {
        std::string reason;
        if ((inputCausality == dsb::model::INPUT_CAUSALITY
                && (outputCausality == dsb::model::OUTPUT_CAUSALITY
                    || outputCausality == dsb::model::CALCULATED_PARAMETER_CAUSALITY))
            || (inputCausality == dsb::model::PARAMETER_CAUSALITY
                && outputCausality == dsb::model::CALCULATED_PARAMETER_CAUSALITY)) {
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
        dsb::model::SlaveID slaveID,
        const dsb::model::VariableSetting& setting)
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
                dsb::model::DataTypeOf(setting.Value()),
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
            const dsb::model::VariableDescription* otherVarDesc = nullptr;
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
        dsb::model::SlaveID slaveID,
        const std::vector<dsb::model::VariableSetting>& settings)
    {
        for (const auto& setting : settings) {
            VerifyVariableSetting(self, slaveID, setting);
        }
    }
}


void ConfigExecutionState::SetVariables(
    ExecutionManagerPrivate& self,
    dsb::model::SlaveID slave,
    const std::vector<dsb::model::VariableSetting>& settings,
    std::chrono::milliseconds timeout,
    ExecutionManager::SetVariablesHandler onComplete)
{
    VerifyVariableSettings(self, slave, settings);
    const auto selfPtr = &self;
    self.slaves.at(slave).slave->SetVariables(
        settings,
        timeout,
        [onComplete, selfPtr](const std::error_code& ec) {
            const auto onExit = dsb::util::OnScopeExit([=]() {
                selfPtr->SlaveOpComplete();
            });
            onComplete(ec);
        });
    self.SlaveOpStarted();
}


// =============================================================================


PrimingExecutionState::PrimingExecutionState(
    ExecutionManager::EndConfigHandler onComplete)
    : m_onComplete(std::move(onComplete))
{
}


void PrimingExecutionState::StateEntered(ExecutionManagerPrivate& self)
{
    const auto selfPtr = &self;
    self.WhenAllSlaveOpsComplete([selfPtr, this] (const std::error_code& ec) {
        assert(!ec);

        // Garbage collection: Remove all slave controllers whose connection
        // has failed or been canceled.
        for (auto it = begin(selfPtr->slaves); it != end(selfPtr->slaves); ) {
            if (it->second.slave->State() == SLAVE_NOT_CONNECTED) {
                it = selfPtr->slaves.erase(it);
            } else {
                ++it;
            }
        }

        const auto keepMeAlive = selfPtr->SwapState(std::make_unique<ReadyExecutionState>());
        assert(keepMeAlive.get() == this);
        dsb::util::LastCall(m_onComplete, std::error_code());
   });
}


// =============================================================================


void ReadyExecutionState::Terminate(ExecutionManagerPrivate& self)
{
    self.DoTerminate();
}


void ReadyExecutionState::BeginConfig(
    ExecutionManagerPrivate& self,
    ExecutionManager::BeginConfigHandler onComplete)
{
    self.SwapState(std::make_unique<ConfigExecutionState>());
    onComplete(std::error_code());
}


void ReadyExecutionState::Step(
    ExecutionManagerPrivate& self,
    dsb::model::TimeDuration stepSize,
    std::chrono::milliseconds timeout,
    ExecutionManager::StepHandler onComplete,
    ExecutionManager::SlaveStepHandler onSlaveStepComplete)
{
    self.SwapState(std::make_unique<SteppingExecutionState>(
        stepSize, timeout, std::move(onComplete), std::move(onSlaveStepComplete)));
}


// =============================================================================


SteppingExecutionState::SteppingExecutionState(
    dsb::model::TimeDuration stepSize,
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
                const auto onExit = dsb::util::OnScopeExit([=]() {
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
            m_onComplete(make_error_code(dsb::error::generic_error::operation_failed));
        } else if (stepFailed) {
            const auto keepMeAlive = selfPtr->SwapState(
                std::make_unique<StepFailedExecutionState>());
            assert(keepMeAlive.get() == this);
            m_onComplete(dsb::error::sim_error::cannot_perform_timestep);
        } else {
            const auto keepMeAlive = selfPtr->SwapState(
                std::make_unique<StepOkExecutionState>(m_stepSize));
            assert(keepMeAlive.get() == this);
            m_onComplete(std::error_code());
        }
    });           
}


// =============================================================================


StepOkExecutionState::StepOkExecutionState(dsb::model::TimeDuration stepSize)
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
                const auto onExit = dsb::util::OnScopeExit([=]() {
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
            m_onComplete(make_error_code(dsb::error::generic_error::operation_failed));
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
