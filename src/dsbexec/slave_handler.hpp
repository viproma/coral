#ifndef DSB_SLAVE_SLAVE_HANDLER_HPP
#define DSB_SLAVE_SLAVE_HANDLER_HPP

#include <cstdint>
#include <deque>
#include "zmq.hpp"


/// The various states a slave may be in.
enum SlaveState
{
    SLAVE_UNKNOWN       = 1,
    SLAVE_CONNECTING    = 1 << 1,
    SLAVE_INITIALIZING  = 1 << 2,
    SLAVE_READY         = 1 << 3,
    SLAVE_STEPPING      = 1 << 4,
    SLAVE_PUBLISHED     = 1 << 5,
    SLAVE_RECEIVING     = 1 << 6,
    SLAVE_STEP_FAILED   = 1 << 7,
    SLAVE_TERMINATED    = 1 << 8,
};


/**
\brief  A class which handles the communication with, and keeps track of the
        state of, one slave in a simulation.
*/
class SlaveHandler
{
public:
    /**
    \brief  Constructor.

    After construction, State() will return SlaveState::SLAVE_UNKNOWN.
    */
    SlaveHandler();

    /**
    \brief  Copy constructor.
    \throws std::exception if the copy operation failed.
    */
    SlaveHandler(SlaveHandler& other);

    /**
    \brief  Assignment operator.
    \throws std::exception if the copy operation failed.
    */
    SlaveHandler& operator=(SlaveHandler& other);

    /**
    \brief  Processes a message from the slave, and if appropriate, prepares
            a reply message.

    This function will parse the message in `msg` and update the state of the
    slave handler according to its contents.  If the message warrants an
    immediate reply, the reply message is prepared and stored in `msg`, and
    the function returns `true`.  It is then up to the caller to actually
    send the message.  In this case, the `envelope` argument is not modified.

    If the message does *not* warrant an immediate reply, no change is made
    to `msg`.  Instead, `envelope` is stored in the SlaveHandler until it is
    time to send a reply (e.g. with SendStep()).  In this case, the `envelope`
    argument can not be expected to contain the envelope anymore when the
    function returns.

    \param [in,out] msg
        On input, the incoming message.  On output, the return message if and
        only if the function returns `true`.
    \param [in,out] envelope
        On input, the return envelope.  On output, an unspecified value if the
        function returns `false`, otherwise it remains unchanged.

    \returns `true` if `msg` contains a reply message on return, `false` if not.
    */
    bool RequestReply(
        std::deque<zmq::message_t>& msg,
        std::deque<zmq::message_t>& envelope);

    /// The last known state of the slave.
    SlaveState State() const;

    /**
    \brief  Sends a STEP message on `socket`.

    \params [in] socket
        The socket on which to send the message.
    \params [in] msg
        A validly constructed STEP message.
        The message will be empty when the function returns.

    \pre    The previous call to RequestReply() returned `false`.
    \pre    `msg` is a valid STEP message.
    \post   `State() == SLAVE_STEPPING && IsSimulating()`

    \throws zmq::error_t on failure to send the message.
    */
    void SendStep(zmq::socket_t& socket, std::deque<zmq::message_t>& msg);

    /**
    \brief  Sends a TERMINATE message on `socket`.

    \params [in] socket
        The socket on which to send the message.
    \params [in] msg
        A validly constructed TERMINATE message.
        The message will be empty when the function returns.

    \pre    The previous call to RequestReply() returned `false`.
    \pre    `msg` is a valid TERMINATE message.
    \post   `State() == SLAVE_TERMINATED && !IsSimulating()`

    \throws zmq::error_t on failure to send the message.
    */
    void SendTerminate(zmq::socket_t& socket, std::deque<zmq::message_t>& msg);

    /**
    \brief  Sends a RECV_VARS message on `socket`.

    \params [in] socket
        The socket on which to send the message.
    \params [in] msg
        A validly constructed RECV_VARS message.
        The message will be empty when the function returns.

    \pre    The previous call to RequestReply() returned `false`.
    \pre    `msg` is a valid RECV_VARS message.
    \post   `State() == SLAVE_RECEIVING`

    \throws zmq::error_t on failure to send the message.
    */
    void SendRecvVars(zmq::socket_t& socket, std::deque<zmq::message_t>& msg);

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
    bool HelloHandler(
        std::deque<zmq::message_t>& msg,
        std::deque<zmq::message_t>& envelope);
    bool InitReadyHandler(
        std::deque<zmq::message_t>& msg,
        std::deque<zmq::message_t>& envelope);
    bool ReadyHandler(
        std::deque<zmq::message_t>& msg,
        std::deque<zmq::message_t>& envelope);
    bool StepFailedHandler(
        std::deque<zmq::message_t>& msg,
        std::deque<zmq::message_t>& envelope);
    bool StepOkHandler(
        std::deque<zmq::message_t>& msg,
        std::deque<zmq::message_t>& envelope);

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

    uint16_t m_protocol;
    SlaveState m_state;
    bool m_isSimulating;
    std::deque<zmq::message_t> m_envelope;
};


#endif // header guard
