#include "dsb/inproc_rpc.hpp"

#include <cassert>
#include <stdexcept>

#include "dsb/comm/messaging.hpp"
#include "dsb/protobuf.hpp"


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
