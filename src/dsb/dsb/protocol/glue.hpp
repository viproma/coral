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


dsbproto::variable::VariableDefinition ToProto(
    const dsb::model::Variable& dsbVariable);


dsb::model::Variable FromProto(
    const dsbproto::variable::VariableDefinition& protoVariable);


dsbproto::domain::ExecutionLocator ToProto(
    const dsb::execution::Locator& executionLocator);


dsb::execution::Locator FromProto(
    const dsbproto::domain::ExecutionLocator& executionLocator);

}}      // namespace
#endif  // header guard
