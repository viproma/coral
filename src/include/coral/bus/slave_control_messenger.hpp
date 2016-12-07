/**
\file
\brief  Defines the coral::bus::ISlaveControlMessenger interface and the two
        related functions ConnectToSlave() and MakeSlaveControlMessenger().

ConnectToSlave() is a good place to start for understanding how to use the
stuff in this header.
*/
#ifndef CORAL_BUS_SLAVE_CONTROL_MESSENGER_HPP
#define CORAL_BUS_SLAVE_CONTROL_MESSENGER_HPP

#include <chrono>
#include <functional>
#include <memory>
#include <system_error>
#include <vector>

#include "boost/noncopyable.hpp"

#include "coral/config.h"

#include "coral/bus/slave_setup.hpp"
#include "coral/net/reactor.hpp"
#include "coral/model.hpp"
#include "coral/net.hpp"


namespace coral
{
namespace bus
{


/// The various states a slave may be in.
enum SlaveState
{
    SLAVE_NOT_CONNECTED,///< Slave is not yet connected
    SLAVE_CONNECTED,    // For internal use
    SLAVE_READY,        ///< Slave is ready
    SLAVE_STEP_OK,      ///< Slave has performed a step and published its variables
    SLAVE_STEP_FAILED,  ///< Slave failed to perform a time step.
    SLAVE_BUSY,         ///< Slave is currently performing some action
};


// Internal types, intentionally left undefined and undocumented.
class PendingSlaveControlConnectionPrivate;
struct SlaveControlConnectionPrivate;


/**
\brief  A handle for a pending connection to a slave.

This type is moveable, non-copyable and default-constructible.
See ConnectToSlave() for more information about its use.
*/
class PendingSlaveControlConnection
{
public:
    explicit PendingSlaveControlConnection(
        std::shared_ptr<PendingSlaveControlConnectionPrivate> p = nullptr) CORAL_NOEXCEPT;
    PendingSlaveControlConnection(PendingSlaveControlConnection&&) CORAL_NOEXCEPT;
    PendingSlaveControlConnection& operator=(PendingSlaveControlConnection&&) CORAL_NOEXCEPT;
    ~PendingSlaveControlConnection() CORAL_NOEXCEPT;

    /**
    \brief  Aborts the connection attempt and calls the completion handler with
            an error code.

    If the connection has already been completed and control of it passed into
    a SlaveControlConnection handle (i.e., the completion handler has been
    called), this operation has no effect.
    */
    void Close();

    /**
    \brief  Returns whether this object refers to a pending connection.

    This is `true` from the moment the object is created (by ConnectToSlave())
    up to (but not including) the point at which the completion handler is
    called.
    */
    operator bool() const CORAL_NOEXCEPT;

private:
    std::shared_ptr<PendingSlaveControlConnectionPrivate> m_private;
};


/**
\brief  A handle for an established connection to a slave.

This type is moveable, non-copyable and default-constructible.
See ConnectToSlave() for more information about its use.
*/
class SlaveControlConnection
{
public:
    /// Default constructor.
    SlaveControlConnection() CORAL_NOEXCEPT;

    // For internal use.
    explicit SlaveControlConnection(
        std::unique_ptr<SlaveControlConnectionPrivate> p) CORAL_NOEXCEPT;

    /// Move constructor.
    SlaveControlConnection(SlaveControlConnection&&) CORAL_NOEXCEPT;

    /// Move assignment.
    SlaveControlConnection& operator=(SlaveControlConnection&&) CORAL_NOEXCEPT;

    ~SlaveControlConnection() CORAL_NOEXCEPT;

    /**
    \brief  Returns whether this object refers to an established connection.

    */
    operator bool() const CORAL_NOEXCEPT;

    // For internal use.
    SlaveControlConnectionPrivate& Private();

private:
    std::unique_ptr<SlaveControlConnectionPrivate> m_private;
};


/**
\brief  An interface for classes that implement various versions of the
        master/slave communication protocol.

Use the MakeSlaveControlMessenger() factory function to obtain a messenger
of the appropriate type for a given slave.
*/
class ISlaveControlMessenger : boost::noncopyable
{
public:
    /**
    \brief  Basic completion handler type without any arguments aside from
            an error code.
    */
    typedef std::function<void(const std::error_code&)> VoidHandler;

