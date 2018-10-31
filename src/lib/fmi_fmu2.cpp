/*
Copyright 2016-present, SINTEF Ocean.
This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#include <coral/fmi/fmu2.hpp>

#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <unordered_map>

#include <boost/numeric/conversion/cast.hpp>
#include <fmilib.h>

#include <coral/fmi/glue.hpp>
#include <coral/fmi/importer.hpp>
#include <coral/log.hpp>
#include <coral/util.hpp>

#ifdef _WIN32
#include <coral/fmi/windows.hpp>
#endif


namespace coral
{
namespace fmi
{

// =============================================================================
// FMU2
// =============================================================================

FMU2::FMU2(
    std::shared_ptr<coral::fmi::Importer> importer,
    const boost::filesystem::path& fmuDir)
    : m_importer{importer}
    , m_dir(fmuDir)
    , m_handle{fmi2_import_parse_xml(importer->FmilibHandle(), fmuDir.string().c_str(), nullptr)}
{
    if (m_handle == nullptr) {
        throw std::runtime_error(importer->LastErrorMessage());
    }
    const auto fmuKind = fmi2_import_get_fmu_kind(m_handle);
    if (!(fmuKind & fmi2_fmu_kind_cs)) {
        throw std::runtime_error("Not a co-simulation FMU");
    }

    // Create the slave type description object
    const auto varList = fmi2_import_get_variable_list(m_handle, 0);
    const auto freeVarList = coral::util::OnScopeExit([&]() {
        fmi2_import_free_variable_list(varList);
    });
    std::vector<coral::model::VariableDescription> variables;
    const auto varCount = fmi2_import_get_variable_list_size(varList);
    for (unsigned int i = 0; i < varCount; ++i) {
        const auto var = fmi2_import_get_variable(varList, i);
        m_valueReferences.push_back(fmi2_import_get_variable_vr(var));
        variables.push_back(
            ToVariable(var, boost::numeric_cast<coral::model::VariableID>(i)));
    }
    m_description = std::make_unique<coral::model::SlaveTypeDescription>(
        std::string(fmi2_import_get_model_name(m_handle)),
        std::string(fmi2_import_get_GUID(m_handle)),
        std::string(fmi2_import_get_description(m_handle)),
        std::string(fmi2_import_get_author(m_handle)),
        std::string(fmi2_import_get_model_version(m_handle)),
        variables);
}


FMU2::~FMU2()
{
    fmi2_import_free(m_handle);
}


coral::fmi::FMIVersion FMU2::FMIVersion() const
{
    return coral::fmi::FMIVersion::v2_0;
}


const coral::model::SlaveTypeDescription& FMU2::Description() const
{
    return *m_description;
}


std::shared_ptr<SlaveInstance> FMU2::InstantiateSlave()
{
    return InstantiateSlave2();
}


std::shared_ptr<coral::fmi::Importer> FMU2::Importer() const
{
    return m_importer;
}


namespace
{
    void Prune(std::vector<std::weak_ptr<SlaveInstance2>>& instances)
    {
        auto newEnd = std::remove_if(
            begin(instances),
            end(instances),
            [] (const std::weak_ptr<SlaveInstance2>& wp) { return wp.expired(); });
        instances.erase(newEnd, end(instances));
    }
}


std::shared_ptr<SlaveInstance2> FMU2::InstantiateSlave2()
{
#ifdef _WIN32
    if (!m_additionalDllSearchPath) {
        m_additionalDllSearchPath =
            std::make_unique<AdditionalPath>(FMUBinariesDir(m_dir));
    }
#endif
    Prune(m_instances);
    const bool isSingleton = !!fmi2_import_get_capability(
        m_handle,
        fmi2_cs_canBeInstantiatedOnlyOncePerProcess);
    if (isSingleton && !m_instances.empty()) {
        throw std::runtime_error("FMU can only be instantiated once");
    }
    auto instance =
        std::shared_ptr<SlaveInstance2>(new SlaveInstance2(shared_from_this()));
    m_instances.push_back(instance);
    return instance;
}


const boost::filesystem::path& FMU2::Directory() const
{
    return m_dir;
}


fmi2_value_reference_t FMU2::FMIValueReference(coral::model::VariableID variable)
    const
{
    return m_valueReferences.at(variable);
}


fmi2_import_t* FMU2::FmilibHandle() const
{
    return m_handle;
}


// =============================================================================
// SlaveInstance2
// =============================================================================

namespace
{
    void StepFinishedPlaceholder(fmi2_component_environment_t, fmi2_status_t)
    {
        CORAL_LOG_DEBUG("FMU instance completed asynchronous step, "
            "but this feature is currently not supported");
    }

    struct LogRecord
    {
        LogRecord() { }
        LogRecord(fmi2_status_t s, const std::string& m) : status{s}, message(m) { }
        fmi2_status_t status = fmi2_status_ok;
        std::string message;
    };
    std::unordered_map<std::string, LogRecord> g_logRecords;
    std::mutex g_logMutex;

    void LogMessage(
        fmi2_component_environment_t,
        fmi2_string_t instanceName,
        fmi2_status_t status,
        fmi2_string_t category,
        fmi2_string_t message,
        ...)
    {
        std::va_list args;
        va_start(args, message);
        const auto msgLength = std::vsnprintf(nullptr, 0, message, args);
        va_end(args);
        auto msgBuffer = std::vector<char>(msgLength+1);
        va_start(args, message);
        std::vsnprintf(msgBuffer.data(), msgBuffer.size(), message, args);
        va_end(args);
        assert(msgBuffer.back() == '\0');

        coral::log::Level logLevel = coral::log::error;
        switch (status) {
            case fmi2_status_ok:
                logLevel = coral::log::info;
                break;
            case fmi2_status_warning:
                logLevel = coral::log::warning;
                break;
            case fmi2_status_discard:
                // Don't know if this ever happens, but we should at least
                // print a debug message if it does.
                logLevel = coral::log::debug;
                break;
            case fmi2_status_error:
                logLevel = coral::log::error;
                break;
            case fmi2_status_fatal:
                logLevel = coral::log::error;
                break;
            case fmi2_status_pending:
                // Don't know if this ever happens, but we should at least
                // print a debug message if it does.
                logLevel = coral::log::debug;
                break;
        }

        if (logLevel < coral::log::error) {
            // Errors are not logged; we handle them with exceptions instead.
            coral::log::Log(logLevel, msgBuffer.data());
        }

        g_logMutex.lock();
        g_logRecords[instanceName] =
            LogRecord{status, std::string(msgBuffer.data())};
        g_logMutex.unlock();
    }

    LogRecord LastLogRecord(const std::string& instanceName)
    {
        std::lock_guard<std::mutex> lock(g_logMutex);
        const auto it = g_logRecords.find(instanceName);
        if (it == g_logRecords.end()) {
            return LogRecord{};
        } else {
            // Note the use of c_str() here, to force the string to be copied.
            // The C++ standard now disallows copy-on-write, but some compilers
            // still use it, which could lead to problems in multithreaded
            // programs.
            return LogRecord{
                it->second.status,
                std::string(it->second.message.c_str())
            };
        }
    }
}


SlaveInstance2::SlaveInstance2(std::shared_ptr<coral::fmi::FMU2> fmu)
    : m_fmu{fmu}
    , m_handle{fmi2_import_parse_xml(fmu->Importer()->FmilibHandle(), fmu->Directory().string().c_str(), nullptr)}
{
    if (m_handle == nullptr) {
        throw std::runtime_error(fmu->Importer()->LastErrorMessage());
    }

    fmi2_callback_functions_t callbacks;
    callbacks.allocateMemory       = std::calloc;
    callbacks.freeMemory           = std::free;
    callbacks.logger               = LogMessage;
    callbacks.stepFinished         = StepFinishedPlaceholder;
    callbacks.componentEnvironment = nullptr;

    if (fmi2_import_create_dllfmu(m_handle, fmi2_fmu_kind_cs, &callbacks) != jm_status_success) {
        const auto msg = fmu->Importer()->LastErrorMessage();
        fmi2_import_free(m_handle);
        throw std::runtime_error(msg);
    }
}


SlaveInstance2::~SlaveInstance2() noexcept
{
    if (m_setupComplete) {
        if (m_simStarted) {
            fmi2_import_terminate(m_handle);
        }
        fmi2_import_free_instance(m_handle);
    }
    fmi2_import_destroy_dllfmu(m_handle);
    fmi2_import_free(m_handle);
}


coral::model::SlaveTypeDescription SlaveInstance2::TypeDescription() const
{
    return FMU()->Description();
}


void SlaveInstance2::Setup(
    const std::string& slaveName,
    const std::string& /*executionName*/,
    coral::model::TimePoint startTime,
    coral::model::TimePoint stopTime,
    bool adaptiveStepSize,
    double relativeTolerance)
{
    assert(!m_setupComplete);
    const auto rci = fmi2_import_instantiate(
        m_handle,
        slaveName.c_str(),
        fmi2_cosimulation,
        nullptr,
        fmi2_false);
    if (rci != jm_status_success) {
        throw std::runtime_error(
            "FMI error: Slave instantiation failed ("
            + LastLogRecord(slaveName).message + ')');
    }

    const auto rcs = fmi2_import_setup_experiment(
        m_handle,
        adaptiveStepSize ? fmi2_true : fmi2_false,
        relativeTolerance,
        startTime,
        stopTime == coral::model::ETERNITY ? fmi2_false : fmi2_true,
        stopTime);
    if (rcs != fmi2_status_ok && rcs != fmi2_status_warning) {
        throw std::runtime_error(
            "FMI error: Slave setup failed ("
            + LastLogRecord(slaveName).message + ')');
    }

    const auto rce = fmi2_import_enter_initialization_mode(m_handle);
    if (rce != fmi2_status_ok && rce != fmi2_status_warning) {
        throw std::runtime_error(
            "FMI error: Slave failed to enter initialization mode ("
            + LastLogRecord(slaveName).message + ')');
    }

    m_setupComplete = true;
    m_instanceName = slaveName;
}


