#include "dsb/execution/locator.hpp"


namespace dsb
{
namespace execution
{


Locator::Locator(
    const std::string& masterEndpoint,
    const std::string& slaveEndpoint,
    const std::string& variablePubEndpoint,
    const std::string& variableSubEndpoint,
    const std::string& execTerminationEndpoint,
    const std::string& execID)
    : m_masterEndpoint(masterEndpoint),
      m_slaveEndpoint(slaveEndpoint),
      m_variablePubEndpoint(variablePubEndpoint),
      m_variableSubEndpoint(variableSubEndpoint),
      m_execTerminationEndpoint(execTerminationEndpoint),
      m_execName(execID)
{
}

const std::string& Locator::MasterEndpoint() const
{
    return m_masterEndpoint;
}

const std::string& Locator::SlaveEndpoint() const
{
    return m_slaveEndpoint;
}

const std::string& Locator::VariablePubEndpoint() const
{
    return m_variablePubEndpoint;
}

const std::string& Locator::VariableSubEndpoint() const
{
    return m_variableSubEndpoint;
}

const std::string& Locator::ExecTerminationEndpoint() const
{
    return m_execTerminationEndpoint;
}

const std::string& Locator::ExecName() const
{
    return m_execName;
}


}} // namespace
