#include "dsb/inproc_rpc.hpp"

#include "boost/foreach.hpp"
#include "dsb/comm.hpp"
#include "dsb/protobuf.hpp"
#include "variable.pb.h"


enum CallResult
{
    SUCCESS_CALL_RESULT,
    LOGIC_ERROR_CALL_RESULT,
    RUNTIME_ERROR_CALL_RESULT
};


void dsb::inproc_rpc::ReturnSuccess(zmq::socket_t& socket)
{
    auto m = dsb::comm::EncodeRawDataFrame(SUCCESS_CALL_RESULT);
    socket.send(m);
}


void dsb::inproc_rpc::ReturnSuccess(
    zmq::socket_t& socket,
    std::deque<zmq::message_t>& returnValues)
{
    auto m = dsb::comm::EncodeRawDataFrame(SUCCESS_CALL_RESULT);
    socket.send(m, ZMQ_SNDMORE);
    dsb::comm::Send(socket, returnValues);
}


void dsb::inproc_rpc::ThrowLogicError(
    zmq::socket_t& socket,
    const std::string& what)
{
    auto m0 = dsb::comm::EncodeRawDataFrame(LOGIC_ERROR_CALL_RESULT);
    auto m1 = dsb::comm::ToFrame(what);
    socket.send(m0, ZMQ_SNDMORE);
    socket.send(m1);
}


void dsb::inproc_rpc::ThrowRuntimeError(
    zmq::socket_t& socket,
    const std::string& what)
{
    auto m0 = dsb::comm::EncodeRawDataFrame(RUNTIME_ERROR_CALL_RESULT);
    auto m1 = dsb::comm::ToFrame(what);
    socket.send(m0, ZMQ_SNDMORE);
    socket.send(m1);
}


namespace
{
    /*
    Performs an "RPC call".

    On entry, `msg` must contain any arguments to the call, encoded as ZMQ
    message frames.  These must be decoded by the corresponding Unmarshal()
    function.  On successful return, `msg` contains information returned by the
    call, if any.  The function throws std::runtime_error or std::logic_error
    if the call fails.
    */
    void Call(
        zmq::socket_t& socket,
        dsb::inproc_rpc::CallType call,
        std::deque<zmq::message_t>& msg)
    {
        auto headerFrame = dsb::comm::EncodeRawDataFrame(call);
        if (msg.empty()) {
            socket.send(headerFrame);
        } else {
            socket.send(headerFrame, ZMQ_SNDMORE);
            dsb::comm::Send(socket, msg);
        }

        dsb::comm::Receive(socket, msg);
        assert (!msg.empty());
        const auto result = dsb::comm::DecodeRawDataFrame<CallResult>(msg[0]);
        switch (result) {
            case SUCCESS_CALL_RESULT:
                msg.pop_front();
                return;
            case LOGIC_ERROR_CALL_RESULT:
                assert (msg.size() == 2);
                throw std::logic_error(dsb::comm::ToString(msg[1]));
            case RUNTIME_ERROR_CALL_RESULT:
                assert (msg.size() == 2);
                throw std::runtime_error(dsb::comm::ToString(msg[1]));
            default:
                assert (!"Invalid RPC result");
        }
    }
}

#define ASSERT_CALL_TYPE(msg, callType) \
    assert (dsb::comm::DecodeRawDataFrame<dsb::inproc_rpc::CallType>(msg[0]) == callType);


