/**
\file
\brief  Glue code that relates public APIs and internal communication protocols.
*/
#ifndef CORAL_PROTOCOL_GLUE_HPP
#define CORAL_PROTOCOL_GLUE_HPP

#include "coral/model.hpp"
#include "model.pb.h"
#include "net.pb.h"

#include "coral/net.hpp"
#include "domain.pb.h"


namespace coral
{
namespace protocol
{


/// Converts a variable description to a protocol buffer.
coralproto::model::VariableDescription ToProto(
    const coral::model::VariableDescription& ourVariable);

/// Converts a protocol buffer to a variable definition.
coral::model::VariableDescription FromProto(
    const coralproto::model::VariableDescription& protoVariable);

/// Converts a slave type description to a protocol buffer.
coralproto::model::SlaveTypeDescription ToProto(
    const coral::model::SlaveTypeDescription& src);

/// Converts a protocol buffer to a slave type description.
coral::model::SlaveTypeDescription FromProto(
    const coralproto::model::SlaveTypeDescription& src);

/// Converts a ScalarValue to a protocol buffer (in place).
void ConvertToProto(
    const coral::model::ScalarValue& source,
    coralproto::model::ScalarValue& target);

/// Converts a protocol buffer to a ScalarValue.
coral::model::ScalarValue FromProto(const coralproto::model::ScalarValue& source);

/// Converts a Variable to a protocol buffer (in place).
void ConvertToProto(
    const coral::model::Variable& source,
    coralproto::model::Variable& target);

/// Converts a protocol buffer to a Variable.
coral::model::Variable FromProto(const coralproto::model::Variable& source);

void ConvertToProto(
    const coral::net::SlaveLocator& source,
    coralproto::net::SlaveLocator& target);

coral::net::SlaveLocator FromProto(const coralproto::net::SlaveLocator& source);

}}      // namespace
#endif  // header guard
