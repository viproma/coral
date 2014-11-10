#include "dsb/slave/fmi.hpp"

#include <cassert>
#include <cstdlib>
#include <limits>

#include "boost/foreach.hpp"
#include "fmilibcpp/ImportContext.hpp"


namespace dsb
{
namespace slave
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
                                   std::ostream* outputStream)
    : m_initializing(true),
      m_outputStream(outputStream)
{
    auto ctx = fmilib::MakeImportContext(nullptr, jm_log_level_error);
    auto fmu = ctx->Import(fmuPath, m_fmuDir.Path().string());
    if (fmu->FmiVersion() != fmilib::kFmiVersion1_0) {
        throw std::runtime_error("Only FMI version 1.0 supported");
    }
    m_fmu = std::static_pointer_cast<fmilib::fmi1::Fmu>(fmu);

    JM_CALL(fmi1_import_instantiate_slave(
        m_fmu->Handle(),
        "unnamed_slave",
        nullptr,
        nullptr,
        0,
        false,
        false));

    auto fmiVars = fmi1_import_get_variable_list(m_fmu->Handle());
    assert (fmiVars);

    if (m_outputStream) *m_outputStream << "Time";
    std::clog << "Variables:" << std::endl;
    const auto nVars = fmi1_import_get_variable_list_size(fmiVars);
    for (size_t i = 0; i < nVars; ++i) {
        auto var = fmi1_import_get_variable(fmiVars, i);

        dsb::bus::DataType dataType;
        switch (fmi1_import_get_variable_base_type(var)) {
            case fmi1_base_type_real:
                dataType = dsb::bus::REAL_DATATYPE;
                break;
            case fmi1_base_type_int:
                dataType = dsb::bus::INTEGER_DATATYPE;
                break;
            case fmi1_base_type_bool:
                dataType = dsb::bus::BOOLEAN_DATATYPE;
                break;
            case fmi1_base_type_str:
                dataType = dsb::bus::STRING_DATATYPE;
                break;
            case fmi1_base_type_enum:
                assert (!"Enum types not supported yet");
                break;
            default:
                assert (!"Unknown type");
        }

        dsb::bus::Variability variability;
        switch (fmi1_import_get_variability(var)) {
            case fmi1_variability_enu_constant:
                variability = dsb::bus::CONSTANT_VARIABILITY;
                break;
            case fmi1_variability_enu_parameter:
                variability = dsb::bus::FIXED_VARIABILITY;
                break;
            case fmi1_variability_enu_discrete:
                variability = dsb::bus::DISCRETE_VARIABILITY;
                break;
            case fmi1_variability_enu_continuous:
                variability = dsb::bus::CONTINUOUS_VARIABILITY;
                break;
            default:
                assert (!"Variable with variability 'unknown' encountered");
                continue;
        }

        dsb::bus::Causality causality;
        switch (fmi1_import_get_causality(var)) {
            case fmi1_causality_enu_input:
                causality = (variability == dsb::bus::FIXED_VARIABILITY)
                            ? dsb::bus::PARAMETER_CAUSALITY
                            : dsb::bus::INPUT_CAUSALITY;
                break;
            case fmi1_causality_enu_output:
                causality = dsb::bus::OUTPUT_CAUSALITY;
                break;
            case fmi1_causality_enu_internal:
                causality = dsb::bus::LOCAL_CAUSALITY;
                break;
            default:
                assert (!"Variable with causality 'none' encountered");
                continue;
        }

        m_fmiValueRefs.push_back(fmi1_import_get_variable_vr(var));
        m_variables.push_back(dsb::bus::VariableInfo(
            i,
            fmi1_import_get_variable_name(var),
            dataType,
            causality,
            variability));

        // TODO: Temporary, remove later
        std::clog << "  " << i << ": " << fmi1_import_get_variable_name(var) << std::endl;
        if (m_outputStream) *m_outputStream << "," << fmi1_import_get_variable_name(var);
    }
    if (m_outputStream) *m_outputStream << std::endl;
}


FmiSlaveInstance::~FmiSlaveInstance()
{
    fmi1_import_free_slave_instance(m_fmu->Handle());
}


