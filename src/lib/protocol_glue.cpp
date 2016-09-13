#include "coral/protocol/glue.hpp"

#include <algorithm>
#include <cassert>
#include <iterator>
#include <vector>


namespace
{
    void ConvertToProto_(
        const coral::model::VariableDescription& ourVariable,
        coralproto::model::VariableDescription& protoVariable)
    {
        protoVariable.set_id(ourVariable.ID());
        protoVariable.set_name(ourVariable.Name());
        switch (ourVariable.DataType()) {
            case coral::model::REAL_DATATYPE:
                protoVariable.set_data_type(coralproto::model::REAL);
                break;
            case coral::model::INTEGER_DATATYPE:
                protoVariable.set_data_type(coralproto::model::INTEGER);
                break;
            case coral::model::BOOLEAN_DATATYPE:
                protoVariable.set_data_type(coralproto::model::BOOLEAN);
                break;
            case coral::model::STRING_DATATYPE:
                protoVariable.set_data_type(coralproto::model::STRING);
                break;
            default:
                assert (!"Unknown data type");
        }
        switch (ourVariable.Causality()) {
            case coral::model::PARAMETER_CAUSALITY:
                protoVariable.set_causality(coralproto::model::PARAMETER);
                break;
            case coral::model::CALCULATED_PARAMETER_CAUSALITY:
                protoVariable.set_causality(coralproto::model::CALCULATED_PARAMETER);
                break;
            case coral::model::INPUT_CAUSALITY:
                protoVariable.set_causality(coralproto::model::INPUT);
                break;
            case coral::model::OUTPUT_CAUSALITY:
                protoVariable.set_causality(coralproto::model::OUTPUT);
                break;
            case coral::model::LOCAL_CAUSALITY:
                protoVariable.set_causality(coralproto::model::LOCAL);
                break;
            default:
                assert (!"Unknown causality");
        }
        switch (ourVariable.Variability()) {
            case coral::model::CONSTANT_VARIABILITY:
                protoVariable.set_variability(coralproto::model::CONSTANT);
                break;
            case coral::model::FIXED_VARIABILITY:
                protoVariable.set_variability(coralproto::model::FIXED);
                break;
            case coral::model::TUNABLE_VARIABILITY:
                protoVariable.set_variability(coralproto::model::TUNABLE);
                break;
            case coral::model::DISCRETE_VARIABILITY:
                protoVariable.set_variability(coralproto::model::DISCRETE);
                break;
            case coral::model::CONTINUOUS_VARIABILITY:
                protoVariable.set_variability(coralproto::model::CONTINUOUS);
                break;
            default:
                assert (!"Unknown variability");
        }
    }
}


coralproto::model::VariableDescription coral::protocol::ToProto(
    const coral::model::VariableDescription& ourVariable)
{
    coralproto::model::VariableDescription protoVariable;
    ConvertToProto_(ourVariable, protoVariable);
    return protoVariable;
}


coral::model::VariableDescription coral::protocol::FromProto(
    const coralproto::model::VariableDescription& protoVariable)
{
    coral::model::DataType dataType = coral::model::REAL_DATATYPE;
    switch (protoVariable.data_type()) {
        case coralproto::model::REAL:
            dataType = coral::model::REAL_DATATYPE;
            break;
        case coralproto::model::INTEGER:
            dataType = coral::model::INTEGER_DATATYPE;
            break;
        case coralproto::model::BOOLEAN:
            dataType = coral::model::BOOLEAN_DATATYPE;
            break;
        case coralproto::model::STRING:
            dataType = coral::model::STRING_DATATYPE;
            break;
        default:
            assert (!"Unknown data type");
    }
    coral::model::Causality causality = coral::model::LOCAL_CAUSALITY;
    switch (protoVariable.causality()) {
        case coralproto::model::PARAMETER:
            causality = coral::model::PARAMETER_CAUSALITY;
            break;
        case coralproto::model::CALCULATED_PARAMETER:
            causality = coral::model::CALCULATED_PARAMETER_CAUSALITY;
            break;
        case coralproto::model::INPUT:
            causality = coral::model::INPUT_CAUSALITY;
            break;
        case coralproto::model::OUTPUT:
            causality = coral::model::OUTPUT_CAUSALITY;
            break;
        case coralproto::model::LOCAL:
            causality = coral::model::LOCAL_CAUSALITY;
            break;
        default:
            assert (!"Unknown causality");
    }
    coral::model::Variability variability = coral::model::CONTINUOUS_VARIABILITY;
    switch (protoVariable.variability()) {
        case coralproto::model::CONSTANT:
            variability = coral::model::CONSTANT_VARIABILITY;
            break;
        case coralproto::model::FIXED:
            variability = coral::model::FIXED_VARIABILITY;
            break;
        case coralproto::model::TUNABLE:
            variability = coral::model::TUNABLE_VARIABILITY;
            break;
        case coralproto::model::DISCRETE:
            variability = coral::model::DISCRETE_VARIABILITY;
            break;
        case coralproto::model::CONTINUOUS:
            variability = coral::model::CONTINUOUS_VARIABILITY;
            break;
        default:
            assert (!"Unknown variability");
    }
    return coral::model::VariableDescription(
        protoVariable.id(),
        protoVariable.name(),
        dataType,
        causality,
        variability);
}


