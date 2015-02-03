#include "dsb/protocol/glue.hpp"

#include <cassert>


// TODO: Use a lookup table or something in the two following functions?
// (A job for the "X macro"?  http://www.drdobbs.com/cpp/the-x-macro/228700289)

dsbproto::variable::VariableDefinition dsb::protocol::ToProto(
    const dsb::model::Variable& dsbVariable)
{
    dsbproto::variable::VariableDefinition protoVariable;
    protoVariable.set_id(dsbVariable.ID());
    protoVariable.set_name(dsbVariable.Name());
    switch (dsbVariable.DataType()) {
        case dsb::model::REAL_DATATYPE:
            protoVariable.set_data_type(dsbproto::variable::REAL);
            break;
        case dsb::model::INTEGER_DATATYPE:
            protoVariable.set_data_type(dsbproto::variable::INTEGER);
            break;
        case dsb::model::BOOLEAN_DATATYPE:
            protoVariable.set_data_type(dsbproto::variable::BOOLEAN);
            break;
        case dsb::model::STRING_DATATYPE:
            protoVariable.set_data_type(dsbproto::variable::STRING);
            break;
        default:
            assert (!"Unknown data type");
    }
    switch (dsbVariable.Causality()) {
        case dsb::model::PARAMETER_CAUSALITY:
            protoVariable.set_causality(dsbproto::variable::PARAMETER);
            break;
        case dsb::model::CALCULATED_PARAMETER_CAUSALITY:
            protoVariable.set_causality(dsbproto::variable::CALCULATED_PARAMETER);
            break;
        case dsb::model::INPUT_CAUSALITY:
            protoVariable.set_causality(dsbproto::variable::INPUT);
            break;
        case dsb::model::OUTPUT_CAUSALITY:
            protoVariable.set_causality(dsbproto::variable::OUTPUT);
            break;
        case dsb::model::LOCAL_CAUSALITY:
            protoVariable.set_causality(dsbproto::variable::LOCAL);
            break;
        default:
            assert (!"Unknown causality");
    }
    switch (dsbVariable.Variability()) {
        case dsb::model::CONSTANT_VARIABILITY:
            protoVariable.set_variability(dsbproto::variable::CONSTANT);
            break;
        case dsb::model::FIXED_VARIABILITY:
            protoVariable.set_variability(dsbproto::variable::FIXED);
            break;
        case dsb::model::TUNABLE_VARIABILITY:
            protoVariable.set_variability(dsbproto::variable::TUNABLE);
            break;
        case dsb::model::DISCRETE_VARIABILITY:
            protoVariable.set_variability(dsbproto::variable::DISCRETE);
            break;
        case dsb::model::CONTINUOUS_VARIABILITY:
            protoVariable.set_variability(dsbproto::variable::CONTINUOUS);
            break;
        default:
            assert (!"Unknown variability");
    }
    return protoVariable;
}


dsb::model::Variable dsb::protocol::FromProto(
    const dsbproto::variable::VariableDefinition& protoVariable)
{
    dsb::model::DataType dataType;
    switch (protoVariable.data_type()) {
        case dsbproto::variable::REAL:
            dataType = dsb::model::REAL_DATATYPE;
            break;
        case dsbproto::variable::INTEGER:
            dataType = dsb::model::INTEGER_DATATYPE;
            break;
        case dsbproto::variable::BOOLEAN:
            dataType = dsb::model::BOOLEAN_DATATYPE;
            break;
        case dsbproto::variable::STRING:
            dataType = dsb::model::STRING_DATATYPE;
            break;
        default:
            assert (!"Unknown data type");
    }
    dsb::model::Causality causality;
    switch (protoVariable.causality()) {
        case dsbproto::variable::PARAMETER:
            causality = dsb::model::PARAMETER_CAUSALITY;
            break;
        case dsbproto::variable::CALCULATED_PARAMETER:
            causality = dsb::model::CALCULATED_PARAMETER_CAUSALITY;
            break;
        case dsbproto::variable::INPUT:
            causality = dsb::model::INPUT_CAUSALITY;
            break;
        case dsbproto::variable::OUTPUT:
            causality = dsb::model::OUTPUT_CAUSALITY;
            break;
        case dsbproto::variable::LOCAL:
            causality = dsb::model::LOCAL_CAUSALITY;
            break;
        default:
            assert (!"Unknown causality");
    }
    dsb::model::Variability variability;
    switch (protoVariable.variability()) {
        case dsbproto::variable::CONSTANT:
            variability = dsb::model::CONSTANT_VARIABILITY;
            break;
        case dsbproto::variable::FIXED:
            variability = dsb::model::FIXED_VARIABILITY;
            break;
        case dsbproto::variable::TUNABLE:
            variability = dsb::model::TUNABLE_VARIABILITY;
            break;
        case dsbproto::variable::DISCRETE:
            variability = dsb::model::DISCRETE_VARIABILITY;
            break;
        case dsbproto::variable::CONTINUOUS:
            variability = dsb::model::CONTINUOUS_VARIABILITY;
            break;
        default:
            assert (!"Unknown variability");
    }
    return dsb::model::Variable(
        protoVariable.id(),
        protoVariable.name(),
        dataType,
        causality,
        variability);
}



dsbproto::domain::ExecutionLocator dsb::protocol::ToProto(
    const dsb::execution::Locator& executionLocator)
{
    dsbproto::domain::ExecutionLocator el;
    el.set_master_endpoint(executionLocator.MasterEndpoint());
    el.set_slave_endpoint(executionLocator.SlaveEndpoint());
    el.set_variable_pub_endpoint(executionLocator.VariablePubEndpoint());
    el.set_variable_sub_endpoint(executionLocator.VariableSubEndpoint());
    el.set_execution_name(executionLocator.ExecName());
    el.set_comm_timeout_seconds(executionLocator.CommTimeout().count());
    return el;
}


dsb::execution::Locator dsb::protocol::FromProto(
    const dsbproto::domain::ExecutionLocator& executionLocator)
{
    return dsb::execution::Locator(
        executionLocator.master_endpoint(),
        executionLocator.slave_endpoint(),
        executionLocator.variable_pub_endpoint(),
        executionLocator.variable_sub_endpoint(),
        "",
        executionLocator.execution_name(),
        boost::chrono::seconds(executionLocator.comm_timeout_seconds()));
}
