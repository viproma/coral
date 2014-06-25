#include "proxy.hpp"

#include <exception>
#include <utility>

#include "boost/thread.hpp"
#include "boost/uuid/random_generator.hpp"
#include "boost/uuid/uuid_io.hpp"


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

    /// Returns a string that contains a random UUID.
    std::string RandomUUID()
    {
        boost::uuids::random_generator gen;
        return boost::uuids::to_string(gen());
    }

    /// Single-value container with move-on-copy semantics.
    template<typename T>
    class Unique
    {
    public:
        /// Moves `value` into the newly constructed container.
        Unique(T&& value) : m_payload(std::move(value)) { }

        /**
        \brief  Copy constructor which moves the payload away from the source
                container and into the target container.
        */
        Unique(Unique& source) : m_payload(std::move(source.m_payload)) { }

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
            std::shared_ptr<zmq::context_t> context,
            Unique<zmq::socket_t> socket1,
            Unique<zmq::socket_t> socket2,
            const std::string& controlEndpoint)
            : m_context(context),
              m_socket1(socket1),
              m_socket2(socket2),
              // std::string was never designed for thread safety, and may be
              // implemented with COW, so we trick it into always making a copy.
              m_controlEndpoint(controlEndpoint.begin(), controlEndpoint.end())
        {
        }

        void operator()()
        {
            assert ((void*) *m_context);
            assert ((void*) m_socket1.Payload());
            assert ((void*) m_socket2.Payload());
            assert (!m_controlEndpoint.empty());

            auto controlSocket = zmq::socket_t(*m_context, ZMQ_PAIR);
            controlSocket.connect(m_controlEndpoint.c_str());

            zmq::pollitem_t pollItems[3] = {
                { controlSocket,        0, ZMQ_POLLIN, 0 },
                { m_socket1.Payload(),  0, ZMQ_POLLIN, 0 },
                { m_socket2.Payload(),  0, ZMQ_POLLIN, 0 }
            };
            for (;;) {
                zmq::poll(pollItems, 3);
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
        std::shared_ptr<zmq::context_t> m_context;
        Unique<zmq::socket_t> m_socket1;
        Unique<zmq::socket_t> m_socket2;
        const std::string m_controlEndpoint;
    };

}


dsb::broker::Proxy::Proxy(zmq::socket_t controlSocket, boost::thread thread)
    : m_active(true),
      m_controlSocket(std::move(controlSocket)),
      m_thread(std::move(thread))
{
    assert (static_cast<void*>(m_controlSocket)
            && "Proxy::Proxy(): invalid socket");
    assert (m_thread.get_id() != boost::thread::id()
            && "Proxy::Proxy(): thread handle does not refer to a thread");
    assert (!m_thread.try_join_for(boost::chrono::seconds(0))
            && "Proxy::Proxy(): thread has already terminated");
}


dsb::broker::Proxy::Proxy(Proxy&& other)
    : m_active(other.m_active),
      m_controlSocket(std::move(other.m_controlSocket)),
      m_thread(std::move(other.m_thread))
{
    other.m_active = false;
}


dsb::broker::Proxy::~Proxy()
{
    m_thread.detach();
}


dsb::broker::Proxy& dsb::broker::Proxy::operator=(Proxy&& rhs)
{
    if (m_active) m_thread.detach();
    m_active = rhs.m_active;
    rhs.m_active = false;
    m_controlSocket = std::move(rhs.m_controlSocket);
    m_thread = std::move(rhs.m_thread);
    return *this;
}


void dsb::broker::Proxy::Stop()
{
    if (m_active) {
        assert (!m_thread.try_join_for(boost::chrono::seconds(0))
                && "Proxy thread has terminated prematurely");
        m_controlSocket.send("", 0);
        m_active = false;
    }
}


boost::thread& dsb::broker::Proxy::Thread__()
{
    return m_thread;
}


dsb::broker::Proxy dsb::broker::SpawnProxy(
    std::shared_ptr<zmq::context_t> context,
    zmq::socket_t&& socket1,
    zmq::socket_t&& socket2)
{
    assert ((void*) *context && "context not initialised");
    assert ((void*) socket1 && "socket1 not initialised");
    assert ((void*) socket2 && "socket2 not initialised");

    const auto controlEndpoint = "inproc://" + RandomUUID();
    auto controlSocket = zmq::socket_t(*context, ZMQ_PAIR);
    controlSocket.bind(controlEndpoint.c_str());

    auto thread = boost::thread::thread(ProxyFunctor(
            context,
            Unique<zmq::socket_t>(std::move(socket1)),
            Unique<zmq::socket_t>(std::move(socket2)),
            controlEndpoint
        ));
    assert (!(void*) socket1 && "socket1 not moved into proxy thread");
    assert (!(void*) socket2 && "socket2 not moved into proxy thread");
    return Proxy(std::move(controlSocket), std::move(thread));
}


dsb::broker::Proxy dsb::broker::SpawnProxy(
    std::shared_ptr<zmq::context_t> context,
    int socketType1, const std::string& endpoint1,
    int socketType2, const std::string& endpoint2)
{
    assert ((void*) *context && "context not initialised");
    auto socket1 = zmq::socket_t(*context, socketType1);
    socket1.bind(endpoint1.c_str());
    auto socket2 = zmq::socket_t(*context, socketType2);
    socket2.bind(endpoint2.c_str());
    return SpawnProxy(context, std::move(socket1), std::move(socket2));
}