namespace
{
    // TODO: Use a lookup table or something here?  (Is this a job for the
    // "X macro"?  http://www.drdobbs.com/cpp/the-x-macro/228700289
    dsb::model::Variable ProtoToDsb(
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
}

void dsb::inproc_rpc::CallGetSlaveTypes(
    zmq::socket_t& socket,
    std::vector<dsb::types::SlaveType>& slaveTypes)
{
    std::deque<zmq::message_t> msg;
    Call(socket, GET_SLAVE_TYPES, msg);
    assert (!msg.empty());
    dsbproto::inproc_rpc::SlaveTypeList recvdSlaveTypes;
    dsb::protobuf::ParseFromFrame(msg[0], recvdSlaveTypes);

    slaveTypes.clear();
    BOOST_FOREACH(const auto& st, recvdSlaveTypes.slave_type()) {
        dsb::types::SlaveType slaveType;
        const auto& sti = st.slave_type_info();
        slaveType.name = sti.name();
        slaveType.uuid = sti.uuid();
        slaveType.description = sti.description();
        slaveType.author = sti.author();
        slaveType.version = sti.version();
        BOOST_FOREACH(const auto& var, sti.variable()) {
            slaveType.variables.push_back(ProtoToDsb(var));
        }
        BOOST_FOREACH(const auto& provider, st.provider()) {
            slaveType.providers.push_back(provider);
        }
        slaveTypes.push_back(slaveType);
    }
}

void dsb::inproc_rpc::ReturnGetSlaveTypes(
    zmq::socket_t& socket,
    dsbproto::inproc_rpc::SlaveTypeList& slaveTypes)
{
    auto msg = std::deque<zmq::message_t>(1);
    dsb::protobuf::SerializeToFrame(slaveTypes, msg[0]);
    ReturnSuccess(socket, msg);
}


void dsb::inproc_rpc::CallSetSimulationTime(
    zmq::socket_t& socket,
    double startTime,
    double stopTime)
{
    std::deque<zmq::message_t> args;
    args.push_back(dsb::comm::EncodeRawDataFrame(startTime));
    args.push_back(dsb::comm::EncodeRawDataFrame(stopTime));
    Call(socket, SET_SIMULATION_TIME_CALL, args);
}


void dsb::inproc_rpc::UnmarshalSetSimulationTime(
    const std::deque<zmq::message_t>& msg,
    double& startTime,
    double& stopTime)
{
    assert (msg.size() == 3);
    ASSERT_CALL_TYPE(msg, SET_SIMULATION_TIME_CALL);
    startTime = dsb::comm::DecodeRawDataFrame<double>(msg[1]);
    stopTime  = dsb::comm::DecodeRawDataFrame<double>(msg[2]);
}


void dsb::inproc_rpc::CallAddSlave(zmq::socket_t& socket, uint16_t slaveId)
{
    std::deque<zmq::message_t> args;
    args.push_back(dsb::comm::EncodeRawDataFrame(slaveId));
    Call(socket, ADD_SLAVE_CALL, args);
}


void dsb::inproc_rpc::UnmarshalAddSlave(
    const std::deque<zmq::message_t>& msg,
    uint16_t& slaveId)
{
    assert (msg.size() == 2);
    ASSERT_CALL_TYPE(msg, ADD_SLAVE_CALL);
    slaveId = dsb::comm::DecodeRawDataFrame<uint16_t>(msg[1]);
}


namespace
{
    class ScalarValueConverterVisitor : public boost::static_visitor<>
    {
    public:
        ScalarValueConverterVisitor(dsbproto::variable::ScalarValue& value)
            : m_value(&value) { }
        void operator()(const double& value)      const { m_value->set_real_value(value); }
        void operator()(const int& value)         const { m_value->set_integer_value(value); }
        void operator()(const bool& value)        const { m_value->set_boolean_value(value); }
        void operator()(const std::string& value) const { m_value->set_string_value(value); }
    private:
        dsbproto::variable::ScalarValue* m_value;
    };