    /**
    \brief  Destructor

    Note that the destructor does *not* call Terminate() or Close().  The
    completion handlers for any ongoing or pending operations will therefore
    never get called.  Use Terminate() or Close() before destruction to ensure
    that all callbacks are called.
    */
    virtual ~ISlaveControlMessenger() CORAL_NOEXCEPT { }

    /**
    \brief  Returns the current state of the slave, as deduced from the messages
            that have been sent to it and its replies (or lack thereof).
    */
    virtual SlaveState State() const CORAL_NOEXCEPT = 0;

    /**
    \brief  Ends all communication with the slave.

    This will cause the completion handler for any on-going operation to be
    called with error code coral::error::generic_error::aborted.

    Note that this simply closes the communication channel without notifying
    the slave first.  This means that the slave may stay alive, waiting for
    a command from the master which never comes, until it times out and shuts
    itself down.  Use Terminate() to ensure a controlled slave termination.

    \post `State() == SLAVE_NOT_CONNECTED`
    */
    virtual void Close() = 0;


    /// Completion handler type for GetDescription()
    typedef std::function<void(const std::error_code&, const coral::model::SlaveDescription&)>
        GetDescriptionHandler;

    /**
    \brief  Requests a description of the slave.

    On return, the slave state is `SLAVE_BUSY`.  When the operation completes
    (or fails), `onComplete` is called.  Before `onComplete` is called, the
    slave state is updated to one of the following:

      - `SLAVE_READY` on success or non-fatal failure
      - `SLAVE_NOT_CONNECTED` on fatal failure

    `onComplete` must have the following signature:
    ~~~{.cpp}
    void f(const std::error_code&, const coral::model::SlaveDescription&);
    ~~~
    The first argument specified whether an error occurred, and if so, which
    one.  If an error occurred, the second argument should be ignored.
    Otherwise, the second argument is a description of the slave.  Note that
    this object may not have a correct slave ID and name, as this information
    may not be known by the slave itself.

    Possible error conditions are:

      - `std::errc::bad_message`: The slave sent invalid data.
      - `std::errc::timed_out`: The slave did not reply in time.
      - `std::errc::operation_canceled`: The operation was aborted
            (e.g. by Close()).
      - `coral::error::generic_error::failed`: The operation failed (e.g. due to
            an error in the slave).

    All error conditions are fatal unless otherwise specified.

    \param [in] timeout         Max. allowed time for the operation to complete
    \param [in] onComplete      Completion handler

    \throws std::invalid_argument if `timeout` is less than 1 ms or
        if `onComplete` is empty.

    \pre  `State() == SLAVE_READY`
    \post `State() == SLAVE_BUSY`.
    */
    virtual void GetDescription(
        std::chrono::milliseconds timeout,
        GetDescriptionHandler onComplete) = 0;


    /// Completion handler type for SetVariables()
    typedef VoidHandler SetVariablesHandler;

    /**
    \brief  Sets the values of, or connects, one or more of the slave's
            variables.

    On return, the slave state is `SLAVE_BUSY`.  When the operation completes
    (or fails), `onComplete` is called.  Before `onComplete` is called, the
    slave state is updated to one of the following:

      - `SLAVE_READY` on success or non-fatal failure
      - `SLAVE_NOT_CONNECTED` on fatal failure

    `onComplete` must have the following signature:
    ~~~{.cpp}
    void f(const std::error_code&);
    ~~~
    Possible error conditions are:

      - `std::errc::bad_message`: The slave sent invalid data.
      - `std::errc::timed_out`: The slave did not reply in time.
      - `coral::error::generic_error::aborted`: The operation was aborted
            (e.g. by Close()).
      - `coral::error::generic_error::failed`: The operation failed (e.g. due to
            an error in the slave).

    All error conditions are fatal unless otherwise specified.

    \note
        At the moment, there are no non-fatal failures.  In the future, errors
        such as "invalid variable value" will be non-fatal, but none such are
        implemented yet.

    \param [in] settings        A list of variable values and connections
    \param [in] timeout         Max. allowed time for the operation to complete
    \param [in] onComplete      Completion handler

    \throws std::invalid_argument if `timeout` is less than 1 ms or
        if `onComplete` is empty.

    \pre  `State() == SLAVE_READY`
    \post `State() == SLAVE_BUSY`.
    */
    virtual void SetVariables(
        const std::vector<coral::model::VariableSetting>& settings,
        std::chrono::milliseconds timeout,
        SetVariablesHandler onComplete) = 0;


