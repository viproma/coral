/**
\file
\brief Defines the dsb::bus::SlaveTracker class.
*/
#ifndef DSB_BUS_SLAVE_TRACKER_HPP
#define DSB_BUS_SLAVE_TRACKER_HPP

#include <cstdint>
#include <deque>
#include <queue>

#include "zmq.hpp"
#include "execution.pb.h"


namespace dsb
{
namespace bus
{


/// The various states a slave may be in.
enum SlaveState
{
    SLAVE_UNKNOWN       = 1,
    SLAVE_CONNECTING    = 1 << 1,
    SLAVE_CONNECTED     = 1 << 2,
    SLAVE_READY         = 1 << 3,
    SLAVE_BUSY          = 1 << 4,
    SLAVE_STEPPING      = 1 << 5,
    SLAVE_PUBLISHED     = 1 << 6,
    SLAVE_RECEIVING     = 1 << 7,
    SLAVE_STEP_FAILED   = 1 << 8,
    SLAVE_TERMINATED    = 1 << 9,
};


enum
{
    TERMINATABLE_STATES = SLAVE_READY | SLAVE_PUBLISHED | SLAVE_STEP_FAILED
};


/**
\brief  A class which handles the communication with, and keeps track of the
        state of, one slave in a simulation.
*/
class SlaveTracker
{
public:
    /**
    \brief  Constructor.

    After construction, State() will return SlaveState::SLAVE_UNKNOWN.
    */
    SlaveTracker(double startTime, double stopTime);

    /**
    \brief  Copy constructor.
    \throws std::exception if the copy operation failed.
    */
    SlaveTracker(const SlaveTracker& other);

    /**
    \brief  Assignment operator.
    \throws std::exception if the copy operation failed.
    */
    SlaveTracker& operator=(const SlaveTracker& other);

    /**
    \brief  Processes a message from the slave, and if appropriate, sends
            a reply.

    This function will parse the message in `msg` and update the state of the
    slave handler according to its contents.  An immediate reply will be sent
    on `socket` to the peer identified by `envelope` if the request warrants
    it; otherwise, the envelope will be stored  in the SlaveTracker until it is
    time to send a reply (e.g. with SendStep()).

    \param [in] socket    The socket on which to send the message.
    \param [in] envelope  The return envelope, empty on return.
    \param [in] msg       The incoming message, empty on return.

    \returns `true` if an immediate reply was sent, `false` if not.
    \throws zmq::error_t if ZMQ fails to send the reply message.

    \pre `socket` is a valid ZMQ socket, `envelope` and `msg` are not empty.
    \post `envelope` and `msg` are empty.
    */
    bool RequestReply(
        zmq::socket_t& socket,
        std::deque<zmq::message_t>& envelope,
        std::deque<zmq::message_t>& msg);

    /// The last known state of the slave.
    SlaveState State() const;

    /**
    \brief  Sends a SET_VARS message immediately if the slave is ready to
            receive one; otherwise it will be enqueued and sent the next
            time the slave enters the READY state.

    \param [in] socket  The socket used to communicate with slaves.
    \param [in] data    The SET_VARS data.

    \throws zmq::error_t on failure to send the message.
    */
    void EnqueueSetVars(
        zmq::socket_t& socket,
        const dsbproto::execution::SetVarsData& data);

    /**
    \brief  Sends a CONNECT_VARS message immediately if the slave is ready to
            receive one; otherwise it will be enqueued and sent the next
            time the slave enters the READY state.

    \param [in] socket  The socket used to communicate with slaves.
    \param [in] data    The CONNECT_VARS data.

    \throws zmq::error_t on failure to send the message.
    */
    void EnqueueConnectVars(
        zmq::socket_t& socket,
        const dsbproto::execution::ConnectVarsData& data);

    /**
    \brief  Sends a STEP message on `socket` and sets the IsSimulating() flag
            to `true`.

    \param [in] socket  The socket on which to send the message.
    \param [in] data    The STEP data.

    \pre    The previous call to RequestReply() returned `false`.
    \post   `State() == SLAVE_STEPPING && IsSimulating()`

    \throws zmq::error_t on failure to send the message.
    */
    void SendStep(zmq::socket_t& socket, const dsbproto::execution::StepData& data);

    /**
    \brief  Sends a TERMINATE message on `socket` and sets the IsSimulating()
            flag to `false`.

    \param [in] socket  The socket on which to send the message.

    \pre    The previous call to RequestReply() returned `false`.
    \post   `State() == SLAVE_TERMINATED && !IsSimulating()`

    \throws zmq::error_t on failure to send the message.
    */
    void SendTerminate(zmq::socket_t& socket);

    /**
    \brief  Sends a RECV_VARS message on `socket`.

    \param [in] socket  The socket on which to send the message.

    \pre    The previous call to RequestReply() returned `false`.
    \post   `State() == SLAVE_RECEIVING`

    \throws zmq::error_t on failure to send the message.
    */
    void SendRecvVars(zmq::socket_t& socket);

    /**
    \brief  Whether this slave is currently performing a simulation.

    This is `true` if and only if the slave has at some point received a STEP
    message and it has not received a subsequent TERMINATE message.
    */
    bool IsSimulating() const;

private:
    // Invalid protocol number
    static const uint16_t UNKNOWN_PROTOCOL = 0xFFFF;

    // Functions that handle specific message types for RequestReply().
    // On return, the reply (or, strictly speaking, the following request) is
    // stored in the `msg` argument.
    bool HelloHandler(std::deque<zmq::message_t>& msg);
    bool SubmitHandler(std::deque<zmq::message_t>& msg);
    bool ReadyHandler(std::deque<zmq::message_t>& msg);
    bool StepFailedHandler(std::deque<zmq::message_t>& msg);
    bool StepOkHandler(std::deque<zmq::message_t>& msg);

    // If the current state is one of the states OR-ed together in `oldStates`,
    // this function sets the state to `newState` and returns `true`.
    // Otherwise, it returns `false`.
    bool UpdateSlaveState(int oldStates, SlaveState newState);

    // Function which does the common work for the SendXxx() functions.
    // It calls UpdateSlaveState() and sends `msg` on `socket`.
    void SendSynchronousMsg(
        zmq::socket_t& socket,
        std::deque<zmq::message_t>& msg,
        int allowedOldStates,
        SlaveState newState);

    double m_startTime;
    double m_stopTime;
    uint16_t m_protocol;
    SlaveState m_state;
    bool m_isSimulating;
    std::deque<zmq::message_t> m_envelope;
    std::queue<dsbproto::execution::SetVarsData> m_pendingSetVars;
    std::queue<dsbproto::execution::ConnectVarsData> m_pendingConnectVars;
};


}}      // namespace
#endif  // header guard
