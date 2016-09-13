#include "coral/fmi/glue.hpp"

#include <cassert>
#include <stdexcept>


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


}} // namespace
