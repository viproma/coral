#include "dsb/fmi/slave_type.hpp"

#include <cassert>
#include <stdexcept>

#include "boost/lexical_cast.hpp"
#include "boost/numeric/conversion/cast.hpp"

#include "dsb/fmilib/fmu.hpp"
#include "dsb/fmilib/importcontext.hpp"
#include "dsb/fmi/glue.hpp"


namespace dsb
{
namespace fmi
{


FmiSlaveType::FmiSlaveType(
    const std::string& fmuPath,
    SlaveStarter slaveStarterFunction)
    : m_fmuPath(fmuPath),
      m_slaveStarterFunction(slaveStarterFunction),
      m_unzipDir("dsb-fmus")
{
    auto ctx = dsb::fmilib::MakeImportContext(nullptr, jm_log_level_error);
    auto fmu = ctx->Import(fmuPath, m_unzipDir.Path().string());
    if (fmu->FmiVersion() != dsb::fmilib::kFmiVersion1_0) {
        throw std::runtime_error("Only FMI version 1.0 supported");
    }
    m_fmu = std::static_pointer_cast<dsb::fmilib::Fmu1>(fmu);

    // Create the slave type description object
    const auto varList = fmi1_import_get_variable_list(m_fmu->Handle());
    const auto freeVarList = dsb::util::OnScopeExit([&]() {
        fmi1_import_free_variable_list(varList);
    });
    std::vector<dsb::model::VariableDescription> varVector;
    const auto varCount = fmi1_import_get_variable_list_size(varList);
    for (std::size_t i = 0; i < varCount; ++i) {
        varVector.push_back(ToVariable(
            fmi1_import_get_variable(varList, boost::numeric_cast<unsigned int>(i)),
            boost::numeric_cast<dsb::model::VariableID>(i)));
    }
    m_description = std::make_unique<dsb::model::SlaveTypeDescription>(
        m_fmu->ModelName(),
        m_fmu->GUID(),
        m_fmu->Description(),
        m_fmu->Author(),
        m_fmu->ModelVersion(),
        varVector);
}

const dsb::model::SlaveTypeDescription& FmiSlaveType::Description() const
{
    return *m_description;
}

bool FmiSlaveType::Instantiate(
    std::chrono::milliseconds timeout,
    dsb::net::SlaveLocator& slaveLocator)
{
    m_instantiationFailureDescription.clear();
    try {
        slaveLocator = m_slaveStarterFunction(m_fmuPath, timeout);
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
