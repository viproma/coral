/**
\file
\brief  Main module header for coral::model.
\copyright
    Copyright 2013-2017, SINTEF Ocean and the Coral contributors.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifndef CORAL_MODEL_HPP
#define CORAL_MODEL_HPP

#include <cstdint>
#include <iterator>
#include <limits>
#include <map>
#include <string>
#include <vector>

#include <boost/range/adaptor/map.hpp>
#include <boost/variant.hpp>

#include <coral/config.h>


namespace coral
{
/// Types and constants that describe model structure.
namespace model
{


/// A number that uniquely identifies a time step in an execution.
typedef std::int32_t StepID;


/// A number which will never be used for an actual time step ID.
const StepID INVALID_STEP_ID = -1;


/// The type used to specify (simulation) time points.
typedef double TimePoint;


/// A special TimePoint value that lies infinitely far in the future.
const TimePoint ETERNITY = std::numeric_limits<TimePoint>::infinity();


/**
\brief  The type used to specify (simulation) time durations.

If `t1` and `t2` have type TimePoint, then `t2-t1` has type TimeDuration.
If `t` has type TimePoint and `dt` has type TimeDuration, then `t+dt` has type
TimePoint.
*/
typedef double TimeDuration;


/// Unsigned integer type used for slave identifiers.
typedef std::uint16_t SlaveID;


/// An invalid slave identifier.
const SlaveID INVALID_SLAVE_ID = 0;


/// Unsigned integer type used for variable identifiers.
typedef std::uint32_t VariableID;


/// Variable data types.
enum DataType
{
    REAL_DATATYPE       = 1,
    INTEGER_DATATYPE    = 1 << 1,
    BOOLEAN_DATATYPE    = 1 << 2,
    STRING_DATATYPE     = 1 << 3,
};


/// Variable causalities.  These correspond to FMI causality definitions.
enum Causality
{
    PARAMETER_CAUSALITY             = 1,
    CALCULATED_PARAMETER_CAUSALITY  = 1 << 1,
    INPUT_CAUSALITY                 = 1 << 2,
    OUTPUT_CAUSALITY                = 1 << 3,
    LOCAL_CAUSALITY                 = 1 << 4,
};


/// Variable variabilities.  These correspond to FMI variability definitions.
enum Variability
{
    CONSTANT_VARIABILITY    = 1,
    FIXED_VARIABILITY       = 1 << 1,
    TUNABLE_VARIABILITY     = 1 << 2,
    DISCRETE_VARIABILITY    = 1 << 3,
    CONTINUOUS_VARIABILITY  = 1 << 4,
};


/// A description of a single variable.
class VariableDescription
{
public:
    VariableDescription(
        coral::model::VariableID id,
        const std::string& name,
        coral::model::DataType dataType,
        coral::model::Causality causality,
        coral::model::Variability variability);

    /**
    \brief  An identifier which uniquely refers to this variable in the context
            of a single slave type.

    Variable IDs are not unique across slave types.
    */
    coral::model::VariableID ID() const;

    /**
    \brief  A human-readable name for the variable.

    The name is unique in the context of a single slave type.
    */
    const std::string& Name() const;

    /// The variable's data type.
    coral::model::DataType DataType() const;

    /// The variable's causality.
    coral::model::Causality Causality() const;

    /// The variable's variability.
    coral::model::Variability Variability() const;

private:
    VariableID m_id;
    std::string m_name;
    coral::model::DataType m_dataType;
    coral::model::Causality m_causality;
    coral::model::Variability m_variability;
};


/// A description of a slave type.
class SlaveTypeDescription
{
    typedef std::map<VariableID, VariableDescription> VariablesMap;
public:
    /// The return type of the Variables() function.
    typedef boost::select_second_const_range<VariablesMap>
        ConstVariablesRange;

    // Construction/destruction
    SlaveTypeDescription() CORAL_NOEXCEPT;

    template<typename VariableDescriptionRange>
    SlaveTypeDescription(
        const std::string& name,
        const std::string& uuid,
        const std::string& description,
        const std::string& author,
        const std::string& version,
        const VariableDescriptionRange& variables);

    ~SlaveTypeDescription() = default;

    // Copy
    SlaveTypeDescription(const SlaveTypeDescription&) = default;
    SlaveTypeDescription& operator=(const SlaveTypeDescription&) = default;

    // Move
    CORAL_DEFINE_DEFAULT_MOVE(SlaveTypeDescription,
        m_name, m_uuid, m_description, m_author, m_version, m_variables)

    /// The slave type name.
    const std::string& Name() const;

    /// A universally unique identifier (UUID) for the slave type.
    const std::string& UUID() const;

    /// A human-readable description of the slave type.
    const std::string& Description() const;

    /// Author information.
    const std::string& Author() const;

    /// Version information.
    const std::string& Version() const;

    /// Information about all variables.
    ConstVariablesRange Variables() const;

    /**
    \brief  Information about the variable with the given ID, O(log n) lookup.

    \throws std::out_of_range
        If there is no variable with the given ID.
    */
    const VariableDescription& Variable(VariableID id) const;

private:
    std::string m_name;
    std::string m_uuid;
    std::string m_description;
    std::string m_author;
    std::string m_version;
    VariablesMap m_variables;
};


