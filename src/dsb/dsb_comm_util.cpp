#include "dsb/comm/util.hpp"


namespace dsb
{
namespace comm
{


std::string dsb::comm::LastEndpoint(zmq::socket_t& socket)
{
    const size_t MAX_ENDPOINT_SIZE = 257; // including terminating zero
    char buffer[MAX_ENDPOINT_SIZE];
    size_t length = MAX_ENDPOINT_SIZE;
    socket.getsockopt(ZMQ_LAST_ENDPOINT, buffer, &length);
    assert (length > 0 && buffer[length-1] == '\0');
    return std::string(buffer, length-1);
}


}} // namespace
