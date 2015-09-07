#include "dsb/comm/util.hpp"

#include "boost/lexical_cast.hpp"


namespace dsb
{
namespace comm
{

namespace
{
    zmq::context_t globalContext;
}

zmq::context_t& GlobalContext()
{
    return globalContext;
}


std::uint16_t BindToEphemeralPort(
    zmq::socket_t& socket,
    const std::string& networkInterface)
{
    const auto endpoint = "tcp://" + networkInterface + ":*";
    socket.bind(endpoint.c_str());
    return EndpointPort(dsb::comm::LastEndpoint(socket));
}


std::string LastEndpoint(zmq::socket_t& socket)
{
    const size_t MAX_ENDPOINT_SIZE = 257; // including terminating zero
    char buffer[MAX_ENDPOINT_SIZE];
    size_t length = MAX_ENDPOINT_SIZE;
    socket.getsockopt(ZMQ_LAST_ENDPOINT, buffer, &length);
    assert (length > 0 && buffer[length-1] == '\0');
    return std::string(buffer, length-1);
}


std::uint16_t EndpointPort(const std::string& endpoint)
{
    // We expect a string on the form "tcp://addr:port", where the 'addr' and
    // 'port' substrings must both be at least one character long, and look for
    // the last colon.
    const size_t colonPos = endpoint.rfind(':');
    if (endpoint.size() < 9 || colonPos < 7 || colonPos >= endpoint.size() - 1) {
        throw std::invalid_argument("Invalid endpoint specification: " + endpoint);
    }
    return boost::lexical_cast<std::uint16_t>(endpoint.substr(colonPos + 1));
}


}} // namespace