    /// Completion handler type for SetPeers()
    typedef VoidHandler SetPeersHandler;

    /**
    \brief  Sets or resets the list of peers to which the slave should be
            connected for the purpose of subscribing to variable data.

    On return, the slave state is `SLAVE_BUSY`.  When the operation completes
    (or fails), `onComplete` is called.  Before `onComplete` is called, the
    slave state is updated to one of the following:

      - `SLAVE_READY` on success or non-fatal failure
      - `SLAVE_NOT_CONNECTED` on fatal failure

    `onComplete` must have the following signature:
    ~~~{.cpp}
    void f(const std::error_code&);
    ~~~
    Possible error conditions are:

      - `std::errc::bad_message`: The slave sent invalid data.
      - `std::errc::timed_out`: The slave did not reply in time.
      - `coral::error::generic_error::aborted`: The operation was aborted
            (e.g. by Close()).
      - `coral::error::generic_error::failed`: The operation failed (e.g. due to
            an error in the slave).

    All error conditions are fatal unless otherwise specified.

    \note
        At the moment, there are no non-fatal failures.  In the future, errors
        such as "invalid variable value" will be non-fatal, but none such are
        implemented yet.

    \param [in] peers           A list of peer endpoints
    \param [in] timeout         Max. allowed time for the operation to complete
    \param [in] onComplete      Completion handler

    \throws std::invalid_argument if `timeout` is less than 1 ms or
        if `onComplete` is empty.

    \pre  `State() == SLAVE_READY`
    \post `State() == SLAVE_BUSY`.
    */
    virtual void SetPeers(
        const std::vector<coral::net::Endpoint>& peer,
        std::chrono::milliseconds timeout,
        SetPeersHandler onComplete) = 0;


    /// Completion handler type for ResendVars()
    typedef VoidHandler ResendVarsHandler;

    /**
    \brief  Makes the slave send all variable values and then wait to receive
            values for all connected input variables.

    On return, the slave state is `SLAVE_BUSY`.  When the operation completes
    (or fails), `onComplete` is called.  Before `onComplete` is called, the
    slave state is updated to one of the following:

      - `SLAVE_READY` on success or non-fatal failure
      - `SLAVE_NOT_CONNECTED` on fatal failure

    `onComplete` must have the following signature:
    ~~~{.cpp}
    void f(const std::error_code&);
    ~~~
    Possible error conditions are:

      - `coral::error::sim_error::data_timeout`: The slave did not receive all
            expected variable values in time.  This is a non-fatal error.
      - `std::errc::bad_message`: The slave sent invalid data.
      - `std::errc::timed_out`: The slave did not reply in time.
      - `coral::error::generic_error::aborted`: The operation was aborted
            (e.g. by Close()).
      - `coral::error::generic_error::failed`: The operation failed (e.g. due to
            an error in the slave).

    All error conditions are fatal unless otherwise specified.

    \param [in] timeout         Max. allowed time for the operation to complete
    \param [in] onComplete      Completion handler

    \throws std::invalid_argument if `timeout` is less than 1 ms or
        if `onComplete` is empty.

    \pre  `State() == SLAVE_READY`
    \post `State() == SLAVE_BUSY`.
    */
    virtual void ResendVars(
        std::chrono::milliseconds timeout,
        ResendVarsHandler onComplete) = 0;


    /// Completion handler type for Step()
    typedef VoidHandler StepHandler;

