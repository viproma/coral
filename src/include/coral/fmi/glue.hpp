/**
\file
\brief Conversions between FMI variable attributes and "our" attributes.
*/
#ifndef CORAL_FMI_GLUE_HPP
#define CORAL_FMI_GLUE_HPP

#include "fmilib.h"
#include "coral/model.hpp"


namespace coral
{
namespace fmi
{


/// Converts an FMI 1.0 base type to "our" data type.
coral::model::DataType ToDataType(fmi1_base_type_enu_t t);


/**
\brief  Converts an FMI 1.0 variable causality to "our" corresponding causality.

The causality mapping is not unique, so the variable's variability is also
needed.
*/
coral::model::Causality ToCausality(
    fmi1_causality_enu_t c,
    fmi1_variability_enu_t v);


/// Converts an FMI 1.0 variable variability to "our" corresponding variability.
coral::model::Variability ToVariability(fmi1_variability_enu_t v);


/// Converts an FMI 1.0 variable description to a Variable object.
coral::model::VariableDescription ToVariable(
    fmi1_import_variable_t* fmiVariable,
    coral::model::VariableID id);


}}      // namespace
#endif  // header guard
