#include "dsb/protocol/glue.hpp"

#include <algorithm>
#include <cassert>
#include <iterator>
#include <vector>


namespace
{
    void ConvertToProto_(
        const dsb::model::VariableDescription& dsbVariable,
        dsbproto::model::VariableDescription& protoVariable)
    {
        protoVariable.set_id(dsbVariable.ID());
        protoVariable.set_name(dsbVariable.Name());
        switch (dsbVariable.DataType()) {
            case dsb::model::REAL_DATATYPE:
                protoVariable.set_data_type(dsbproto::model::REAL);
                break;
            case dsb::model::INTEGER_DATATYPE:
                protoVariable.set_data_type(dsbproto::model::INTEGER);
                break;
            case dsb::model::BOOLEAN_DATATYPE:
                protoVariable.set_data_type(dsbproto::model::BOOLEAN);
                break;
            case dsb::model::STRING_DATATYPE:
                protoVariable.set_data_type(dsbproto::model::STRING);
                break;
            default:
                assert (!"Unknown data type");
        }
        switch (dsbVariable.Causality()) {
            case dsb::model::PARAMETER_CAUSALITY:
                protoVariable.set_causality(dsbproto::model::PARAMETER);
                break;
            case dsb::model::CALCULATED_PARAMETER_CAUSALITY:
                protoVariable.set_causality(dsbproto::model::CALCULATED_PARAMETER);
                break;
            case dsb::model::INPUT_CAUSALITY:
                protoVariable.set_causality(dsbproto::model::INPUT);
                break;
            case dsb::model::OUTPUT_CAUSALITY:
                protoVariable.set_causality(dsbproto::model::OUTPUT);
                break;
            case dsb::model::LOCAL_CAUSALITY:
                protoVariable.set_causality(dsbproto::model::LOCAL);
                break;
            default:
                assert (!"Unknown causality");
        }
        switch (dsbVariable.Variability()) {
            case dsb::model::CONSTANT_VARIABILITY:
                protoVariable.set_variability(dsbproto::model::CONSTANT);
                break;
            case dsb::model::FIXED_VARIABILITY:
                protoVariable.set_variability(dsbproto::model::FIXED);
                break;
            case dsb::model::TUNABLE_VARIABILITY:
                protoVariable.set_variability(dsbproto::model::TUNABLE);
                break;
            case dsb::model::DISCRETE_VARIABILITY:
                protoVariable.set_variability(dsbproto::model::DISCRETE);
                break;
            case dsb::model::CONTINUOUS_VARIABILITY:
                protoVariable.set_variability(dsbproto::model::CONTINUOUS);
                break;
            default:
                assert (!"Unknown variability");
        }
    }
}


dsbproto::model::VariableDescription dsb::protocol::ToProto(
    const dsb::model::VariableDescription& dsbVariable)
{
    dsbproto::model::VariableDescription protoVariable;
    ConvertToProto_(dsbVariable, protoVariable);
    return protoVariable;
}


