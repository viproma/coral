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


namespace dsb
{
/// Types and constants that describe model structure.
namespace model
{


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
class Variable
{
public:
    Variable(
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


/// A variable ID-value pair.
struct VariableValue
{
    VariableID id;
    ScalarValue value;
};


/**
\brief  A type that describes a variable connection.

The variable connection is described from the perspective of the slave whose
input is being connected (i.e., that slave's ID is implied known, and therefore
not included).
*/
struct VariableConnection
{
    VariableID inputId;       ///< The input variable which is to be connected.
    SlaveID otherSlaveId;     ///< The slave whose output variable to connect to.
    VariableID otherOutputId; ///< The output variable which is to be connected.
};


}}      // namespace
#endif  // header guard
