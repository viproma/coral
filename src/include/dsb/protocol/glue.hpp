/**
\file
\brief  Glue code that relates public APIs and internal communication protocols.
*/
#ifndef DSB_PROTOCOL_GLUE_HPP
#define DSB_PROTOCOL_GLUE_HPP

#include "dsb/model.hpp"
#include "model.pb.h"
#include "net.pb.h"

#include "dsb/net.hpp"
#include "domain.pb.h"


namespace dsb
{
namespace protocol
{


/// Converts a variable description to a protocol buffer.
dsbproto::model::VariableDescription ToProto(
    const dsb::model::VariableDescription& dsbVariable);

/// Converts a protocol buffer to a variable definition.
dsb::model::VariableDescription FromProto(
    const dsbproto::model::VariableDescription& protoVariable);

/// Converts a slave type description to a protocol buffer.
dsbproto::model::SlaveTypeDescription ToProto(
    const dsb::model::SlaveTypeDescription& src);

/// Converts a protocol buffer to a slave type description.
dsb::model::SlaveTypeDescription FromProto(
    const dsbproto::model::SlaveTypeDescription& src);

/// Converts an execution locator to a protocol buffer.
dsbproto::net::ExecutionLocator ToProto(
    const dsb::net::ExecutionLocator& executionLocator);

/// Converts a protocol buffer to an execution locator.
dsb::net::ExecutionLocator FromProto(
    const dsbproto::net::ExecutionLocator& executionLocator);

/// Converts a ScalarValue to a protocol buffer (in place).
void ConvertToProto(
    const dsb::model::ScalarValue& source,
    dsbproto::model::ScalarValue& target);

/// Converts a protocol buffer to a ScalarValue.
dsb::model::ScalarValue FromProto(const dsbproto::model::ScalarValue& source);

/// Converts a Variable to a protocol buffer (in place).
void ConvertToProto(
    const dsb::model::Variable& source,
    dsbproto::model::Variable& target);

/// Converts a protocol buffer to a Variable.
dsb::model::Variable FromProto(const dsbproto::model::Variable& source);

void ConvertToProto(
    const dsb::net::SlaveLocator& source,
    dsbproto::net::SlaveLocator& target);

dsb::net::SlaveLocator FromProto(const dsbproto::net::SlaveLocator& source);

}}      // namespace
#endif  // header guard