dsb::model::VariableDescription dsb::protocol::FromProto(
    const dsbproto::model::VariableDescription& protoVariable)
{
    dsb::model::DataType dataType = dsb::model::REAL_DATATYPE;
    switch (protoVariable.data_type()) {
        case dsbproto::model::REAL:
            dataType = dsb::model::REAL_DATATYPE;
            break;
        case dsbproto::model::INTEGER:
            dataType = dsb::model::INTEGER_DATATYPE;
            break;
        case dsbproto::model::BOOLEAN:
            dataType = dsb::model::BOOLEAN_DATATYPE;
            break;
        case dsbproto::model::STRING:
            dataType = dsb::model::STRING_DATATYPE;
            break;
        default:
            assert (!"Unknown data type");
    }
    dsb::model::Causality causality = dsb::model::LOCAL_CAUSALITY;
    switch (protoVariable.causality()) {
        case dsbproto::model::PARAMETER:
            causality = dsb::model::PARAMETER_CAUSALITY;
            break;
        case dsbproto::model::CALCULATED_PARAMETER:
            causality = dsb::model::CALCULATED_PARAMETER_CAUSALITY;
            break;
        case dsbproto::model::INPUT:
            causality = dsb::model::INPUT_CAUSALITY;
            break;
        case dsbproto::model::OUTPUT:
            causality = dsb::model::OUTPUT_CAUSALITY;
            break;
        case dsbproto::model::LOCAL:
            causality = dsb::model::LOCAL_CAUSALITY;
            break;
        default:
            assert (!"Unknown causality");
    }
    dsb::model::Variability variability = dsb::model::CONTINUOUS_VARIABILITY;
    switch (protoVariable.variability()) {
        case dsbproto::model::CONSTANT:
            variability = dsb::model::CONSTANT_VARIABILITY;
            break;
        case dsbproto::model::FIXED:
            variability = dsb::model::FIXED_VARIABILITY;
            break;
        case dsbproto::model::TUNABLE:
            variability = dsb::model::TUNABLE_VARIABILITY;
            break;
        case dsbproto::model::DISCRETE:
            variability = dsb::model::DISCRETE_VARIABILITY;
            break;
        case dsbproto::model::CONTINUOUS:
            variability = dsb::model::CONTINUOUS_VARIABILITY;
            break;
        default:
            assert (!"Unknown variability");
    }
    return dsb::model::VariableDescription(
        protoVariable.id(),
        protoVariable.name(),
        dataType,
        causality,
        variability);
}


dsbproto::model::SlaveTypeDescription dsb::protocol::ToProto(
    const dsb::model::SlaveTypeDescription& src)
{
    dsbproto::model::SlaveTypeDescription tgt;
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


dsb::model::SlaveTypeDescription dsb::protocol::FromProto(
    const dsbproto::model::SlaveTypeDescription& src)
{
    std::vector<dsb::model::VariableDescription> tgtVars;
    for (const auto& srcVar : src.variable()) tgtVars.push_back(FromProto(srcVar));
    return dsb::model::SlaveTypeDescription(
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
        explicit ScalarValueConverterVisitor(dsbproto::model::ScalarValue& value)
            : m_value(&value) { }
        void operator()(const double& value)      const { m_value->set_real_value(value); }
        void operator()(const int& value)         const { m_value->set_integer_value(value); }
        void operator()(const bool& value)        const { m_value->set_boolean_value(value); }
        void operator()(const std::string& value) const { m_value->set_string_value(value); }
    private:
        dsbproto::model::ScalarValue* m_value;
    };
}

void dsb::protocol::ConvertToProto(
    const dsb::model::ScalarValue& source,
    dsbproto::model::ScalarValue& target)
{
    target.Clear();
    boost::apply_visitor(ScalarValueConverterVisitor(target), source);
}


dsb::model::ScalarValue dsb::protocol::FromProto(
    const dsbproto::model::ScalarValue& source)
{
    if (source.has_real_value())            return source.real_value();
    else if (source.has_integer_value())    return source.integer_value();
    else if (source.has_boolean_value())    return source.boolean_value();
    else if (source.has_string_value())     return source.string_value();
    else {
        assert (!"Corrupt or empty ScalarValue protocol buffer");
        return dsb::model::ScalarValue();
    }
}


void dsb::protocol::ConvertToProto(
    const dsb::model::Variable& source,
    dsbproto::model::Variable& target)
{
    target.Clear();
    target.set_slave_id(source.Slave());
    target.set_variable_id(source.ID());
}


dsb::model::Variable dsb::protocol::FromProto(const dsbproto::model::Variable& source)
{
    return dsb::model::Variable(source.slave_id(), source.variable_id());
}


void dsb::protocol::ConvertToProto(
    const dsb::net::SlaveLocator& source,
    dsbproto::net::SlaveLocator& target)
{
    target.Clear();
    target.set_control_endpoint(source.ControlEndpoint().URL());
    target.set_data_pub_endpoint(source.DataPubEndpoint().URL());
}


dsb::net::SlaveLocator dsb::protocol::FromProto(
    const dsbproto::net::SlaveLocator& source)
{
    return dsb::net::SlaveLocator(
        dsb::net::Endpoint(source.control_endpoint()),
        dsb::net::Endpoint(source.data_pub_endpoint()));
}