void SlaveInstance2::StartSimulation()
{
    assert(m_setupComplete);
    assert(!m_simStarted);
    const auto rc = fmi2_import_exit_initialization_mode(m_handle);
    if (rc != fmi2_status_ok && rc != fmi2_status_warning) {
        throw std::runtime_error(
            "FMI error: Slave failed to exit initialization mode ("
            + LastLogRecord(m_instanceName).message + ')');
    }
    m_simStarted = true;
}


void SlaveInstance2::EndSimulation()
{
    assert(m_simStarted);
    const auto rc = fmi2_import_terminate(m_handle);
    m_simStarted = false;
    if (rc != fmi2_status_ok && rc != fmi2_status_warning) {
        throw std::runtime_error(
            "FMI error: Failed to terminate slave ("
            + LastLogRecord(m_instanceName).message + ')');
    }
}


bool SlaveInstance2::DoStep(
    coral::model::TimePoint currentT,
    coral::model::TimeDuration deltaT)
{
    assert(m_simStarted);
    const auto rc = fmi2_import_do_step(m_handle, currentT, deltaT, fmi2_true);
    if (rc == fmi2_status_ok || rc == fmi2_status_warning) {
        return true;
    } else if (rc == fmi2_status_discard) {
        return false;
    } else if (rc == fmi2_status_pending) {
        throw std::runtime_error(
            "FMI error: Slave performs time step asynchronously, which is "
            "not supported");
    } else {
        throw std::runtime_error(
            "Failed to perform time step ("
            + LastLogRecord(m_instanceName).message + ')');
    }
}


