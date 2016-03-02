#include "dsb/fmi/slave.hpp"

#include <cassert>
#include <fstream>
#include <limits>
#include <stdexcept>

#include "boost/numeric/conversion/cast.hpp"
#include "fmilib.h"

#include "dsb/fmilib/importcontext.hpp"
#include "dsb/fmi/glue.hpp"
#include "dsb/util.hpp"


namespace dsb
{
namespace fmi
{


#define JM_CALL(expression) \
    do { \
        if ((expression) != jm_status_success) \
            throw std::runtime_error("FMI Library error while performing the following function call: " #expression); \
    } while (false);
#define FMI1_CALL(expression) \
    do { \
        if ((expression) != fmi1_status_ok) \
            throw std::runtime_error("FMI1 error while performing the following function call: " #expression); \
    } while (false);


FmiSlaveInstance::FmiSlaveInstance(const std::string& fmuPath,
                                   const std::string* outputFilePrefix)
    : m_initialized(false)
    , m_outputFilePrefix(outputFilePrefix
        ? (outputFilePrefix->empty() ? "./" : *outputFilePrefix)
        : std::string{})
{
    auto ctx = dsb::fmilib::MakeImportContext(nullptr, jm_log_level_error);
    auto fmu = ctx->Import(fmuPath, m_fmuDir.Path().string());
    if (fmu->FmiVersion() != dsb::fmilib::kFmiVersion1_0) {
        throw std::runtime_error("Only FMI version 1.0 supported");
    }
    m_fmu = std::static_pointer_cast<dsb::fmilib::Fmu1>(fmu);

    JM_CALL(fmi1_import_instantiate_slave(
        m_fmu->Handle(),
        "unnamed_slave",
        nullptr,
        nullptr,
        0,
        false,
        false));

    const auto fmiVars = fmi1_import_get_variable_list(m_fmu->Handle());
    assert (fmiVars);
    const auto freeFmiVars = dsb::util::OnScopeExit([&]() {
        fmi1_import_free_variable_list(fmiVars);
    });

    std::vector<dsb::model::VariableDescription> variables;
    const auto nVars = fmi1_import_get_variable_list_size(fmiVars);
    for (unsigned int i = 0; i < nVars; ++i) {
        const auto var = fmi1_import_get_variable(fmiVars, i);
        m_fmiValueRefs.push_back(fmi1_import_get_variable_vr(var));
        variables.push_back(
            ToVariable(var, boost::numeric_cast<dsb::model::VariableID>(i)));
    }
    m_typeDescription = std::make_unique<dsb::model::SlaveTypeDescription>(
        m_fmu->ModelName(),
        m_fmu->GUID(),
        m_fmu->Description(),
        m_fmu->Author(),
        m_fmu->ModelVersion(),
        variables);
}


FmiSlaveInstance::~FmiSlaveInstance()
{
    if (m_initialized) {
        fmi1_import_terminate_slave(m_fmu->Handle());
    }
    fmi1_import_free_slave_instance(m_fmu->Handle());
}


bool FmiSlaveInstance::Setup(
    dsb::model::TimePoint startTime,
    dsb::model::TimePoint stopTime,
    const std::string& executionName,
    const std::string& slaveName)
{
    m_startTime = startTime;
    m_stopTime = stopTime;

    // TODO: Temporary, to be removed when we have proper observers
    if (!m_outputFilePrefix.empty()) {
        auto outputFileName = m_outputFilePrefix;
        if (executionName.empty()) {
            outputFileName += dsb::util::Timestamp();
        } else {
            outputFileName += executionName;
        }
        outputFileName += '_';
        if (slaveName.empty()) {
            outputFileName += m_typeDescription->Name() + '_'
                + dsb::util::RandomString(6, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
        } else {
            outputFileName += slaveName;
        }
        outputFileName += ".csv";
        m_outputStream = std::make_unique<std::ofstream>(
            outputFileName,
            std::ios_base::out | std::ios_base::trunc
#ifdef _MSC_VER
            , _SH_DENYWR // Don't let other processes/threads write to the file
#endif
            );
        if (m_outputStream->fail()) {
            throw std::runtime_error(
                "Error opening output file for writing: " + outputFileName);
        }
        *m_outputStream << "Time";
        for (const auto& var : m_typeDescription->Variables()) {
            *m_outputStream << "," << var.Name();
        }
        *m_outputStream << std::endl;
    }
    return true;
}


const dsb::model::SlaveTypeDescription& FmiSlaveInstance::TypeDescription() const
{
    return *m_typeDescription;
}


double FmiSlaveInstance::GetRealVariable(dsb::model::VariableID varRef) const
{
    double retVal;
    FMI1_CALL(fmi1_import_get_real(m_fmu->Handle(), &m_fmiValueRefs[varRef], 1, &retVal));
    return retVal;
}


int FmiSlaveInstance::GetIntegerVariable(dsb::model::VariableID varRef) const
{
    int retVal;
    FMI1_CALL(fmi1_import_get_integer(m_fmu->Handle(), &m_fmiValueRefs[varRef], 1, &retVal));
    return retVal;
}


bool FmiSlaveInstance::GetBooleanVariable(dsb::model::VariableID varRef) const
{
    fmi1_boolean_t retVal;
    FMI1_CALL(fmi1_import_get_boolean(m_fmu->Handle(), &m_fmiValueRefs[varRef], 1, &retVal));
    return retVal != 0;
}


std::string FmiSlaveInstance::GetStringVariable(dsb::model::VariableID varRef) const
{
    fmi1_string_t retVal;
    FMI1_CALL(fmi1_import_get_string(m_fmu->Handle(), &m_fmiValueRefs[varRef], 1, &retVal));
    return std::string(retVal ? retVal : "");
}


void FmiSlaveInstance::SetRealVariable(dsb::model::VariableID varRef, double value)
{
    FMI1_CALL(fmi1_import_set_real(m_fmu->Handle(), &m_fmiValueRefs[varRef], 1, &value));
}


void FmiSlaveInstance::SetIntegerVariable(dsb::model::VariableID varRef, int value)
{
    FMI1_CALL(fmi1_import_set_integer(m_fmu->Handle(), &m_fmiValueRefs[varRef], 1, &value));
}


void FmiSlaveInstance::SetBooleanVariable(dsb::model::VariableID varRef, bool value)
{
    fmi1_boolean_t fmiBool = value;
    FMI1_CALL(fmi1_import_set_boolean(m_fmu->Handle(), &m_fmiValueRefs[varRef], 1, &fmiBool));
}


void FmiSlaveInstance::SetStringVariable(dsb::model::VariableID varRef, const std::string& value)
{
    const auto cValue = value.c_str();
    FMI1_CALL(fmi1_import_set_string(m_fmu->Handle(), &m_fmiValueRefs[varRef], 1, &cValue));
}


//TODO: This is only temporary!
namespace
{
    void PrintVariable(
        std::ostream& out,
        const dsb::model::VariableDescription& varInfo,
        dsb::execution::ISlaveInstance& slaveInstance)
    {
        out << ",";
        switch (varInfo.DataType()) {
            case dsb::model::REAL_DATATYPE:
                out << slaveInstance.GetRealVariable(varInfo.ID());
                break;
            case dsb::model::INTEGER_DATATYPE:
                out << slaveInstance.GetIntegerVariable(varInfo.ID());
                break;
            case dsb::model::BOOLEAN_DATATYPE:
                out << slaveInstance.GetBooleanVariable(varInfo.ID());
                break;
            case dsb::model::STRING_DATATYPE:
                out << slaveInstance.GetStringVariable(varInfo.ID());
                break;
            default:
                assert (false);
        }
    }
}


bool FmiSlaveInstance::DoStep(
    dsb::model::TimePoint currentT,
    dsb::model::TimeDuration deltaT)
{
    if (!m_initialized) {
        FMI1_CALL(fmi1_import_initialize_slave(
            m_fmu->Handle(),
            m_startTime,
            m_stopTime != std::numeric_limits<double>::infinity(),
            m_stopTime));
        m_initialized = true;
    }
    const auto rc = fmi1_import_do_step(m_fmu->Handle(), currentT, deltaT, true);

    // TODO: Temporary, to be removed when we have proper observers
    if (m_outputStream) {
        *m_outputStream << (currentT + deltaT);
        for (const auto& var : m_typeDescription->Variables()) {
            PrintVariable(*m_outputStream, var, *this);
        }
        *m_outputStream << std::endl;
    }

    return rc == fmi1_status_ok;
}


}} // namespace
