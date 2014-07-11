#ifndef DSB_MODEL_HPP
#define DSB_MODEL_HPP

#include <map>
#include <memory>
#include <ostream>
#include <string>

#include "boost/variant.hpp"

#include "library.hpp"


namespace dsb
{
namespace model
{


/// A variable identifier.
struct VariableID
{
    /**
    \brief  Constructs a variable identifier from a string.
    
    Note that the slave and variable names themselves are not checked, only
    the format of the full identifier.

    \param [in] qualifiedName   A string that contains the name of a slave and
                                the name of one of its variables, separated by
                                a dot (".").

    \post #slave is set to the portion of the string leading up to the first
        dot, while #variable is set to the portion of the string after the
        first dot.

    \throws std::runtime_error if `qualifiedName` does not have the appropriate
            format.
    */
    VariableID(const std::string& qualifiedName);

    /// Returns the full variable identifier, on the form "slave.variable".
    std::string QualifiedName() const;

    /// The name of the slave to which the variable belongs.
    std::string slave;

    /// The name of the variable.
    std::string variable;
};


/// A class that contains the value of a variable.
class VariableValue
{
public:
    /**
    \brief  Constructs a VariableValue of the given data type.

    \post `IsSet() == false`.
    */
    VariableValue(dsb::library::DataType dataType);

    /// Whether the variable value has been set yet.
    bool IsSet() const;

    /**
    \brief  Parses the given string according to the stored data type, and sets
            the variable to the resulting value.

    \post `IsSet() == true` if successful, otherwise the object is not modified.

    \throws std::bad_cast if the given string could not be interpreted as a
            value of the appropriate data type.
    */
    void Parse(const std::string& valueString);

private:
    dsb::library::DataType m_dataType;
    bool m_isSet;
    boost::variant<double, int, bool, std::string> m_value;

    friend std::ostream& operator<<(
        std::ostream& stream,
        const VariableValue& variableValue);
};


/// Writes a variable value to an output stream.
std::ostream& operator<<(std::ostream& stream, const VariableValue& variableValue);


/// A class that represents a system of connected slaves.
class Model
{
public:
    /**
    \brief  Adds a slave with the given name and type to the model.

    \throws std::runtime_error if a slave with the given name has already been
            added.
    */
    void AddSlave(
        const std::string& name,
        const dsb::library::SlaveType& type);

    /**
    \brief  Sets the value of one of a slave's variables by interpreting a
            string.

    \throws std::out_of_range if the slave has no slave called `slaveName`, or
            the slave has no variable named `variableName`.
    \throws std::bad_cast if `variableValue` could not be converted to the
            variable's data type.
    */
    void SetVariableFromString(
        const std::string& slaveName,
        const std::string& variableName,
        const std::string& variableValue);

    /// Connects an input variable to an output variable.
    void Connect(const VariableID& input, const VariableID& output);

    // This function is not part of the API; it is only here for debugging
    // purposes while the API is under development.  It will be removed later.
    void DebugDump() const;

private:
    struct Slave
    {
        dsb::library::SlaveType type;
        std::map<std::string, VariableValue> variableValues;
    };
    std::map<std::string, Slave> m_slaves;
    std::map<std::string, std::map<std::string, VariableID>> m_connections;
};


}}      // namespace
#endif  // header guard