    /**
    \brief  Tells the slave to perform a time step

    On return, the slave state is `SLAVE_BUSY`.  When the operation completes
    (or fails), `onComplete` is called.  Before `onComplete` is called, the
    slave state is updated to one of the following:

      - `SLAVE_STEP_OK` if the step completed and the slave has sent its outputs
      - `SLAVE_STEP_FAILED` if the step could not be completed
      - `SLAVE_NOT_CONNECTED` on fatal failure

    `onComplete` must have the following signature:
    ~~~{.cpp}
    void f(const std::error_code&);
    ~~~
    Possible error conditions are:

      - `coral::error::sim_error::cannot_perform_timestep` (non-fatal): The slave
            was unable to complete the time step.
      - `std::errc::bad_message`: The slave sent invalid data.
      - `std::errc::timed_out`: The slave did not reply in time.
      - `coral::error::generic_error::aborted`: The operation was aborted
            (e.g. by Close()).
      - `coral::error::generic_error::failed`: The operation failed (e.g. due to
            an error in the slave).

    All error conditions are fatal unless otherwise specified.

    \param [in] stepID          The ID of the time step to be performed
    \param [in] currentT        The current time point
    \param [in] deltaT          The step size
    \param [in] timeout         Max. allowed time for the operation to complete
    \param [in] onComplete      Completion handler

    \throws std::invalid_argument if `timeout` is less than 1 ms or
        if `onComplete` is empty.

    \pre  `State() == SLAVE_READY`
    \post `State() == SLAVE_BUSY`.
    */
    virtual void Step(
        coral::model::StepID stepID,
        coral::model::TimePoint currentT,
        coral::model::TimeDuration deltaT,
        std::chrono::milliseconds timeout,
        StepHandler onComplete) = 0;


    /// Completion handler type for AcceptStep()
    typedef VoidHandler AcceptStepHandler;

    /**
    \brief  Tells the slave that the time step is accepted and it should
            update its inputs with data from other slaves.

    On return, the slave state is `SLAVE_BUSY`.  When the operation completes
    (or fails), `onComplete` is called.  Before `onComplete` is called, the
    slave state is updated to one of the following:

      - `SLAVE_READY` on success
      - `SLAVE_NOT_CONNECTED` on failure

    `onComplete` must have the following signature:
    ~~~{.cpp}
    void f(const std::error_code&);
    ~~~
    Possible error conditions are:

      - `std::errc::bad_message`: The slave sent invalid data.
      - `std::errc::timed_out`: The slave did not reply in time.
      - `coral::error::generic_error::aborted`: The operation was aborted
            (e.g. by Close()).
      - `coral::error::generic_error::failed`: The operation failed (e.g. due to
            an error in the slave).

    \param [in] timeout         Max. allowed time for the operation to complete
    \param [in] onComplete      Completion handler

    \throws std::invalid_argument if `timeout` is less than 1 ms or
        if `onComplete` is empty.

    \pre  `State() == SLAVE_STEP_OK`
    \post `State() == SLAVE_BUSY`.
    */
    virtual void AcceptStep(
        std::chrono::milliseconds timeout,
        AcceptStepHandler onComplete) = 0;

