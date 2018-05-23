/**
\file
\brief  Glue code that relates public APIs and internal communication protocols.
\copyright
    Copyright 2013-2017, SINTEF Ocean and the Coral contributors.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifndef CORAL_PROTOCOL_GLUE_HPP
#define CORAL_PROTOCOL_GLUE_HPP

#include <coral/model.hpp>
#include <coral/net.hpp>

#ifdef _MSC_VER
#   pragma warning(push, 0)
#endif
#include <domain.pb.h>
#include <model.pb.h>
#include <net.pb.h>
#ifdef _MSC_VER
#   pragma warning(pop)
#endif


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