coralproto::model::SlaveTypeDescription coral::protocol::ToProto(
    const coral::model::SlaveTypeDescription& src)
{
    coralproto::model::SlaveTypeDescription tgt;
    tgt.set_name(src.Name());
    tgt.set_uuid(src.UUID());
    tgt.set_description(src.Description());
    tgt.set_author(src.Author());
    tgt.set_version(src.Version());
    for (const auto& srcVar : src.Variables()) {
        ConvertToProto_(srcVar, *tgt.add_variable());
    }
    return tgt;
}


coral::model::SlaveTypeDescription coral::protocol::FromProto(
    const coralproto::model::SlaveTypeDescription& src)
{
    std::vector<coral::model::VariableDescription> tgtVars;
    for (const auto& srcVar : src.variable()) tgtVars.push_back(FromProto(srcVar));
    return coral::model::SlaveTypeDescription(
        src.name(),
        src.uuid(),
        src.description(),
        src.author(),
        src.version(),
        tgtVars);
}


namespace
{
    class ScalarValueConverterVisitor : public boost::static_visitor<>
    {
    public:
        explicit ScalarValueConverterVisitor(coralproto::model::ScalarValue& value)
            : m_value(&value) { }
        void operator()(const double& value)      const { m_value->set_real_value(value); }
        void operator()(const int& value)         const { m_value->set_integer_value(value); }
        void operator()(const bool& value)        const { m_value->set_boolean_value(value); }
        void operator()(const std::string& value) const { m_value->set_string_value(value); }
    private:
        coralproto::model::ScalarValue* m_value;
    };
}

void coral::protocol::ConvertToProto(
    const coral::model::ScalarValue& source,
    coralproto::model::ScalarValue& target)
{
    target.Clear();
    boost::apply_visitor(ScalarValueConverterVisitor(target), source);
}


coral::model::ScalarValue coral::protocol::FromProto(
    const coralproto::model::ScalarValue& source)
{
    if (source.has_real_value())            return source.real_value();
    else if (source.has_integer_value())    return source.integer_value();
    else if (source.has_boolean_value())    return source.boolean_value();
    else if (source.has_string_value())     return source.string_value();
    else {
        assert (!"Corrupt or empty ScalarValue protocol buffer");
        return coral::model::ScalarValue();
    }
}


void coral::protocol::ConvertToProto(
    const coral::model::Variable& source,
    coralproto::model::Variable& target)
{
    target.Clear();
    target.set_slave_id(source.Slave());
    target.set_variable_id(source.ID());
}


coral::model::Variable coral::protocol::FromProto(const coralproto::model::Variable& source)
{
    return coral::model::Variable(source.slave_id(), source.variable_id());
}


void coral::protocol::ConvertToProto(
    const coral::net::SlaveLocator& source,
    coralproto::net::SlaveLocator& target)
{
    target.Clear();
    target.set_control_endpoint(source.ControlEndpoint().URL());
    target.set_data_pub_endpoint(source.DataPubEndpoint().URL());
}


coral::net::SlaveLocator coral::protocol::FromProto(
    const coralproto::net::SlaveLocator& source)
{
    return coral::net::SlaveLocator(
        coral::net::Endpoint(source.control_endpoint()),
        coral::net::Endpoint(source.data_pub_endpoint()));
}