namespace
{
    std::runtime_error MakeGetOrSetException(
        const std::string& getOrSet,
        coral::model::VariableID varID,
        const FMU2& fmu,
        const std::string& instanceName)
    {
        return std::runtime_error(
            "Failed to " + getOrSet + "value of variable with ID "
            + std::to_string(varID) + " and FMI value reference "
            + std::to_string(fmu.FMIValueReference(varID))
            + " (" + LastLogRecord(instanceName).message + ")");
    }
}


double SlaveInstance2::GetRealVariable(coral::model::VariableID varID) const
{
    const auto valRef = m_fmu->FMIValueReference(varID);
    fmi2_real_t value = 0.0;
    const auto status = fmi2_import_get_real(m_handle, &valRef, 1, &value);
    if (status != fmi2_status_ok && status != fmi2_status_warning) {
        throw MakeGetOrSetException("get", varID, *FMU2(), m_instanceName);
    }
    return value;
}


int SlaveInstance2::GetIntegerVariable(coral::model::VariableID varID) const
{
    const auto valRef = m_fmu->FMIValueReference(varID);
    fmi2_integer_t value = 0;
    const auto status = fmi2_import_get_integer(m_handle, &valRef, 1, &value);
    if (status != fmi2_status_ok && status != fmi2_status_warning) {
        throw MakeGetOrSetException("get", varID, *FMU2(), m_instanceName);
    }
    return value;
}


