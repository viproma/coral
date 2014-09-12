#include "library.hpp"


namespace dsb { namespace library
{



const std::string& SlaveType::Name() const
{
    return m_name;
}


dsb::sequence::Sequence<const VariableInfo> SlaveType::Variables() const
{
    return dsb::sequence::ContainerSequence(m_variables);
}


// =============================================================================


const SlaveType* Library::FindSlaveType(const std::string& slaveTypeName)
{
    auto types = SlaveTypes();
    while (!types.Empty()) {
        const auto& t = types.Next();
        if (t.Name() == slaveTypeName) return &t;
    }
    return nullptr;
}


}} // namespace
