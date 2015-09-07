#include "dsb/model.hpp"

#include "dsb/error.hpp"


namespace dsb
{
namespace model
{


VariableDescription::VariableDescription(
    dsb::model::VariableID id,
    const std::string& name,
    dsb::model::DataType dataType,
    dsb::model::Causality causality,
    dsb::model::Variability variability)
    : m_id(id),
      m_name(name),
      m_dataType(dataType),
      m_causality(causality),
      m_variability(variability)
{ }


dsb::model::VariableID VariableDescription::ID() const
{
    return m_id;
}


const std::string& VariableDescription::Name() const
{
    return m_name;
}


dsb::model::DataType VariableDescription::DataType() const
{
    return m_dataType;
}


dsb::model::Causality VariableDescription::Causality() const
{
    return m_causality;
}


dsb::model::Variability VariableDescription::Variability() const
{
    return m_variability;
}


// =============================================================================
// VariableSetting
// =============================================================================


VariableSetting::VariableSetting(
    VariableID variable,
    const ScalarValue& value)
    : m_variable(variable),
      m_hasValue(true),
      m_value(value),
      m_connectedOutput()
{
}


VariableSetting::VariableSetting(
    VariableID inputVar,
    const dsb::model::Variable& outputVar)
    : m_variable(inputVar),
      m_hasValue(false),
      m_value(),
      m_connectedOutput(outputVar)
{
    DSB_INPUT_CHECK(!outputVar.Empty());
}


VariableSetting::VariableSetting(
    VariableID inputVar,
    const ScalarValue& value,
    const dsb::model::Variable& outputVar)
    : m_variable(inputVar),
      m_hasValue(true),
      m_value(value),
      m_connectedOutput(outputVar)
{
    DSB_INPUT_CHECK(!outputVar.Empty());
}


VariableID VariableSetting::Variable() const DSB_NOEXCEPT
{
    return m_variable;
}


bool VariableSetting::HasValue() const DSB_NOEXCEPT
{
    return m_hasValue;
}


const ScalarValue& VariableSetting::Value() const
{
    DSB_PRECONDITION_CHECK(HasValue());
    return m_value;
}


bool VariableSetting::IsConnected() const DSB_NOEXCEPT
{
    return m_connectedOutput.Slave() != INVALID_SLAVE_ID;
}


const dsb::model::Variable& VariableSetting::ConnectedOutput() const
{
    DSB_PRECONDITION_CHECK(IsConnected());
    return m_connectedOutput;
}


}} // namespace
