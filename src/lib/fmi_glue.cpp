/*
Copyright 2013-2017, SINTEF Ocean and the Coral contributors.
This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#include <coral/fmi/glue.hpp>

#include <cassert>
#include <stdexcept>

#include <coral/log.hpp>


namespace coral
{
namespace fmi
{


coral::model::DataType ToDataType(fmi1_base_type_enu_t t)
{
    switch (t) {
        case fmi1_base_type_real: return coral::model::REAL_DATATYPE;
        case fmi1_base_type_int:  return coral::model::INTEGER_DATATYPE;
        case fmi1_base_type_bool: return coral::model::BOOLEAN_DATATYPE;
        case fmi1_base_type_str:  return coral::model::STRING_DATATYPE;
        case fmi1_base_type_enum:
            throw std::runtime_error("FMI 1.0 enumeration variable types not supported yet");
        default:
            throw std::logic_error("Unknown or unsupported FMI 1.0 variable data type");
    }
}


coral::model::DataType ToDataType(fmi2_base_type_enu_t t)
{
    switch (t) {
        case fmi2_base_type_real: return coral::model::REAL_DATATYPE;
        case fmi2_base_type_int:  return coral::model::INTEGER_DATATYPE;
        case fmi2_base_type_bool: return coral::model::BOOLEAN_DATATYPE;
        case fmi2_base_type_str:  return coral::model::STRING_DATATYPE;
        case fmi2_base_type_enum:
            throw std::runtime_error("FMI 2.0 enumeration variable types not supported yet");
        default:
            throw std::logic_error("Unknown or unsupported FMI 2.0 variable data type");
    }
}


coral::model::Causality ToCausality(
    fmi1_causality_enu_t c,
    fmi1_variability_enu_t v)
{
    switch (c) {
        case fmi1_causality_enu_input:
            return (v == fmi1_variability_enu_parameter)
                ? coral::model::PARAMETER_CAUSALITY
                : coral::model::INPUT_CAUSALITY;
        case fmi1_causality_enu_output:
            return coral::model::OUTPUT_CAUSALITY;
        case fmi1_causality_enu_internal:
        case fmi1_causality_enu_none:
             return coral::model::LOCAL_CAUSALITY;
        default:
            throw std::logic_error("Unknown or unsupported FMI 1.0 variable causality encountered");
    }
}


coral::model::Causality ToCausality(fmi2_causality_enu_t c)
{
    switch (c) {
        case fmi2_causality_enu_parameter:              return coral::model::PARAMETER_CAUSALITY;
        case fmi2_causality_enu_calculated_parameter:   return coral::model::CALCULATED_PARAMETER_CAUSALITY;
        case fmi2_causality_enu_input:                  return coral::model::INPUT_CAUSALITY;
        case fmi2_causality_enu_output:                 return coral::model::OUTPUT_CAUSALITY;
        case fmi2_causality_enu_local:                  return coral::model::LOCAL_CAUSALITY;
        case fmi2_causality_enu_independent:
            coral::log::Log(
                coral::log::warning,
                "Encountered variable with FMI 2.0 causality \"independent\", "
                "which is not supported. Falling back to \"local\".");
            return coral::model::LOCAL_CAUSALITY;
        default:
            throw std::logic_error("Unknown or unsupported FMI 2.0 variable causality encountered");
    }
}


coral::model::Variability ToVariability(fmi1_variability_enu_t v)
{
    switch (v) {
        case fmi1_variability_enu_constant:   return coral::model::CONSTANT_VARIABILITY;
        case fmi1_variability_enu_parameter:  return coral::model::FIXED_VARIABILITY;
        case fmi1_variability_enu_discrete:   return coral::model::DISCRETE_VARIABILITY;
        case fmi1_variability_enu_continuous: return coral::model::CONTINUOUS_VARIABILITY;
        case fmi1_variability_enu_unknown:
            assert (!"Variable with variability 'unknown' encountered");
            return coral::model::CONTINUOUS_VARIABILITY; // Safe fallback.
        default:
            throw std::logic_error("Unknown or unsupported FMI 1.0 variable variability encountered");
    }
}


coral::model::Variability ToVariability(fmi2_variability_enu_t v)
{
    switch (v) {
        case fmi2_variability_enu_constant:     return coral::model::CONSTANT_VARIABILITY;
        case fmi2_variability_enu_fixed:        return coral::model::FIXED_VARIABILITY;
        case fmi2_variability_enu_tunable:      return coral::model::TUNABLE_VARIABILITY;
        case fmi2_variability_enu_discrete:     return coral::model::DISCRETE_VARIABILITY;
        case fmi2_variability_enu_continuous:   return coral::model::CONTINUOUS_VARIABILITY;
        default:
            throw std::logic_error("Unknown or unsupported FMI 1.0 variable variability encountered");
    }
}


coral::model::VariableDescription ToVariable(
    fmi1_import_variable_t* fmiVariable,
    coral::model::VariableID id)
{
    assert (fmiVariable != nullptr);
    const auto fmiVariability = fmi1_import_get_variability(fmiVariable);
    return coral::model::VariableDescription(
        id,
        fmi1_import_get_variable_name(fmiVariable),
        ToDataType(fmi1_import_get_variable_base_type(fmiVariable)),
        ToCausality(fmi1_import_get_causality(fmiVariable), fmiVariability),
        ToVariability(fmiVariability));
}


coral::model::VariableDescription ToVariable(
    fmi2_import_variable_t* fmiVariable,
    coral::model::VariableID id)
{
    assert (fmiVariable != nullptr);
    return coral::model::VariableDescription(
        id,
        fmi2_import_get_variable_name(fmiVariable),
        ToDataType(fmi2_import_get_variable_base_type(fmiVariable)),
        ToCausality(fmi2_import_get_causality(fmiVariable)),
        ToVariability(fmi2_import_get_variability(fmiVariable)));
}


}} // namespace
