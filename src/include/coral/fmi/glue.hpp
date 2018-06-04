/**
\file
\brief Conversions between FMI variable attributes and "our" attributes.
\copyright
    Copyright 2013-present, SINTEF Ocean.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifndef CORAL_FMI_GLUE_HPP
#define CORAL_FMI_GLUE_HPP

#include <fmilib.h>
#include <coral/model.hpp>


namespace coral
{
namespace fmi
{


/// Converts an FMI 1.0 base type to "our" data type.
coral::model::DataType ToDataType(fmi1_base_type_enu_t t);


/// Converts an FMI 2.0 base type to "our" data type.
coral::model::DataType ToDataType(fmi2_base_type_enu_t t);


/**
\brief  Converts an FMI 1.0 variable causality to "our" corresponding causality.

The causality mapping is not unique, so the variable's variability is also
needed.
*/
coral::model::Causality ToCausality(
    fmi1_causality_enu_t c,
    fmi1_variability_enu_t v);


/// Converts an FMI 2.0 variable causality to "our" corresponding causality.
coral::model::Causality ToCausality(fmi2_causality_enu_t c);


/// Converts an FMI 1.0 variable variability to "our" corresponding variability.
coral::model::Variability ToVariability(fmi1_variability_enu_t v);


/// Converts an FMI 2.0 variable variability to "our" corresponding variability.
coral::model::Variability ToVariability(fmi2_variability_enu_t v);


/// Converts an FMI 1.0 variable description to a VariableDescription object.
coral::model::VariableDescription ToVariable(
    fmi1_import_variable_t* fmiVariable,
    coral::model::VariableID id);


/// Converts an FMI 2.0 variable description to a VariableDescription object.
coral::model::VariableDescription ToVariable(
    fmi2_import_variable_t* fmiVariable,
    coral::model::VariableID id);


}}      // namespace
#endif  // header guard
