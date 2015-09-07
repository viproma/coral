/**
\file
\brief  Defines the dsb::bus::SlaveController class
*/
#ifndef DSB_BUS_SLAVE_CONTROLLER_HPP
#define DSB_BUS_SLAVE_CONTROLLER_HPP

#include <functional>
#include <memory>
#include <system_error>
#include <vector>

#include "boost/chrono/duration.hpp"

#include "dsb/config.h"
#include "dsb/bus/slave_control_messenger.hpp"
#include "dsb/bus/slave_setup.hpp"
#include "dsb/comm/reactor.hpp"
#include "dsb/model.hpp"
#include "dsb/net.hpp"


namespace dsb
{
namespace bus
{


/// A class which is used for controlling one slave in an execution.
class SlaveController
{
public:
    /**
    \brief  Basic completion handler type without any arguments aside from
            an error code.
    */
    typedef std::function<void(const std::error_code&)> VoidHandler;


    /// Completion handler type for the constructor.
    typedef VoidHandler ConnectHandler;

    /**
    \brief  Constructor

    The constructor initiates the connection to the slave and returns
    immediately.  Whether the connection succeeds or fails is reported
    asynchronously via `onComplete`.  In the meantime, it is possible to
    enqueue other commands (e.g. SetVariables()) pending a successful
    connection.  If the connection fails, any pending commands will be
    canceled and their callbacks will be called with error code
    `dsb::error::generic_error::canceled`.

    If the initial connection fails (e.g. if the slave is not up and running
    yet), the connection may be retried automatically.  The maximum number of
    connection attempts is given by `maxConnectionAttempts`.

    \param [in] reactor
        The reactor used for the messaging/event loop.
    \param [in] slaveLocator
        Information about how to connect to the slave.
    \param [in] slaveID
        The ID number assigned to the slave.
    \param [in] setup
        Slave configuration parameters.
    \param [in] timeout
        Max. allowed time for the slave to reply to each message sent to it.
    \param [in] onComplete
        Completion handler.
    \param [in] maxConnectionAttempts
        How many times to try the connection if it fails.  This includes the
        first one, so the value must be at least 1. The default is 3.

    \throws std::invalid_argument if `slaveLocator` is empty, if `slaveID` is
        invalid, if `timeout` is less than 1 ms, if `onComplete` is empty,
        or if `maxConnectionAttempts < 1`.
    */
    SlaveController(
        dsb::comm::Reactor& reactor,
        const dsb::net::SlaveLocator& slaveLocator,
        dsb::model::SlaveID slaveID,
        const SlaveSetup& setup,
        boost::chrono::milliseconds timeout,
        ConnectHandler onComplete,
        int maxConnectionAttempts = 3);

    /**
    \brief  Destructor

    Note that the destructor does *not* call Terminate() or Close().  The
    completion handlers for any ongoing or pending operations will therefore
    never get called.  Use Terminate() or Close() before destruction to ensure
    that all callbacks are called.
    */
    ~SlaveController();

    /**
    \brief  Closes the connection to the slave and cancels all pending
            operations.
    
    After this, the SlaveController can no longer be used for anything.

    Note that this simply closes the communication channel without notifying
    the slave first.  This means that the slave may stay alive, waiting for
    a command from the master which never comes, until it times out and shuts
    itself down.  Use Terminate() to ensure a controlled slave termination.
    */
    void Close();

    /// Returns the current state of the slave.
    SlaveState State() const DSB_NOEXCEPT;

    /// Completion handler type for SetVariables()
    typedef VoidHandler SetVariablesHandler;

    /**
    \brief  Sets the values of, or connects, one or more of the slave's
            variables.

    \param [in] settings
        A list of variable values and connections. May not be empty.
    \param [in] timeout
        Max. allowed time for the operation to complete. Must be at least 1 ms.
    \param [in] onComplete
        Completion handler.
    */
    void SetVariables(
        const std::vector<dsb::model::VariableSetting>& settings,
        boost::chrono::milliseconds timeout,
        SetVariablesHandler onComplete);

    /// Completion handler type for Step()
    typedef VoidHandler StepHandler;

    /**
    \brief  Makes the slave perform a time step.

    \param [in] currentT
        The current time point.
    \param [in] deltaT
        The step size. Must be positive.
    \param [in] timeout
        Max. allowed time for the operation to complete. Must be at least 1 ms.
    \param [in] onComplete
        Completion handler.
    */
    void Step(
        dsb::model::TimePoint currentT,
        dsb::model::TimeDuration deltaT,
        boost::chrono::milliseconds timeout,
        StepHandler onComplete);

    /// Completion handler type for AcceptStep()
    typedef VoidHandler AcceptStepHandler;

    /**
    \brief  Tells the slave that the time step is accepted and it should
            update its inputs with results from other slaves.

    \param [in] timeout
        Max. allowed time for the operation to complete. Must be at least 1 ms.
    \param [in] onComplete
        Completion handler.
    */
    void AcceptStep(
        boost::chrono::milliseconds timeout,
        AcceptStepHandler onComplete);

    /**
    \brief  Terminates the slave and cancels all pending operations.

    After this, the SlaveController can no longer be used for anything.
    */
    void Terminate();

private:
    // Make this class non-movable, since we leak pointers to 'this' in lambda
    // functions passed to SlaveControlMessenger.
    SlaveController(SlaveController&&);
    SlaveController& operator=(SlaveController&&);

    // A handle for the pending connection.
    PendingSlaveControlConnection m_pendingConnection;

    // The object through which we communicate with the slave.
    std::unique_ptr<dsb::bus::ISlaveControlMessenger> m_messenger;
};


}} // namespace
#endif // header guard
