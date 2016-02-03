#define DSB_USE_OLD_DOMAIN_INPROC_RPC
#include "dsb/inproc_rpc.hpp"
#undef DSB_USE_OLD_DOMAIN_INPROC_RPC

#include <cassert>
#include <stdexcept>

#include "dsb/comm/messaging.hpp"
#include "dsb/protobuf.hpp"

// For DSB_USE_OLD_DOMAIN_INPROC_RPC code:
#include "boost/foreach.hpp"
#include "dsb/protocol/glue.hpp"


enum CallResult
{
    SUCCESS_CALL_RESULT,
    RUNTIME_ERROR_CALL_RESULT,
    LOGIC_ERROR_CALL_RESULT,
};


void dsb::inproc_rpc::Call(
    zmq::socket_t& socket,
    int call,
    const google::protobuf::MessageLite* args,
    google::protobuf::MessageLite* returnValue)
{
    std::vector<zmq::message_t> msg;
    msg.push_back(dsb::comm::EncodeRawDataFrame(call));
    if (args) {
        msg.push_back(zmq::message_t());
        dsb::protobuf::SerializeToFrame(*args, msg.back());
    }
    dsb::comm::Send(socket, msg);

    dsb::comm::Receive(socket, msg);
    const auto result = dsb::comm::DecodeRawDataFrame<CallResult>(msg[0]);
    if (result == SUCCESS_CALL_RESULT) {
        assert(msg.size() == (returnValue ? 2 : 1));
        if (msg.size() == 2) {
            dsb::protobuf::ParseFromFrame(msg[1], *returnValue);
        }
    } else {
        assert (msg.size() == 2);
        const auto what = dsb::comm::ToString(msg[1]);
        if (result == RUNTIME_ERROR_CALL_RESULT) {
            throw std::runtime_error(what);
        } else {
            assert(result == LOGIC_ERROR_CALL_RESULT);
            throw std::logic_error(what);
        }
    }
}


int dsb::inproc_rpc::GetCallType(const std::vector<zmq::message_t>& msg)
{
    assert(msg.size() == 1 || msg.size() == 2);
    return dsb::comm::DecodeRawDataFrame<int>(msg[0]);
}


void dsb::inproc_rpc::UnmarshalArgs(
    const std::vector<zmq::message_t>& msg,
    google::protobuf::MessageLite& args)
{
    assert(msg.size() == 2);
    dsb::protobuf::ParseFromFrame(msg[1], args);
}


void dsb::inproc_rpc::ReturnSuccess(
    zmq::socket_t& socket,
    const google::protobuf::MessageLite* returnValue)
{
    std::vector<zmq::message_t> msg;
    msg.push_back(dsb::comm::EncodeRawDataFrame(SUCCESS_CALL_RESULT));
    if (returnValue) {
        msg.push_back(zmq::message_t());
        dsb::protobuf::SerializeToFrame(*returnValue, msg.back());
    }
    dsb::comm::Send(socket, msg);
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

// =============================================================================
// Domain controller RPC stuff. To be phased out, as was done with the
// execution controller RPC.

void dsb::inproc_rpc::ReturnSuccess(
    zmq::socket_t& socket,
    std::vector<zmq::message_t>& returnValues)
{
    auto m = dsb::comm::EncodeRawDataFrame(SUCCESS_CALL_RESULT);
    socket.send(m, ZMQ_SNDMORE);
    dsb::comm::Send(socket, returnValues);
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
    void RpcCall(
        zmq::socket_t& socket,
        dsb::inproc_rpc::CallType call,
        std::vector<zmq::message_t>& msg)
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
                msg.erase(msg.begin());
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
    std::vector<zmq::message_t> msg;
    RpcCall(socket, GET_SLAVE_TYPES_CALL, msg);
    assert (!msg.empty());
    dsbproto::domain_controller::SlaveTypeList recvdSlaveTypes;
    dsb::protobuf::ParseFromFrame(msg[0], recvdSlaveTypes);

    slaveTypes.clear();
    for (const auto& st : recvdSlaveTypes.slave_type()) {
        dsb::domain::Controller::SlaveType slaveType = {
            dsb::protocol::FromProto(st.description()),
            std::vector<std::string>()
        };
        for (const auto& provider : st.provider()) {
            slaveType.providers.push_back(provider);
        }
        slaveTypes.push_back(slaveType);
    }
}

void dsb::inproc_rpc::ReturnGetSlaveTypes(
    zmq::socket_t& socket,
    dsbproto::domain_controller::SlaveTypeList& slaveTypes)
{
    auto msg = std::vector<zmq::message_t>(1);
    dsb::protobuf::SerializeToFrame(slaveTypes, msg[0]);
    ReturnSuccess(socket, msg);
}

dsb::net::SlaveLocator dsb::inproc_rpc::CallInstantiateSlave(
    zmq::socket_t& socket,
    const std::string& slaveTypeUUID,
    std::chrono::milliseconds timeout,
    const std::string& provider)
{
    std::vector<zmq::message_t> msg;
    msg.push_back(dsb::comm::ToFrame(slaveTypeUUID));
    msg.push_back(dsb::comm::EncodeRawDataFrame(timeout.count()));
    msg.push_back(dsb::comm::ToFrame(provider));
    RpcCall(socket, INSTANTIATE_SLAVE_CALL, msg);
    assert (msg.size() == 2);
    return dsb::net::SlaveLocator(
        dsb::comm::ToString(msg[0]),
        dsb::comm::ToString(msg[1]));
}

void dsb::inproc_rpc::UnmarshalInstantiateSlave(
    std::vector<zmq::message_t>& msg,
    std::string& slaveTypeUUID,
    std::chrono::milliseconds& timeout,
    std::string& provider)
{
    assert (msg.size() == 4);
    ASSERT_CALL_TYPE(msg, INSTANTIATE_SLAVE_CALL);
    slaveTypeUUID = dsb::comm::ToString(msg[1]);
    timeout = std::chrono::milliseconds(
        dsb::comm::DecodeRawDataFrame<std::chrono::milliseconds::rep>(msg[2])),
    provider = dsb::comm::ToString(msg[3]);
}

void dsb::inproc_rpc::ReturnInstantiateSlave(
    zmq::socket_t& socket,
    const dsb::net::SlaveLocator& slaveLocator)
{
    std::vector<zmq::message_t> msg;
    msg.push_back(dsb::comm::ToFrame(slaveLocator.Endpoint()));
    msg.push_back(dsb::comm::ToFrame(slaveLocator.Identity()));
    ReturnSuccess(socket, msg);
}
