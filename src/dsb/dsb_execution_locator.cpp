#include "dsb/execution/locator.hpp"


namespace dsb
{
namespace execution
{


Locator::Locator(
    const std::string& masterEndpoint,
    const std::string& slaveEndpoint,
    const std::string& variablePubEndpoint,
    const std::string& variableSubEndpoint)
    : m_masterEndpoint(masterEndpoint),
      m_slaveEndpoint(slaveEndpoint),
      m_variablePubEndpoint(variablePubEndpoint),
      m_variableSubEndpoint(variableSubEndpoint)
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


}} // namespace
