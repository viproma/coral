#include "p2p_proxy.hpp"

#include <algorithm>
#include <deque>
#include <utility>

#include "boost/thread.hpp"
#include "dsb/comm.hpp"
#include "dsb/config.h"
#include "dsb/util.hpp"


namespace dd = dsb::domain_broker;


namespace
{
    void SwapEnvelopes(std::deque<zmq::message_t>& msg)
    {
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
            zmq::socket_t&& socket1,
            zmq::socket_t&& socket2,
            zmq::socket_t&& killSocket)
            : m_context(context),
              m_socket1(std::move(socket1)),
              m_socket2(std::move(socket2)),
              m_killSocket(std::move(killSocket))
        {
        }

        ProxyFunctor(ProxyFunctor&& other) DSB_NOEXCEPT
            : m_context(std::move(other.m_context)),
              m_socket1(std::move(other.m_socket1)),
              m_socket2(std::move(other.m_socket2)),
              m_killSocket(std::move(other.m_killSocket))
        {
        }

        void operator()()
        {
            const size_t SOCKET_COUNT = 3;
            zmq::pollitem_t pollItems[SOCKET_COUNT] = {
                { m_killSocket, 0, ZMQ_POLLIN, 0 },
                { m_socket1,    0, ZMQ_POLLIN, 0 },
                { m_socket2,    0, ZMQ_POLLIN, 0 }
            };
            for (;;) {
                zmq::poll(pollItems, SOCKET_COUNT);
                if (pollItems[0].revents & ZMQ_POLLIN) {
                    m_killSocket.recv((char*) nullptr, 0);
                    return;
                }
                if (pollItems[1].revents & ZMQ_POLLIN) {
                    SwapEnvelopesAndTransfer(m_socket1, m_socket2);
                }
                if (pollItems[2].revents & ZMQ_POLLIN) {
                    SwapEnvelopesAndTransfer(m_socket2, m_socket1);
                }
            }

        }

    private:
        std::shared_ptr<zmq::context_t> m_context;
        zmq::socket_t m_socket1;
        zmq::socket_t m_socket2;
        zmq::socket_t m_killSocket;
    };
}



zmq::socket_t dd::SpawnP2PProxy(
    std::shared_ptr<zmq::context_t> context,
    const std::string& endpoint1,
    const std::string& endpoint2)
{
    auto killSocketLocal = zmq::socket_t(*context, ZMQ_PAIR);
    auto killSocketRemote = zmq::socket_t(*context, ZMQ_PAIR);
    const auto killEndpoint = "inproc://" + dsb::util::RandomUUID();
    killSocketLocal.bind(killEndpoint.c_str());
    killSocketRemote.connect(killEndpoint.c_str());

    auto socket1 = zmq::socket_t(*context, ZMQ_ROUTER);
    socket1.bind(endpoint1.c_str());
    auto socket2 = zmq::socket_t(*context, ZMQ_ROUTER);
    socket2.bind(endpoint2.c_str());

    boost::thread(ProxyFunctor(
        context,
        std::move(socket1),
        std::move(socket2),
        std::move(killSocketRemote)));
    return killSocketLocal;
}
