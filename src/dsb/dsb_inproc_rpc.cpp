#include "dsb/inproc_rpc.hpp"

#include "boost/foreach.hpp"
#include "dsb/comm.hpp"
#include "dsb/protocol/glue.hpp"
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


void dsb::inproc_rpc::CallGetSlaveTypes(
    zmq::socket_t& socket,
    std::vector<dsb::domain::Controller::SlaveType>& slaveTypes)
{
    std::deque<zmq::message_t> msg;
    Call(socket, GET_SLAVE_TYPES_CALL, msg);
    assert (!msg.empty());
    dsbproto::inproc_rpc::SlaveTypeList recvdSlaveTypes;
    dsb::protobuf::ParseFromFrame(msg[0], recvdSlaveTypes);

    slaveTypes.clear();
    BOOST_FOREACH(const auto& st, recvdSlaveTypes.slave_type()) {
        dsb::domain::Controller::SlaveType slaveType;
        const auto& sti = st.slave_type_info();
        slaveType.name = sti.name();
        slaveType.uuid = sti.uuid();
        slaveType.description = sti.description();
        slaveType.author = sti.author();
        slaveType.version = sti.version();
        BOOST_FOREACH(const auto& var, sti.variable()) {
            slaveType.variables.push_back(dsb::protocol::FromProto(var));
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

void dsb::inproc_rpc::CallInstantiateSlave(
    zmq::socket_t& socket,
    const std::string& slaveTypeUUID,
    const dsb::execution::Locator& executionLocator,
    dsb::model::SlaveID slaveID,
    const std::string& provider)
{
    std::deque<zmq::message_t> args;
    args.push_back(dsb::comm::ToFrame(slaveTypeUUID));
    args.push_back(dsb::comm::ToFrame(executionLocator.MasterEndpoint()));
    args.push_back(dsb::comm::ToFrame(executionLocator.SlaveEndpoint()));
    args.push_back(dsb::comm::ToFrame(executionLocator.VariablePubEndpoint()));
    args.push_back(dsb::comm::ToFrame(executionLocator.VariableSubEndpoint()));
    args.push_back(dsb::comm::ToFrame(executionLocator.ExecName()));
    args.push_back(dsb::comm::EncodeRawDataFrame(slaveID));
    args.push_back(dsb::comm::ToFrame(provider));
    Call(socket, INSTANTIATE_SLAVE_CALL, args);
}

void dsb::inproc_rpc::UnmarshalInstantiateSlave(
    std::deque<zmq::message_t>& msg,
    std::string& slaveTypeUUID,
    dsb::execution::Locator& executionLocator,
    dsb::model::SlaveID& slaveID,
    std::string& provider)
{
    assert (msg.size() == 9);
    ASSERT_CALL_TYPE(msg, INSTANTIATE_SLAVE_CALL);
    slaveTypeUUID = dsb::comm::ToString(msg[1]);
    executionLocator = dsb::execution::Locator(
        dsb::comm::ToString(msg[2]),
        dsb::comm::ToString(msg[3]),
        dsb::comm::ToString(msg[4]),
        dsb::comm::ToString(msg[5]),
        "",
        dsb::comm::ToString(msg[6]));
    slaveID = dsb::comm::DecodeRawDataFrame<dsb::model::SlaveID>(msg[7]);
    provider = dsb::comm::ToString(msg[8]);
}

void dsb::inproc_rpc::CallSetSimulationTime(
    zmq::socket_t& socket,
    dsb::model::TimePoint startTime,
    dsb::model::TimePoint stopTime)
{
    std::deque<zmq::message_t> args;
    args.push_back(dsb::comm::EncodeRawDataFrame(startTime));
    args.push_back(dsb::comm::EncodeRawDataFrame(stopTime));
    Call(socket, SET_SIMULATION_TIME_CALL, args);
}


void dsb::inproc_rpc::UnmarshalSetSimulationTime(
    const std::deque<zmq::message_t>& msg,
    dsb::model::TimePoint& startTime,
    dsb::model::TimePoint& stopTime)
{
    assert (msg.size() == 3);
    ASSERT_CALL_TYPE(msg, SET_SIMULATION_TIME_CALL);
    startTime = dsb::comm::DecodeRawDataFrame<dsb::model::TimePoint>(msg[1]);
    stopTime  = dsb::comm::DecodeRawDataFrame<dsb::model::TimePoint>(msg[2]);
}


void dsb::inproc_rpc::CallAddSlave(zmq::socket_t& socket, dsb::model::SlaveID slaveId)
{
    std::deque<zmq::message_t> args;
    args.push_back(dsb::comm::EncodeRawDataFrame(slaveId));
    Call(socket, ADD_SLAVE_CALL, args);
}


void dsb::inproc_rpc::UnmarshalAddSlave(
    const std::deque<zmq::message_t>& msg,
    dsb::model::SlaveID& slaveId)
{
    assert (msg.size() == 2);
    ASSERT_CALL_TYPE(msg, ADD_SLAVE_CALL);
    slaveId = dsb::comm::DecodeRawDataFrame<dsb::model::SlaveID>(msg[1]);
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
        const dsb::model::ScalarValue& source,
        dsbproto::variable::ScalarValue& target)
    {
        target.Clear();
        boost::apply_visitor(ScalarValueConverterVisitor(target), source);
    }
}


void dsb::inproc_rpc::CallSetVariables(
    zmq::socket_t& socket,
    dsb::model::SlaveID slaveId,
    const std::vector<dsb::model::VariableValue>& variables)
{
    std::deque<zmq::message_t> args;
    args.push_back(dsb::comm::EncodeRawDataFrame(slaveId));

    dsbproto::execution::SetVarsData setVarsData;
    BOOST_FOREACH (const auto v, variables) {
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
    dsb::model::SlaveID& slaveId,
    dsbproto::execution::SetVarsData& setVarsData)
{
    assert (msg.size() == 3);
    ASSERT_CALL_TYPE(msg, SET_VARIABLES_CALL);
    slaveId = dsb::comm::DecodeRawDataFrame<dsb::model::SlaveID>(msg[1]);
    dsb::protobuf::ParseFromFrame(msg[2], setVarsData);
}


void dsb::inproc_rpc::CallConnectVariables(
    zmq::socket_t& socket,
    dsb::model::SlaveID slaveId,
    const std::vector<dsb::model::VariableConnection>& variables)
{
    std::deque<zmq::message_t> args;
    args.push_back(dsb::comm::EncodeRawDataFrame(slaveId));
    BOOST_FOREACH (const auto v, variables) {
        args.push_back(dsb::comm::EncodeRawDataFrame(v.inputId));
        args.push_back(dsb::comm::EncodeRawDataFrame(v.otherSlaveId));
        args.push_back(dsb::comm::EncodeRawDataFrame(v.otherOutputId));
    }
    Call(socket, CONNECT_VARIABLES_CALL, args);
}


void dsb::inproc_rpc::UnmarshalConnectVariables(
    const std::deque<zmq::message_t>& msg,
    dsb::model::SlaveID& slaveId,
    dsbproto::execution::ConnectVarsData& connectVarsData)
{
    assert (msg.size() >= 2 && (msg.size()-2) % 3 == 0);
    ASSERT_CALL_TYPE(msg, CONNECT_VARIABLES_CALL);
    slaveId = dsb::comm::DecodeRawDataFrame<dsb::model::SlaveID>(msg[1]);
    for (size_t i = 2; i < msg.size(); i+=3) {
        auto& newVar = *connectVarsData.add_connection();
        newVar.set_input_var_id(
            dsb::comm::DecodeRawDataFrame<dsb::model::VariableID>(msg[i]));
        newVar.mutable_output_var()->set_slave_id(
            dsb::comm::DecodeRawDataFrame<dsb::model::SlaveID>(msg[i+1]));
        newVar.mutable_output_var()->set_var_id(
            dsb::comm::DecodeRawDataFrame<dsb::model::VariableID>(msg[i+2]));
    }
}


void dsb::inproc_rpc::CallWaitForReady(zmq::socket_t& socket)
{
    std::deque<zmq::message_t> dummy;
    Call(socket, WAIT_FOR_READY_CALL, dummy);
}


void dsb::inproc_rpc::CallStep(
    zmq::socket_t& socket,
    dsb::model::TimePoint t,
    dsb::model::TimeDuration dt)
{
    std::deque<zmq::message_t> args;
    args.push_back(dsb::comm::EncodeRawDataFrame(t));
    args.push_back(dsb::comm::EncodeRawDataFrame(dt));
    Call(socket, STEP_CALL, args);
}


void dsb::inproc_rpc::UnmarshalStep(
    const std::deque<zmq::message_t>& msg,
    dsbproto::execution::StepData& stepData)
{
    assert (msg.size() == 3);
    ASSERT_CALL_TYPE(msg, STEP_CALL);
    stepData.set_timepoint(dsb::comm::DecodeRawDataFrame<dsb::model::TimePoint>(msg[1]));
    stepData.set_stepsize(dsb::comm::DecodeRawDataFrame<dsb::model::TimeDuration>(msg[2]));
}


void dsb::inproc_rpc::CallTerminate(zmq::socket_t& socket)
{
    std::deque<zmq::message_t> dummy;
    Call(socket, TERMINATE_CALL, dummy);
}
