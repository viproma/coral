#include "p2p_proxy.hpp"

#include <algorithm>
#include <deque>
#include <utility>

#include "boost/lexical_cast.hpp"
#include "boost/thread.hpp"
#include "dsb/comm.hpp"
#include "dsb/config.h"
#include "dsb/util.hpp"


namespace dd = dsb::domain_broker;


std::uint16_t dd::BindToEphemeralPort(
    zmq::socket_t& socket,
    const std::string& networkInterface)
{
    const auto endpoint = "tcp://" + networkInterface + ":*";
    socket.bind(endpoint.c_str());
    return EndpointPort(dsb::comm::LastEndpoint(socket));
}


std::uint16_t dd::EndpointPort(const std::string& endpoint)
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


namespace
{
    void SwapEnvelopes(std::deque<zmq::message_t>& msg)
    {
        // TODO: This is currently somewhat limited in that it can only deal
        // with single-identity envelopes.
        if (msg.size() >= 4 && msg[1].size() == 0 && msg[3].size() == 0) {
            std::swap(msg[0], msg[2]);
        } else assert (!"Received a message with the wrong envelope format");
    }

    void SwapEnvelopesAndTransfer(zmq::socket_t& src, zmq::socket_t& tgt)
    {
        std::deque<zmq::message_t> msg;
        dsb::comm::Receive(src, msg);
        SwapEnvelopes(msg);
        dsb::comm::Send(tgt, msg);
    }


    class ProxyFunctor
    {
    public:
        ProxyFunctor(
            std::shared_ptr<zmq::context_t> context,
            zmq::socket_t&& socket,
            zmq::socket_t&& killSocket)
            : m_context(context),
              m_socket(std::move(socket)),
              m_killSocket(std::move(killSocket))
        {
        }

        ProxyFunctor(ProxyFunctor&& other) DSB_NOEXCEPT
            : m_context(std::move(other.m_context)),
              m_socket(std::move(other.m_socket)),
              m_killSocket(std::move(other.m_killSocket))
        {
        }

        void operator()()
        {
            const size_t SOCKET_COUNT = 2;
            zmq::pollitem_t pollItems[SOCKET_COUNT] = {
                { m_killSocket, 0, ZMQ_POLLIN, 0 },
                { m_socket,     0, ZMQ_POLLIN, 0 }
            };
            for (;;) {
                zmq::poll(pollItems, SOCKET_COUNT);
                if (pollItems[0].revents & ZMQ_POLLIN) {
                    m_killSocket.recv((char*) nullptr, 0);
                    return;
                }
                if (pollItems[1].revents & ZMQ_POLLIN) {
                    SwapEnvelopesAndTransfer(m_socket, m_socket);
                }
            }

        }

    private:
        std::shared_ptr<zmq::context_t> m_context;
        zmq::socket_t m_socket;
        zmq::socket_t m_killSocket;
    };
}



zmq::socket_t dd::SpawnP2PProxy(
    std::shared_ptr<zmq::context_t> context,
    const std::string& networkInterface,
    std::uint16_t& ephemeralPort)
{
    auto killSocketLocal = zmq::socket_t(*context, ZMQ_PAIR);
    auto killSocketRemote = zmq::socket_t(*context, ZMQ_PAIR);
    const auto killEndpoint = "inproc://" + dsb::util::RandomUUID();
    killSocketLocal.bind(killEndpoint.c_str());
    killSocketRemote.connect(killEndpoint.c_str());

    auto socket = zmq::socket_t(*context, ZMQ_ROUTER);
    const auto ep = BindToEphemeralPort(socket, networkInterface);

    boost::thread(ProxyFunctor(
        context,
        std::move(socket),
        std::move(killSocketRemote)));

    ephemeralPort = ep;
    return killSocketLocal;
}