bool SlaveInstance2::GetBooleanVariable(coral::model::VariableID varID) const
{
    const auto valRef = m_fmu->FMIValueReference(varID);
    fmi2_boolean_t value = 0;
    const auto status = fmi2_import_get_boolean(m_handle, &valRef, 1, &value);
    if (status != fmi2_status_ok && status != fmi2_status_warning) {
        throw MakeGetOrSetException("get", varID, *FMU2(), m_instanceName);
    }
    return value != fmi2_false;
}


std::string SlaveInstance2::GetStringVariable(coral::model::VariableID varID) const
{
    const auto valRef = m_fmu->FMIValueReference(varID);
    fmi2_string_t value = nullptr;
    const auto status = fmi2_import_get_string(m_handle, &valRef, 1, &value);
    if (status != fmi2_status_ok && status != fmi2_status_warning) {
        throw MakeGetOrSetException("get", varID, *FMU2(), m_instanceName);
    }
    return value ? std::string(value) : std::string();
}


bool SlaveInstance2::SetRealVariable(coral::model::VariableID varID, double value)
{
    const auto valRef = m_fmu->FMIValueReference(varID);
    const auto status = fmi2_import_set_real(m_handle, &valRef, 1, &value);
    if (status == fmi2_status_ok || status == fmi2_status_warning) {
        return true;
    } else if (status == fmi2_status_discard) {
        return false;
    } else {
        throw MakeGetOrSetException("set", varID, *FMU2(), m_instanceName);
    }
}


bool SlaveInstance2::SetIntegerVariable(coral::model::VariableID varID, int value)
{
    const auto valRef = m_fmu->FMIValueReference(varID);
    const auto status = fmi2_import_set_integer(m_handle, &valRef, 1, &value);
    if (status == fmi2_status_ok || status == fmi2_status_warning) {
        return true;
    } else if (status == fmi2_status_discard) {
        return false;
    } else {
        throw MakeGetOrSetException("set", varID, *FMU2(), m_instanceName);
    }
}


bool SlaveInstance2::SetBooleanVariable(coral::model::VariableID varID, bool value)
{
    fmi2_boolean_t fmiValue = value;
    const auto valRef = m_fmu->FMIValueReference(varID);
    const auto status = fmi2_import_set_boolean(m_handle, &valRef, 1, &fmiValue);
    if (status == fmi2_status_ok || status == fmi2_status_warning) {
        return true;
    } else if (status == fmi2_status_discard) {
        return false;
    } else {
        throw MakeGetOrSetException("set", varID, *FMU2(), m_instanceName);
    }
}


bool SlaveInstance2::SetStringVariable(coral::model::VariableID varID, const std::string& value)
{
    const auto fmiValue = value.c_str();
    const auto valRef = m_fmu->FMIValueReference(varID);
    const auto status = fmi2_import_set_string(m_handle, &valRef, 1, &fmiValue);
    if (status == fmi2_status_ok || status == fmi2_status_warning) {
        return true;
    } else if (status == fmi2_status_discard) {
        return false;
    } else {
        throw MakeGetOrSetException("set", varID, *FMU2(), m_instanceName);
    }
}


std::shared_ptr<coral::fmi::FMU> SlaveInstance2::FMU() const
{
    return FMU2();
}


std::shared_ptr<coral::fmi::FMU2> SlaveInstance2::FMU2() const
{
    return m_fmu;
}


fmi2_import_t* SlaveInstance2::FmilibHandle() const
{
    return m_handle;
}


}} //namespace
