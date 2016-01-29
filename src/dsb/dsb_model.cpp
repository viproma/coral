#include "dsb/model.hpp"

#include "dsb/error.hpp"
#include "dsb/util.hpp"


namespace dsb
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
        DataType operator()(double)      const DSB_NOEXCEPT { return REAL_DATATYPE; }
        DataType operator()(int)         const DSB_NOEXCEPT { return INTEGER_DATATYPE; }
        DataType operator()(bool)        const DSB_NOEXCEPT { return BOOLEAN_DATATYPE; }
        DataType operator()(std::string) const DSB_NOEXCEPT { return STRING_DATATYPE; }
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
// SlaveTypeDescription
// =============================================================================

SlaveTypeDescription::SlaveTypeDescription() DSB_NOEXCEPT
{
}


SlaveTypeDescription::SlaveTypeDescription(SlaveTypeDescription&& other)
    DSB_NOEXCEPT
    : m_name(std::move(other.m_name)),
      m_uuid(std::move(other.m_uuid)),
      m_description(std::move(other.m_description)),
      m_author(std::move(other.m_author)),
      m_version(std::move(other.m_version)),
      m_variables(std::move(other.m_variables))
{
}

SlaveTypeDescription& SlaveTypeDescription::operator=(SlaveTypeDescription&& other)
    DSB_NOEXCEPT
{
    m_name = std::move(other.m_name);
    m_uuid = std::move(other.m_uuid);
    m_description = std::move(other.m_description);
    m_author = std::move(other.m_author);
    m_version = std::move(other.m_version);
    m_variables = std::move(other.m_variables);
    return *this;
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


SlaveDescription::SlaveDescription(SlaveDescription&& other)
    DSB_NOEXCEPT
    : m_id(dsb::util::MoveAndReplace(other.m_id, INVALID_SLAVE_ID)),
      m_name(std::move(other.m_name)),
      m_typeDescription(std::move(other.m_typeDescription))
{
}


SlaveDescription& SlaveDescription::operator=(SlaveDescription&& other)
    DSB_NOEXCEPT
{
    m_id = dsb::util::MoveAndReplace(other.m_id, INVALID_SLAVE_ID);
    m_name = std::move(other.m_name);
    m_typeDescription = std::move(other.m_typeDescription);
    return *this;
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
