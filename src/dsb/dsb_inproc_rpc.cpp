#include "dsb/inproc_rpc.hpp"

#include "dsb/comm.hpp"
#include "dsb/protobuf.hpp"


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
    dsb::sequence::Sequence<dsb::types::Variable&> variables)
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
    dsb::sequence::Sequence<dsb::types::VariableConnection&> variables)
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
