#include "dsb/fmi/slave_type.hpp"

#include <cassert>
#include <stdexcept>

#include "boost/lexical_cast.hpp"

#include "fmilibcpp/Fmu.hpp"
#include "fmilibcpp/ImportContext.hpp"
#include "dsb/fmi/glue.hpp"


namespace dsb
{
namespace fmi
{


FmiSlaveType::FmiSlaveType(
    const std::string& fmuPath,
    SlaveStarter slaveStarterFunction)
    : m_fmuPath(fmuPath), m_slaveStarterFunction(slaveStarterFunction), m_varList(nullptr)
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
    return ToVariable(fmi1_import_get_variable(m_varList, index), index);
}

bool FmiSlaveType::InstantiateAndConnect(
    dsb::model::SlaveID slaveID,
    const dsb::execution::Locator& executionLocator)
{
    m_instantiationFailureDescription.clear();
    try {
        m_slaveStarterFunction(slaveID, executionLocator, m_fmuPath);
    } catch (const std::runtime_error& e) {
        m_instantiationFailureDescription = e.what();
        return false;
    }
    return true;
}

std::string FmiSlaveType::InstantiationFailureDescription() const 
{
    return m_instantiationFailureDescription;
}


}} // namespace