/// A description of a specific slave.
class SlaveDescription
{
public:
    // Construction/destruction
    explicit SlaveDescription(
        SlaveID id = INVALID_SLAVE_ID,
        const std::string& name = std::string(),
        const SlaveTypeDescription& typeDescription = SlaveTypeDescription());

    ~SlaveDescription() = default;

    // Copy
    SlaveDescription(const SlaveDescription&) = default;
    SlaveDescription& operator=(const SlaveDescription&) = default;

    // Move
    CORAL_DEFINE_DEFAULT_MOVE(SlaveDescription, m_id, m_name, m_typeDescription)

    /// The slave's ID in the current execution.
    SlaveID ID() const;

    /// Sets the slave's ID in the current execution.
    void SetID(SlaveID value);

    /// The name given to the slave in the current execution.
    const std::string& Name() const;

    /// Sets the name given to the slave in the current execution.
    void SetName(const std::string& value);

    /// Information about the slave type.
    const SlaveTypeDescription& TypeDescription() const;

    /// Sets information about the slave type.
    void SetTypeDescription(const SlaveTypeDescription& value);

private:
    SlaveID m_id;
    std::string m_name;
    SlaveTypeDescription m_typeDescription;
};


/// An algebraic type that can hold values of all supported data types.
typedef boost::variant<double, int, bool, std::string> ScalarValue;


/// Returns the type of data stored in the given ScalarValue.
DataType DataTypeOf(const ScalarValue& v);


/**
\brief  An object that identifies a variable in a simulation, and which consists
        of a slave ID and a variable ID.
*/
class Variable
{
public:
    explicit Variable(SlaveID slave = INVALID_SLAVE_ID, VariableID id = 0)
        : m_slave(slave), m_id(id) { }

    coral::model::SlaveID Slave() const CORAL_NOEXCEPT { return m_slave; }
    coral::model::VariableID ID() const CORAL_NOEXCEPT { return m_id; }
    bool Empty() const CORAL_NOEXCEPT { return m_slave == INVALID_SLAVE_ID; }

private:
    coral::model::SlaveID m_slave;
    coral::model::VariableID m_id;
};


/**
\brief  Equality comparison for Variable objects.

Returns true if a and b have the same slave and variable ID numbers, or if
both are invalid/empty.
*/
bool operator==(const Variable& a, const Variable& b);

/// Inequality comparison for Variable objects, defined as `!(a==b)`.
bool operator!=(const Variable& a, const Variable& b);


/**
\brief  An object which represents the action of assigning an initial value to
        a variable, or to connect it to another variable.
*/
class VariableSetting
{
public:
    /// Indicates a variable which should be given a specific value.
    VariableSetting(
        VariableID variable,
        const ScalarValue& value);

    /**
    \brief  Indicates an input variable which should be connected to, or
            disconnected from, an output variable.

    If `outputVar` is a default-constructed `Variable` object (i.e., if
    `outputVar.Empty()` is `true`) this is equivalent to "no connection",
    meaning that an existing connection should be broken.
    */
    VariableSetting(
        VariableID inputVar,
        const coral::model::Variable& outputVar);

    /**
    \brief  Indicates an input variable which should both be given a specific
            value *and* connected to or disconnected from an output variable.

    If `outputVar` is a default-constructed `Variable` object (i.e., if
    `outputVar.Empty()` is `true`) this is equivalent to "no connection",
    meaning that an existing connection should be broken.
    */
    VariableSetting(
        VariableID inputVar,
        const ScalarValue& value,
        const coral::model::Variable& outputVar);

    /// The variable ID.
    VariableID Variable() const CORAL_NOEXCEPT;

    /// Whether the variable is to be given a value.
    bool HasValue() const CORAL_NOEXCEPT;

    /**
    \brief  The variable value, if any.
    \pre `HasValue() == true`
    */
    const ScalarValue& Value() const;

    /// Whether this represents a variable connection change.
    bool IsConnectionChange() const CORAL_NOEXCEPT;

    /**
    \brief  The output to which the variable is to be connected, if any.
    \pre `IsConnectionChange() == true`
    */
    const coral::model::Variable& ConnectedOutput() const;

private:
    VariableID m_variable;
    bool m_hasValue;
    ScalarValue m_value;
    bool m_isConnectionChange;
    coral::model::Variable m_connectedOutput;
};


/**
\brief  Returns whether `s` contains a valid slave name.

Basically, this checks that `s` matches the regular expression
`[a-zA-Z][0-9a-zA-Z_]*`.
*/
bool IsValidSlaveName(const std::string& s);


// =============================================================================
// Function template definitions
// =============================================================================

template<typename VariableDescriptionRange>
SlaveTypeDescription::SlaveTypeDescription(
    const std::string& name,
    const std::string& uuid,
    const std::string& description,
    const std::string& author,
    const std::string& version,
    const VariableDescriptionRange& variables)
    : m_name(name),
      m_uuid(uuid),
      m_description(description),
      m_author(author),
      m_version(version)
{
    for (const auto& v : variables) {
        m_variables.insert(std::make_pair(v.ID(), v));
    }
}


}}      // namespace
#endif  // header guard
