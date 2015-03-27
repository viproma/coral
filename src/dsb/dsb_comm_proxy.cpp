#include "dsb/comm/proxy.hpp"

#include <exception>
#include <utility>

#include "boost/numeric/conversion/cast.hpp"
#include "boost/thread.hpp"

#include "dsb/comm/util.hpp"
#include "dsb/util.hpp"


// Misc. helper functions and classes
namespace
{
    /*
    \brief  Receives a multipart message from the `source` socket and sends
            it using the `target` socket.

    The function will block until an entire message has been transferred.
    */
    void TransferMessage(zmq::socket_t& source, zmq::socket_t& target)
    {
        zmq::message_t msg;
        bool more = true;
        do {
            source.recv(&msg);
            more = msg.more();
            target.send(msg, more ? ZMQ_SNDMORE : 0);
        } while (more);
    }

    /// Single-value container with move-on-copy semantics.
    template<typename T>
    class Unique
    {
    public:
        /// Moves `value` into the newly constructed container.
        explicit Unique(T&& value) : m_payload(std::move(value)) { }

        /**
        \brief  Copy constructor which moves the payload away from the source
                container and into the target container.
        */
        Unique(Unique& source) DSB_NOEXCEPT : m_payload(std::move(source.m_payload)) { }

        /// Move constructor.
        Unique(Unique&& source) DSB_NOEXCEPT : m_payload(std::move(source.m_payload)) { }

        /// Returns a reference to the payload value.
        T& Payload() { return m_payload; }

    private:
        T m_payload;
    };

    /// The functor that performs the actual proxying for SpawnProxy().
    class ProxyFunctor
    {
    public:
        ProxyFunctor(
            Unique<zmq::socket_t> socket1,
            Unique<zmq::socket_t> socket2,
            const std::string& controlEndpoint,
            long silenceTimeoutMillis)
            : m_socket1(socket1),
              m_socket2(socket2),
              // std::string was never designed for thread safety, and may be
              // implemented with COW, so we trick it into always making a copy.
              m_controlEndpoint(controlEndpoint.begin(), controlEndpoint.end()),
              m_silenceTimeoutMillis(silenceTimeoutMillis)
        {
        }

        void operator()()
        {
            assert ((void*) m_socket1.Payload());
            assert ((void*) m_socket2.Payload());
            assert (!m_controlEndpoint.empty());

            auto controlSocket = zmq::socket_t(dsb::comm::GlobalContext(), ZMQ_PAIR);
            controlSocket.connect(m_controlEndpoint.c_str());
            zmq::pollitem_t pollItems[3] = {
                { controlSocket,        0, ZMQ_POLLIN, 0 },
                { m_socket1.Payload(),  0, ZMQ_POLLIN, 0 },
                { m_socket2.Payload(),  0, ZMQ_POLLIN, 0 }
            };
            for (;;) {
                if (zmq::poll(pollItems, 3, m_silenceTimeoutMillis) == 0) {
                    break;
                }
                if (pollItems[0].revents & ZMQ_POLLIN) {
                    break;
                }
                if (pollItems[1].revents & ZMQ_POLLIN) {
                    TransferMessage(m_socket1.Payload(), m_socket2.Payload());
                }
                if (pollItems[2].revents & ZMQ_POLLIN) {
                    TransferMessage(m_socket2.Payload(), m_socket1.Payload());
                }
            }
        }

    private:
        Unique<zmq::socket_t> m_socket1;
        Unique<zmq::socket_t> m_socket2;
        const std::string m_controlEndpoint;
        long m_silenceTimeoutMillis;
    };

}


dsb::comm::Proxy::Proxy(zmq::socket_t controlSocket, boost::thread thread)
    : m_controlSocket(std::move(controlSocket)),
      m_thread(std::move(thread))
{
    assert (static_cast<void*>(m_controlSocket)
            && "Proxy::Proxy(): invalid socket");
    assert (m_thread.get_id() != boost::thread::id()
            && "Proxy::Proxy(): thread handle does not refer to a thread");
    assert (!m_thread.try_join_for(boost::chrono::seconds(0))
            && "Proxy::Proxy(): thread has already terminated");
}


dsb::comm::Proxy::Proxy(Proxy&& other) DSB_NOEXCEPT
    : m_controlSocket(std::move(other.m_controlSocket)),
      m_thread(std::move(other.m_thread))
{
}


dsb::comm::Proxy::~Proxy()
{
    m_thread.detach();
}


dsb::comm::Proxy& dsb::comm::Proxy::operator=(Proxy&& rhs) DSB_NOEXCEPT
{
    if (m_thread.joinable()) m_thread.detach();
    m_controlSocket = std::move(rhs.m_controlSocket);
    m_thread = std::move(rhs.m_thread);
    return *this;
}


void dsb::comm::Proxy::Stop()
{
    m_controlSocket.send("", 0, ZMQ_DONTWAIT);
}


boost::thread& dsb::comm::Proxy::Thread__()
{
    return m_thread;
}


dsb::comm::Proxy dsb::comm::SpawnProxy(
    zmq::socket_t&& socket1,
    zmq::socket_t&& socket2,
    boost::chrono::milliseconds silenceTimeout)
{
    assert ((void*) socket1 && "socket1 not initialised");
    assert ((void*) socket2 && "socket2 not initialised");
    auto silenceTimeoutMillis = boost::numeric_cast<long>(silenceTimeout.count());

    const auto controlEndpoint = "inproc://" + dsb::util::RandomUUID();
    auto controlSocket = zmq::socket_t(GlobalContext(), ZMQ_PAIR);
    // We set the linger period to zero milliseconds because the proxy may have
    // terminated (due to timeout) by the time we try to send anything to it.
    const int lingerMillis = 0;
    controlSocket.setsockopt(ZMQ_LINGER, &lingerMillis, sizeof(lingerMillis));
    controlSocket.bind(controlEndpoint.c_str());

    auto thread = boost::thread(ProxyFunctor(
            Unique<zmq::socket_t>(std::move(socket1)),
            Unique<zmq::socket_t>(std::move(socket2)),
            controlEndpoint,
            silenceTimeoutMillis
        ));
    assert (!(void*) socket1 && "socket1 not moved into proxy thread");
    assert (!(void*) socket2 && "socket2 not moved into proxy thread");
    return Proxy(std::move(controlSocket), std::move(thread));
}


dsb::comm::Proxy dsb::comm::SpawnProxy(
    int socketType1, const std::string& endpoint1,
    int socketType2, const std::string& endpoint2,
    boost::chrono::milliseconds silenceTimeout)
{
    auto socket1 = zmq::socket_t(GlobalContext(), socketType1);
    socket1.bind(endpoint1.c_str());
    auto socket2 = zmq::socket_t(GlobalContext(), socketType2);
    socket2.bind(endpoint2.c_str());
    return SpawnProxy(std::move(socket1), std::move(socket2), silenceTimeout);
}