    void ConvertScalarValue(
        const dsb::types::ScalarValue& source,
        dsbproto::variable::ScalarValue& target)
    {
        target.Clear();
        boost::apply_visitor(ScalarValueConverterVisitor(target), source);
    }
}


void dsb::inproc_rpc::CallSetVariables(
    zmq::socket_t& socket,
    uint16_t slaveId,
    dsb::sequence::Sequence<dsb::types::Variable> variables)
{
    std::deque<zmq::message_t> args;
    args.push_back(dsb::comm::EncodeRawDataFrame(slaveId));

    dsbproto::control::SetVarsData setVarsData;
    while (!variables.Empty()) {
        const auto v = variables.Next();
        auto& newVar = *setVarsData.add_variable();
        newVar.set_id(v.id);
        ConvertScalarValue(v.value, *newVar.mutable_value());
    }
    args.push_back(zmq::message_t());
    dsb::protobuf::SerializeToFrame(setVarsData, args.back());

    Call(socket, SET_VARIABLES_CALL, args);
}


void dsb::inproc_rpc::UnmarshalSetVariables(
    const std::deque<zmq::message_t>& msg,
    uint16_t& slaveId,
    dsbproto::control::SetVarsData& setVarsData)
{
    assert (msg.size() == 3);
    ASSERT_CALL_TYPE(msg, SET_VARIABLES_CALL);
    slaveId = dsb::comm::DecodeRawDataFrame<uint16_t>(msg[1]);
    dsb::protobuf::ParseFromFrame(msg[2], setVarsData);
}


void dsb::inproc_rpc::CallConnectVariables(
    zmq::socket_t& socket,
    uint16_t slaveId,
    dsb::sequence::Sequence<dsb::types::VariableConnection> variables)
{
    std::deque<zmq::message_t> args;
    args.push_back(dsb::comm::EncodeRawDataFrame(slaveId));
    while (!variables.Empty()) {
        const auto v = variables.Next();
        args.push_back(dsb::comm::EncodeRawDataFrame(v.inputId));
        args.push_back(dsb::comm::EncodeRawDataFrame(v.otherSlaveId));
        args.push_back(dsb::comm::EncodeRawDataFrame(v.otherOutputId));
    }
    Call(socket, CONNECT_VARIABLES_CALL, args);
}


void dsb::inproc_rpc::UnmarshalConnectVariables(
    const std::deque<zmq::message_t>& msg,
    uint16_t& slaveId,
    dsbproto::control::ConnectVarsData& connectVarsData)
{
    assert (msg.size() >= 2 && (msg.size()-2) % 3 == 0);
    ASSERT_CALL_TYPE(msg, CONNECT_VARIABLES_CALL);
    slaveId = dsb::comm::DecodeRawDataFrame<uint16_t>(msg[1]);
    for (size_t i = 2; i < msg.size(); i+=3) {
        auto& newVar = *connectVarsData.add_connection();
        newVar.set_input_var_id(dsb::comm::DecodeRawDataFrame<uint16_t>(msg[i]));
        newVar.mutable_output_var()->set_slave_id(
            dsb::comm::DecodeRawDataFrame<uint16_t>(msg[i+1]));
        newVar.mutable_output_var()->set_var_id(
            dsb::comm::DecodeRawDataFrame<uint16_t>(msg[i+2]));
    }
}


void dsb::inproc_rpc::CallWaitForReady(zmq::socket_t& socket)
{
    Call(socket, WAIT_FOR_READY_CALL, std::deque<zmq::message_t>());
}


void dsb::inproc_rpc::CallStep(zmq::socket_t& socket, double t, double dt)
{
    std::deque<zmq::message_t> args;
    args.push_back(dsb::comm::EncodeRawDataFrame(t));
    args.push_back(dsb::comm::EncodeRawDataFrame(dt));
    Call(socket, STEP_CALL, args);
}


void dsb::inproc_rpc::UnmarshalStep(
    const std::deque<zmq::message_t>& msg,
    dsbproto::control::StepData& stepData)
{
    assert (msg.size() == 3);
    ASSERT_CALL_TYPE(msg, STEP_CALL);
    stepData.set_timepoint(dsb::comm::DecodeRawDataFrame<double>(msg[1]));
    stepData.set_stepsize(dsb::comm::DecodeRawDataFrame<double>(msg[2]));
}


void dsb::inproc_rpc::CallTerminate(zmq::socket_t& socket)
{
    Call(socket, TERMINATE_CALL, std::deque<zmq::message_t>());
}
