#ifndef DSB_P2P_PROXY_HPP
#define DSB_P2P_PROXY_HPP

#include <memory>
#include <string>
#include "zmq.hpp"


namespace dsb
{
namespace domain_broker
{


zmq::socket_t SpawnP2PProxy(
    std::shared_ptr<zmq::context_t> context,
    const std::string& endpoint1,
    const std::string& endpoint2);


}}      // namespace
#endif  // header guard