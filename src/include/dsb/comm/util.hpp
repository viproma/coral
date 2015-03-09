/**
\file
\brief Misc. utility functions for communication-related purposes.
*/
#ifndef DSB_COMM_UTIL_HPP
#define DSB_COMM_UTIL_HPP

#include <string>
#include "zmq.hpp"


namespace dsb
{
namespace comm
{


/**
\brief Returns the value of the ZMQ_LAST_ENDPOINT socket property.
\throws zmq::error_t if ZMQ reports an error.
*/
std::string LastEndpoint(zmq::socket_t& socket);


}}      // namespace
#endif  // header guard