    /**
    \brief  Instructs the slave to terminate, then closes the connection.

    This function has the same effect as Close(), except that it instructs
    the slave to terminate first.

    This function may be called while another operation is ongoing (i.e.,
    `State() == SLAVE_BUSY`).  This will cause the callback of the original
    operation to be called with error code `std::errc::operation_canceled`.

    \pre  `State() != SLAVE_NOT_CONNECTED`
    \post `State() == SLAVE_NOT_CONNECTED`
    */
    virtual void Terminate() = 0;
};


/// Completion handler type for ConnectToSlave()
typedef std::function<void(const std::error_code&, SlaveControlConnection)>
    ConnectToSlaveHandler;


/**
\brief  Initiates a master's connection to a slave.

This function attempts to send a HELLO message to the slave, and then
immediately returns a PendingSlaveControlConnection object which acts as a
reference to the pending connection.  It is the caller's responsibility to hold
on to this object until the connection has been established, as it will be
aborted otherwise.

When the slave replies with a HELLO message of its own, thus completing the
handshake, the completion handler (`onComplete`) is called.
This handler is passed a SlaveControlConnection object, which acts as a
reference to the *established* connection.  From this moment, the previously
returned PendingSlaveControlConnection object has served its purpose, and may
be disposed of (or simply ignored).

The SlaveControlConnection object contains, among other things, which protocol
the slave expects to use.  To continue communication with the slave, this
object must be passed to the MakeSlaveControlMessenger() function, which will
create and return an appropriate ISlaveControlMessenger for the requested
protocol.  If the object is destroyed without being passed to
MakeSlaveControlMessenger, the connection will be closed.

The `timeout` argument specifies how long to wait for a reply from the slave.
If no reply is received within the given duration, the connection may be
retried so that a new HELLO is sent.  The maximum number of attempts (including
the first) is specified by `maxAttempts`.

If the connection fails for some reason (e.g. it is refused by the slave,
or the timeout is reached after the last attempt) the completion handler is
called with an error code and a "null" SlaveControlConnection object.

`onComplete` must have the following signature:
~~~{.cpp}
void f(const std::error_code&, SlaveControlConnection);
~~~
Possible error conditions are:

  - `std::errc::connection_refused`: The connection was aborted by the slave
        due to an error.
  - `std::errc::permission_denied`: The connection was denied by the slave.
  - `std::errc::bad_message`: The slave sent invalid data.
  - `std::errc::timed_out`: The slave is unreachable, and the last connection
        attempt timed out.
  - `std::errc::operation_canceled`: The connection was aborted because
        PendingSlaveControlConnection::Close() was called.

Note that the completion handler is never called if ConnectToSlave() throws
an exception.

\param [in] reactor         The reactor used to listen for a reply from the
                            slave.  This will later be used by the
                            ISlaveControlMessenger object to perform further
                            communication with it, so it is important that
                            it outlives this object.
\param [in] slaveLocator    Information about how to connect to the slave.
                            May not be empty.
\param [in] maxAttempts     The maximum number of times to attempt a connection.
                            Must be at least 1.
\param [in] timeout         The maximum time to wait for a reply from the slave
                            for each connection attempt.  Must be at least 1 ms.
\param [in] onComplete      Completion handler.  May not be null.

\throws std::invalid_argument if any of the arguments are invalid.
*/
PendingSlaveControlConnection ConnectToSlave(
    coral::net::Reactor& reactor,
    const coral::net::SlaveLocator& slaveLocator,
    int maxAttempts,
    std::chrono::milliseconds timeout,
    ConnectToSlaveHandler onComplete);


/// Completion handler type for MakeSlaveControlMessenger()
typedef std::function<void(const std::error_code&)>
    MakeSlaveControlMessengerHandler;

/**
\brief  Returns an object which handles communication with a slave after the
        connection has been established.

Before this function can be called, a connection to the slave must be
established using ConnectToSlave().  Its completion handler receives a
SlaveControlConnection object which is then passed to this function,
which in turn creates and returns an ISlaveControlMessenger object appropriate
for the protocol version requested by the slave.  When the function returns,
the SlaveControlConnection object has served its purpose and may be disposed of
(or simply ignored).

This function also sends some initial configuration data to the slave, for
for example its assigned numeric ID.  When the slave has received and
acknowledged this, the completion handler `onComplete` is called.
Until then, the returned messenger object will be in the SLAVE_BUSY state, and
most of its methods may not be called.  (See the ISlaveControlMessenger
documentation for information about each method's preconditions.)

`onComplete` must have the following signature:
~~~{.cpp}
void f(const std::error_code&);
~~~
Possible error conditions are:

  - `std::errc::bad_message`: The slave sent invalid data.
  - `std::errc::timed_out`: The slave has become unreachable and the connection
        timed out.  The timeout used is the one which was passed to
        ConnectToSlave().
  - `coral::error::generic_error::operation_failed`: The operation failed
        (e.g. due to an error in the slave).

Note that the completion handler is never called if MakeSlaveControlMessenger()
throws an exception.

\param [in] connection      The connection object passed to ConnectToSlave()'s
                            completion handler.  Must refer to an established
                            connection.
\param [in] slaveID         The ID number assigned to the slave. Must be a
                            valid ID.
\param [in] slaveName       The name given to the slave.
\param [in] setup           Slave configuration parameters
\param [in] onComplete      Completion handler. May not be null.

\throws coral::error::ProtocolNotSupported if the slave requested an unsupported
    protocol version.
\throws std::invalid_argument if any of the input arguments are invalid.
*/
std::unique_ptr<ISlaveControlMessenger> MakeSlaveControlMessenger(
    SlaveControlConnection connection,
    coral::model::SlaveID slaveID,
    const std::string& slaveName,
    const SlaveSetup& setup,
    MakeSlaveControlMessengerHandler onComplete);


}} // namespace
#endif // header guard
