/**
\file
\brief Main header file for dsb::types.
*/
#ifndef DSB_TYPES_HPP
#define DSB_TYPES_HPP

#include <cstdint>


namespace dsb
{
/// Generally useful classes and type definitions.
namespace types
{


/**
\brief  A variable ID-value pair.

Currently, this type is only used with dsb::execution::Controller::SetVariables(),
and it only supports real variables.  This will change in the future.
*/
// TODO: Make this more generic.
struct Variable
{
    uint16_t id;
    double value;
};


struct VariableConnection
{
    uint16_t inputId;       ///< The input variable which is to be connected.
    uint16_t otherSlaveId;  ///< The slave whose output variable to connect to.
    uint16_t otherOutputId; ///< The output variable which is to be connected.
};


}}      // namespace
#endif  // header guard
