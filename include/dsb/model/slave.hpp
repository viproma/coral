/**
\file
\brief Types that describe and/or identify slaves.
*/
#ifndef DSB_MODEL_SLAVE_HPP
#define DSB_MODEL_SLAVE_HPP

#include <cstdint>
#include <string>
#include <vector>
#include "dsb/model/variable.hpp"


namespace dsb
{
namespace model
{


/// Unsigned integer type used for slave identification numbers.
typedef std::uint16_t SlaveID;


/// Information about a slave type.
struct SlaveType
{
    std::string name;
    std::string uuid;
    std::string description;
    std::string author;
    std::string version;
    std::vector<Variable> variables;
    std::vector<std::string> providers;
};


/// A type that specifies a variable connection to a slave.
struct VariableConnection
{
    VariableID inputId;       ///< The input variable which is to be connected.
    SlaveID otherSlaveId;     ///< The slave whose output variable to connect to.
    VariableID otherOutputId; ///< The output variable which is to be connected.
};


}}      // namespace
#endif  // header guard
