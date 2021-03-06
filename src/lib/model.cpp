/*
Copyright 2013-present, SINTEF Ocean.
This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#include <coral/model.hpp>

#include <cctype>

#include <coral/error.hpp>
#include <coral/util.hpp>


namespace coral
{
namespace model
{

// =============================================================================
// Free functions
// =============================================================================

namespace
{
    class DataTypeOfVisitor : public boost::static_visitor<DataType>
    {
    public:
        DataType operator()(double)      const noexcept { return REAL_DATATYPE; }
        DataType operator()(int)         const noexcept { return INTEGER_DATATYPE; }
        DataType operator()(bool)        const noexcept { return BOOLEAN_DATATYPE; }
        DataType operator()(std::string) const noexcept { return STRING_DATATYPE; }
    };
}

DataType DataTypeOf(const ScalarValue& v)
{
    return boost::apply_visitor(DataTypeOfVisitor{}, v);
}


// =============================================================================
// VariableDescription
// =============================================================================

VariableDescription::VariableDescription(
    coral::model::VariableID id,
    const std::string& name,
    coral::model::DataType dataType,
    coral::model::Causality causality,
    coral::model::Variability variability)
    : m_id(id),
      m_name(name),
      m_dataType(dataType),
      m_causality(causality),
      m_variability(variability)
{ }


coral::model::VariableID VariableDescription::ID() const
{
    return m_id;
}


const std::string& VariableDescription::Name() const
{
    return m_name;
}


coral::model::DataType VariableDescription::DataType() const
{
    return m_dataType;
}


coral::model::Causality VariableDescription::Causality() const
{
    return m_causality;
}


coral::model::Variability VariableDescription::Variability() const
{
    return m_variability;
}


// =============================================================================
// SlaveTypeDescription
// =============================================================================

SlaveTypeDescription::SlaveTypeDescription() noexcept
{
}


const std::string& SlaveTypeDescription::Name() const
{
    return m_name;
}


const std::string& SlaveTypeDescription::UUID() const
{
    return m_uuid;
}


const std::string& SlaveTypeDescription::Description() const
{
    return m_description;
}


const std::string& SlaveTypeDescription::Author() const
{
    return m_author;
}


const std::string& SlaveTypeDescription::Version() const
{
    return m_version;
}


SlaveTypeDescription::ConstVariablesRange SlaveTypeDescription::Variables() const
{
    return m_variables | boost::adaptors::map_values;
}


const VariableDescription& SlaveTypeDescription::Variable(VariableID id) const
{
    return m_variables.at(id);
}


// =============================================================================
// SlaveDescription
// =============================================================================

SlaveDescription::SlaveDescription(
    SlaveID id,
    const std::string& name,
    const SlaveTypeDescription& typeDescription)
    : m_id(id),
      m_name(name),
      m_typeDescription(typeDescription)
{
}


SlaveID SlaveDescription::ID() const
{
    return m_id;
}


void SlaveDescription::SetID(SlaveID value)
{
    m_id = value;
}


const std::string& SlaveDescription::Name() const
{
    return m_name;
}


void SlaveDescription::SetName(const std::string& value)
{
    m_name = value;
}


const SlaveTypeDescription& SlaveDescription::TypeDescription() const
{
    return m_typeDescription;
}


void SlaveDescription::SetTypeDescription(const SlaveTypeDescription& value)
{
    m_typeDescription = value;
}


// =============================================================================
// Variable
// =============================================================================


bool operator==(const Variable& a, const Variable& b)
{
    return a.Slave() == b.Slave() && a.ID() == b.ID();
}


bool operator!=(const Variable& a, const Variable& b)
{
    return !(a == b);
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
      m_isConnectionChange(false),
      m_connectedOutput()
{
}


VariableSetting::VariableSetting(
    VariableID inputVar,
    const coral::model::Variable& outputVar)
    : m_variable(inputVar),
      m_hasValue(false),
      m_value(),
      m_isConnectionChange(true),
      m_connectedOutput(outputVar)
{
}


VariableSetting::VariableSetting(
    VariableID inputVar,
    const ScalarValue& value,
    const coral::model::Variable& outputVar)
    : m_variable(inputVar),
      m_hasValue(true),
      m_value(value),
      m_isConnectionChange(true),
      m_connectedOutput(outputVar)
{
}


VariableID VariableSetting::Variable() const noexcept
{
    return m_variable;
}


bool VariableSetting::HasValue() const noexcept
{
    return m_hasValue;
}


const ScalarValue& VariableSetting::Value() const
{
    CORAL_PRECONDITION_CHECK(HasValue());
    return m_value;
}


bool VariableSetting::IsConnectionChange() const noexcept
{
    return m_isConnectionChange;
}


const coral::model::Variable& VariableSetting::ConnectedOutput() const
{
    CORAL_PRECONDITION_CHECK(IsConnectionChange());
    return m_connectedOutput;
}


// =============================================================================
// Free functions
// =============================================================================


bool IsValidSlaveName(const std::string& s)
{
    if (s.empty()) return false;
    if (!std::isalpha(s.front())) return false;
    for (char c : s) if (!std::isalnum(c) && c != '_') return false;
    return true;
}


}} // namespace