void FmiSlaveInstance::Setup(double startTime, double stopTime)
{
    m_startTime = startTime;
    m_stopTime = stopTime;
}


std::vector<dsb::bus::VariableInfo> FmiSlaveInstance::Variables()
{
    return m_variables;
}


double FmiSlaveInstance::GetRealVariable(unsigned varRef)
{
    double retVal;
    FMI1_CALL(fmi1_import_get_real(m_fmu->Handle(), &m_fmiValueRefs[varRef], 1, &retVal));
    return retVal;
}


int FmiSlaveInstance::GetIntegerVariable(unsigned varRef)
{
    int retVal;
    FMI1_CALL(fmi1_import_get_integer(m_fmu->Handle(), &m_fmiValueRefs[varRef], 1, &retVal));
    return retVal;
}


bool FmiSlaveInstance::GetBooleanVariable(unsigned varRef)
{
    fmi1_boolean_t retVal;
    FMI1_CALL(fmi1_import_get_boolean(m_fmu->Handle(), &m_fmiValueRefs[varRef], 1, &retVal));
    return retVal != 0;
}


std::string FmiSlaveInstance::GetStringVariable(unsigned varRef)
{
    fmi1_string_t retVal;
    FMI1_CALL(fmi1_import_get_string(m_fmu->Handle(), &m_fmiValueRefs[varRef], 1, &retVal));
    return std::string(retVal ? retVal : "");
}


void FmiSlaveInstance::SetRealVariable(unsigned varRef, double value)
{
    FMI1_CALL(fmi1_import_set_real(m_fmu->Handle(), &m_fmiValueRefs[varRef], 1, &value));
}


void FmiSlaveInstance::SetIntegerVariable(unsigned varRef, int value)
{
    FMI1_CALL(fmi1_import_set_integer(m_fmu->Handle(), &m_fmiValueRefs[varRef], 1, &value));
}


void FmiSlaveInstance::SetBooleanVariable(unsigned varRef, bool value)
{
    fmi1_boolean_t fmiBool = value;
    FMI1_CALL(fmi1_import_set_boolean(m_fmu->Handle(), &m_fmiValueRefs[varRef], 1, &fmiBool));
}


void FmiSlaveInstance::SetStringVariable(unsigned varRef, const std::string& value)
{
    const auto cValue = value.c_str();
    FMI1_CALL(fmi1_import_set_string(m_fmu->Handle(), &m_fmiValueRefs[varRef], 1, &cValue));
}


//TODO: This is only temporary!
namespace
{
    void PrintVariable(
        std::ostream& out,
        const dsb::bus::VariableInfo& varInfo,
        dsb::bus::ISlaveInstance& slaveInstance)
    {
        out << ","; // << varInfo.name << " ";
        switch (varInfo.dataType) {
            case dsb::bus::REAL_DATATYPE:
                out << slaveInstance.GetRealVariable(varInfo.reference);
                break;
            case dsb::bus::INTEGER_DATATYPE:
                out << slaveInstance.GetIntegerVariable(varInfo.reference);
                break;
            case dsb::bus::BOOLEAN_DATATYPE:
                out << slaveInstance.GetBooleanVariable(varInfo.reference);
                break;
            case dsb::bus::STRING_DATATYPE:
                out << slaveInstance.GetStringVariable(varInfo.reference);
                break;
            default:
                assert (false);
        }
    }
}


bool FmiSlaveInstance::DoStep(double currentT, double deltaT)
{
    if (m_initializing) {
        FMI1_CALL(fmi1_import_initialize_slave(
            m_fmu->Handle(),
            m_startTime,
            m_stopTime != std::numeric_limits<double>::infinity(),
            m_stopTime));
        m_initializing = false;
    }
    const auto rc = fmi1_import_do_step(m_fmu->Handle(), currentT, deltaT, true);

    // TODO: Temporary, to be removed when we have proper observers
    if (m_outputStream) {
        *m_outputStream << (currentT + deltaT);
        BOOST_FOREACH (const auto& var, m_variables) {
            PrintVariable(*m_outputStream, var, *this);
        }
        *m_outputStream << std::endl;
    }

    return rc == fmi1_status_ok;
}


}} // namespace
