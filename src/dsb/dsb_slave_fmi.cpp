#include "dsb/slave/fmi.hpp"

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <limits>

#include "fmilibcpp/ImportContext.hpp"


namespace dsb
{
namespace slave
{

TempDir::TempDir()
    : m_path(boost::filesystem::temp_directory_path()
             / boost::filesystem::unique_path())
{
    boost::filesystem::create_directory(m_path);
}

TempDir::~TempDir()
{
    boost::system::error_code ec;
    boost::filesystem::remove_all(m_path, ec);
}

const boost::filesystem::path& TempDir::Path() const
{
    return m_path;
}


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


FmiSlaveInstance::FmiSlaveInstance(const std::string& fmuPath)
    : m_initializing(true)
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
    auto fmiVars = fmi1_import_get_variable_list(m_fmu->Handle());
    assert (fmiVars);

    std::vector<dsb::bus::VariableInfo> vars;
    const auto nVars = fmi1_import_get_variable_list_size(fmiVars);
    for (size_t i = 0; i < nVars; ++i) {
        auto var = fmi1_import_get_variable(fmiVars, i);

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

        vars.push_back(dsb::bus::VariableInfo(
            fmi1_import_get_variable_vr(var),
            fmi1_import_get_variable_name(var),
            causality,
            variability));
    }
    return vars;
}


double FmiSlaveInstance::GetVariable(unsigned varRef)
{
    double retVal;
    FMI1_CALL(fmi1_import_get_real(m_fmu->Handle(), &varRef, 1, &retVal));
    return retVal;
}


void FmiSlaveInstance::SetVariable(unsigned varRef, double value)
{
    FMI1_CALL(fmi1_import_set_real(m_fmu->Handle(), &varRef, 1, &value));
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
    return fmi1_import_do_step(m_fmu->Handle(), currentT, deltaT, true) == fmi1_status_ok;
}


}} // namespace
