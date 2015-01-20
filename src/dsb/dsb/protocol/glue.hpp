/**
\file
\brief  Glue code that relates public APIs and internal communication protocols.
*/
#ifndef DSB_PROTOCOL_GLUE_HPP
#define DSB_PROTOCOL_GLUE_HPP

#include "dsb/model.hpp"
#include "variable.pb.h"

#include "dsb/execution/locator.hpp"
#include "domain.pb.h"


namespace dsb
{
namespace protocol
{


/// Converts a variable definition to a protocol buffer.
dsbproto::variable::VariableDefinition ToProto(
    const dsb::model::Variable& dsbVariable);

/// Converts a protocol buffer to a variable definition.
dsb::model::Variable FromProto(
    const dsbproto::variable::VariableDefinition& protoVariable);

/// Converts an execution locator to a protocol buffer.
dsbproto::domain::ExecutionLocator ToProto(
    const dsb::execution::Locator& executionLocator);

/// Converts a protocol buffer to an execution locator.
dsb::execution::Locator FromProto(
    const dsbproto::domain::ExecutionLocator& executionLocator);


}}      // namespace
#endif  // header guard
