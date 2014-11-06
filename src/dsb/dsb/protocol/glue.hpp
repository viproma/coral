/**
\file
\brief  Glue code that relates public APIs and internal communication protocols.
*/
#ifndef DSB_PROTOCOL_GLUE_HPP
#define DSB_PROTOCOL_GLUE_HPP

#include "dsb/model/variable.hpp"
#include "variable.pb.h"


namespace dsb
{
namespace protocol
{


dsbproto::variable::VariableDefinition ToProto(
    const dsb::model::Variable& dsbVariable);


dsb::model::Variable FromProto(
    const dsbproto::variable::VariableDefinition& protoVariable);


}}      // namespace
#endif  // header guard
