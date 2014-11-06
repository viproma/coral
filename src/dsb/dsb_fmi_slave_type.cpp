#include "dsb/fmi/slave_type.hpp"

#include <cassert>

#include "boost/lexical_cast.hpp"

#include "fmilibcpp/Fmu.hpp"
#include "fmilibcpp/ImportContext.hpp"


namespace dsb
{
namespace fmi
{


FmiSlaveType::FmiSlaveType(
    const std::string& fmuPath,
    const std::string& slaveExePath)
    : m_slaveExePath(slaveExePath), m_varList(nullptr)
{
    auto ctx = fmilib::MakeImportContext();
    auto fmu = ctx->Import(fmuPath, m_unzipDir.Path().string());
    if (fmu->FmiVersion() != fmilib::kFmiVersion1_0) {
        throw std::runtime_error("Only FMI version 1.0 supported");
    }
    m_fmu = std::static_pointer_cast<fmilib::fmi1::Fmu>(fmu);
    m_varList = fmi1_import_get_variable_list(m_fmu->Handle());
}

FmiSlaveType::~FmiSlaveType()
{
    assert (m_varList);
    fmi1_import_free_variable_list(m_varList);
}

std::string FmiSlaveType::Name() const 
{
    return m_fmu->ModelName();
}

std::string FmiSlaveType::Uuid() const 
{
    return m_fmu->GUID();
}

std::string FmiSlaveType::Description() const 
{
    return m_fmu->Description();
}

std::string FmiSlaveType::Author() const 
{
    return m_fmu->Author();
}

std::string FmiSlaveType::Version() const 
{
    return m_fmu->ModelVersion();
}

size_t FmiSlaveType::VariableCount() const 
{
    return fmi1_import_get_variable_list_size(m_varList);
}

dsb::model::Variable FmiSlaveType::Variable(size_t index) const 
{
    auto fmiVar = fmi1_import_get_variable(m_varList, index);

    dsb::model::DataType dataType;
    switch (fmi1_import_get_variable_base_type(fmiVar)) {
        case fmi1_base_type_real:
            dataType = dsb::model::REAL_DATATYPE;
            break;
        case fmi1_base_type_int:
            dataType = dsb::model::INTEGER_DATATYPE;
            break;
        case fmi1_base_type_bool:
            dataType = dsb::model::BOOLEAN_DATATYPE;
            break;
        case fmi1_base_type_str:
            dataType = dsb::model::STRING_DATATYPE;
            break;
        case fmi1_base_type_enum:
            assert (!"Enum types not supported yet");
            break;
        default:
            assert (!"Unknown type");
    }

    dsb::model::Variability variability;
    switch (fmi1_import_get_variability(fmiVar)) {
        case fmi1_variability_enu_constant:
            variability = dsb::model::CONSTANT_VARIABILITY;
            break;
        case fmi1_variability_enu_parameter:
            variability = dsb::model::FIXED_VARIABILITY;
            break;
        case fmi1_variability_enu_discrete:
            variability = dsb::model::DISCRETE_VARIABILITY;
            break;
        case fmi1_variability_enu_continuous:
            variability = dsb::model::CONTINUOUS_VARIABILITY;
            break;
        default:
            assert (!"Variable with variability 'unknown' encountered");
    }

    dsb::model::Causality causality;
    switch (fmi1_import_get_causality(fmiVar)) {
        case fmi1_causality_enu_input:
            causality = (variability == dsb::model::FIXED_VARIABILITY)
                        ? dsb::model::PARAMETER_CAUSALITY
                        : dsb::model::INPUT_CAUSALITY;
            break;
        case fmi1_causality_enu_output:
            causality = dsb::model::OUTPUT_CAUSALITY;
            break;
        case fmi1_causality_enu_internal:
            causality = dsb::model::LOCAL_CAUSALITY;
            break;
        default:
            assert (!"Variable with causality 'none' encountered");
    }

    return dsb::model::Variable(
        index,
        fmi1_import_get_variable_name(fmiVar),
        dataType,
        causality,
        variability);
}

bool FmiSlaveType::InstantiateAndConnect(dsb::model::SlaveID slaveID  /* TODO: Execution locator */ )
{
    std::vector<std::string> args;
    args.push_back(boost::lexical_cast<std::string>(slaveID));
//    dsb::util::SpawnProcess(m_slaveExePath, args);
    return false;
}

std::string FmiSlaveType::InstantiationFailureDescription() const 
{
    assert (!"InstantiationFailureDescription() not implemented yet");
    return std::string();
}


}} // namespace
