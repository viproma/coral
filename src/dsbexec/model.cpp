#include "model.hpp"

#include <cassert>
#include <iostream> // Only for Model::DebugDump(); remove later!
#include <stdexcept>
#include <utility>

#include "boost/foreach.hpp"
#include "boost/lexical_cast.hpp"


namespace dsb
{
namespace model
{


VariableID::VariableID(const std::string& qualifiedName)
{
    const auto dotPos = qualifiedName.find_first_of('.');
    if (dotPos >= qualifiedName.size() - 1) {
        throw std::runtime_error(
            "Invalid variable identifier (should be on the format \"slave.var\"): "
            + qualifiedName);
    }
    slave    = qualifiedName.substr(0, dotPos);
    variable = qualifiedName.substr(dotPos + 1);
}


std::string VariableID::QualifiedName() const
{
    return slave + "." + variable;
}


// =============================================================================


VariableValue::VariableValue(dsb::library::DataType dataType)
    : m_dataType(dataType), m_isSet(false)
{
    using namespace dsb::library;
    switch (dataType) {
        case REAL_DATATYPE:     m_value = 0.0;           break;
        case INTEGER_DATATYPE:  m_value = 0;             break;
        case BOOLEAN_DATATYPE:  m_value = false;         break;
        case STRING_DATATYPE:   m_value = std::string(); break;
        default: assert(!"Invalid data type passed to VariableValue constructor");
    }
}


bool VariableValue::IsSet() const
{
    return m_isSet;
}


void VariableValue::Parse(const std::string& valueString)
{
    using namespace dsb::library;
    switch (m_dataType) {
        case REAL_DATATYPE:     m_value = boost::lexical_cast<double>(valueString);      break;
        case INTEGER_DATATYPE:  m_value = boost::lexical_cast<int>(valueString);         break;
        case BOOLEAN_DATATYPE:  m_value = boost::lexical_cast<bool>(valueString);        break;
        case STRING_DATATYPE:   m_value = boost::lexical_cast<std::string>(valueString); break;
        default: assert(!"Invalid value of VariableValue.m_dataType");                   return;
    }
    m_isSet = true;
}


std::ostream& operator<<(std::ostream& stream, const VariableValue& variableValue)
{
    stream << variableValue.m_value;
    return stream;
}


// =============================================================================


void Model::AddSlave(
    const std::string& name,
    const dsb::library::SlaveType& type)
{
    Slave s = { type };
    auto vars = type.Variables();
    while (!vars.Empty()) {
        const auto& v = vars.Next();
        s.variableValues.insert(
            std::make_pair(v.name, dsb::model::VariableValue(v.dataType)));
    }
    if (!m_slaves.insert(std::make_pair(name, s)).second) {
        throw std::runtime_error("Slave already exists: " + name);
    }
}


void Model::SetVariableFromString(
    const std::string& slaveName,
    const std::string& variableName,
    const std::string& variableValue)
{
    m_slaves.at(slaveName).variableValues.at(variableName).Parse(variableValue);
}


void Model::Connect(const VariableID& input, const VariableID& output)
{
    if (m_slaves.count(input.slave) == 0) {
        throw std::runtime_error(
            "Attempted to connect nonexistent slave: " + input.slave);
    }
    if (m_slaves.count(output.slave) == 0) {
        throw std::runtime_error(
            "Attempted to connect nonexistent slave: " + output.slave);
    }
    auto ins = m_connections[input.slave].insert(
        std::make_pair(input.variable, output));
    if (!ins.second) {
        throw std::runtime_error(
            "VariableID connected multiple times: " + input.QualifiedName());
    }
}


void Model::DebugDump() const
{
    std::cout << "Slaves:\n";
    BOOST_FOREACH(const auto& s, m_slaves) {
        std::cout << "  " << s.first << " (" << s.second.type.Name() << ")\n";
        BOOST_FOREACH(const auto& v, s.second.variableValues) {
            std::cout << "    " << v.first << " = " << v.second << "\n";
        }
    }
    std::cout << "Connections:\n";
    BOOST_FOREACH(const auto& is, m_connections) {
        BOOST_FOREACH(const auto& iv, is.second) {
            std::cout << "  " << iv.second.QualifiedName() << " -> "
                      << is.first << "." << iv.first << "\n";
        }
    }
}


}} // namespace
