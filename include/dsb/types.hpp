/**
\file
\brief Main header file for dsb::types.
*/
#ifndef DSB_TYPES_HPP
#define DSB_TYPES_HPP

#include <cstdint>
#include <string>
#include <vector>
#include "boost/variant.hpp"
#include "dsb/model.hpp"


namespace dsb
{
/// Generally useful classes and type definitions.
namespace types
{


/// Information about a slave type.
struct SlaveType
{
    std::string name;
    std::string uuid;
    std::string description;
    std::string author;
    std::string version;
    std::vector<dsb::model::Variable> variables;
    std::vector<std::string> providers;
};


/// A tagged union of all data types supported by FMI.
typedef boost::variant<double, int, bool, std::string> ScalarValue;


/// A variable ID-value pair.
struct Variable
{
    uint16_t id;
    ScalarValue value;
};


struct VariableConnection
{
    uint16_t inputId;       ///< The input variable which is to be connected.
    uint16_t otherSlaveId;  ///< The slave whose output variable to connect to.
    uint16_t otherOutputId; ///< The output variable which is to be connected.
};


}}      // namespace
#endif  // header guard
