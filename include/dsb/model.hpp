/**
\file
\brief  Main module header for dsb::model.
*/
#ifndef DSB_MODEL_HPP
#define DSB_MODEL_HPP

#include <cstdint>
#include <limits>
#include <string>
#include "boost/variant.hpp"
#include "dsb/config.h"


namespace dsb
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
typedef std::uint16_t VariableID;


/// Variable data types supported by the DSB.
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
        dsb::model::VariableID id,
        const std::string& name,
        dsb::model::DataType dataType,
        dsb::model::Causality causality,
        dsb::model::Variability variability);

    /**
    \brief  An identifier which uniquely refers to this variable in the context
            of a single slave type.

    Variable IDs are not unique across slave types.
    */
    dsb::model::VariableID ID() const;

    /**
    \brief  A human-readable name for the variable.

    The name is unique in the context of a single slave type.
    */
    const std::string& Name() const;

    /// The variable's data type.
    dsb::model::DataType DataType() const;

    /// The variable's causality.
    dsb::model::Causality Causality() const;

    /// The variable's variability.
    dsb::model::Variability Variability() const;

private:
    VariableID m_id;
    std::string m_name;
    dsb::model::DataType m_dataType;
    dsb::model::Causality m_causality;
    dsb::model::Variability m_variability;
};


/// An algebraic type that can hold values of all supported data types.
typedef boost::variant<double, int, bool, std::string> ScalarValue;


/**
\brief  An object that identifies a variable in a simulation, and which consists
        of a slave ID and a variable ID.
*/
class Variable
{
public:
    explicit Variable(SlaveID slave = INVALID_SLAVE_ID, VariableID id = 0)
        : m_slave(slave), m_id(id) { }

    dsb::model::SlaveID Slave() const DSB_NOEXCEPT { return m_slave; }
    dsb::model::VariableID ID() const DSB_NOEXCEPT { return m_id; }
    bool Empty() const DSB_NOEXCEPT { return m_slave == INVALID_SLAVE_ID; }

private:
    dsb::model::SlaveID m_slave;
    dsb::model::VariableID m_id;
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

    /// Indicates an input variable which should be connected to an output variable.
    VariableSetting(
        VariableID inputVar,
        const dsb::model::Variable& outputVar);

    /**
    \brief  Indicates an input variable which should both be given a specific
            value *and* connected to an output variable.
    */
    VariableSetting(
        VariableID inputVar,
        const ScalarValue& value,
        const dsb::model::Variable& outputVar);

    /// The variable ID.
    VariableID Variable() const DSB_NOEXCEPT;

    /// Whether the variable is to be given a value.
    bool HasValue() const DSB_NOEXCEPT;

    /**
    \brief  The variable value, if any.
    \pre `HasValue() == true`
    */
    const ScalarValue& Value() const;

    /// Whether the variable is to be connected.
    bool IsConnected() const DSB_NOEXCEPT;

    /**
    \brief  The output to which the variable is to be connected, if any.
    \pre `IsConnected() == true`
    */
    const dsb::model::Variable& ConnectedOutput() const;

private:
    VariableID m_variable;
    bool m_hasValue;
    ScalarValue m_value;
    dsb::model::Variable m_connectedOutput;
};


}}      // namespace
#endif  // header guard
