#include "dsb/model.hpp"


namespace dsb
{
namespace model
{


Variable::Variable(
    dsb::model::VariableID id,
    const std::string& name,
    dsb::model::DataType dataType,
    dsb::model::Causality causality,
    dsb::model::Variability variability)
    : m_id(id),
      m_name(name),
      m_dataType(dataType),
      m_causality(causality),
      m_variability(variability)
{ }


dsb::model::VariableID Variable::ID() const
{
    return m_id;
}


const std::string& Variable::Name() const
{
    return m_name;
}


dsb::model::DataType Variable::DataType() const
{
    return m_dataType;
}


dsb::model::Causality Variable::Causality() const
{
    return m_causality;
}


dsb::model::Variability Variable::Variability() const
{
    return m_variability;
}


}} // namespace
