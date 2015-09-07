/**
\file
\brief  Defines the dsb::bus::ExecutionManager class
*/
#ifndef DSB_BUS_EXECUTION_MANAGER_HPP
#define DSB_BUS_EXECUTION_MANAGER_HPP

#include <functional>
#include <memory>
#include <system_error>
#include <utility>
#include <vector>

#include "boost/chrono/duration.hpp"
#include "boost/noncopyable.hpp"

#include "dsb/model.hpp"
#include "dsb/net.hpp"

#include "dsb/bus/slave_controller.hpp"
#include "dsb/bus/slave_setup.hpp"
#include "dsb/comm/reactor.hpp"


namespace dsb
{
/// Functions and classes related to the simulation bus structure
namespace bus
{

class ExecutionManagerPrivate;


/// Manages and coordinates all participants in an execution.
class ExecutionManager : boost::noncopyable
{
public:
    /// Constructs an object which manages the given execution.
    explicit ExecutionManager(const dsb::net::ExecutionLocator& execLoc);

    ~ExecutionManager();

    /// Completion handler type for BeginConfig().
    typedef std::function<void(const std::error_code&)> BeginConfigHandler;

    /// Enters configuration mode.
    void BeginConfig(BeginConfigHandler onComplete);

    /// Completion handler type for EndConfig().
    typedef std::function<void(const std::error_code&)> EndConfigHandler;

    /// Leaves configuration mode, enters simulation mode.
    void EndConfig(EndConfigHandler onComplete);

    /// Terminates the entire execution and all associated slaves.
    void Terminate();

    /// Sets the total simulation time for the execution.
    void SetSimulationTime(
        dsb::model::TimePoint startTime,
        dsb::model::TimePoint stopTime);

    /// Completion handler type for AddSlave().
    typedef std::function<void(const std::error_code&, dsb::model::SlaveID)>
        AddSlaveHandler;

    /**
    \brief  Adds a slave to the execution.

    The bus will connect the slave asynchronously and call `onComplete` when
    this is done.  If the connection fails, `onComplete` will be called with
    a "failure" result, after which the slave ID is no longer valid.  (However,
    the ID will not be reused for another slave later.)

    Note that this is a compound operation which consists of two or more steps
    of communication with the slave.  First a certain maximum number of attempts
    (N) to contact the slave are performed, and if this is successful, a slave
    setup step.  The timeout in this case is for each of those steps, so the
    maximum possible time this can take is `(N+1)*commTimeout`.

    \returns the ID of the new slave.
    \throws std::length_error if adding a slave would cause the number of slaves
        to exceed the hard maximum limit of 65535 (which is unlikely indeed).
    */
    dsb::model::SlaveID AddSlave(
        const dsb::net::SlaveLocator& slaveLocator,
        dsb::comm::Reactor& reactor,
        boost::chrono::milliseconds commTimeout,
        AddSlaveHandler onComplete);

    /// Completion handler type for SetVariables().
    typedef std::function<void(const std::error_code&)> SetVariablesHandler;

    /**
    \brief  Sets the values or connections of one or more variables for a single
            slave.
    */
    void SetVariables(
        dsb::model::SlaveID slave,
        const std::vector<dsb::model::VariableSetting>& settings,
        boost::chrono::milliseconds timeout,
        SetVariablesHandler onComplete);

    /// Completion handler type for the Step() function.
    typedef std::function<void(const std::error_code&)> StepHandler;

    /// Completion handler type for the Step() function of individual slaves.
    typedef std::function<void(const std::error_code&, dsb::model::SlaveID)>
        SlaveStepHandler;

    /// Steps the simulation forward.
    void Step(
        dsb::model::TimeDuration stepSize,
        boost::chrono::milliseconds timeout,
        StepHandler onComplete,
        SlaveStepHandler onSlaveStepComplete = nullptr);

    /// Completion handler type for the AcceptStep() function.
    typedef std::function<void(const std::error_code&)> AcceptStepHandler;

    /**
    \brief  Completion handler type for the AcceptStep() function of individual
            slaves.
    */
    typedef std::function<void(const std::error_code&, dsb::model::SlaveID)>
        SlaveAcceptStepHandler;

    /// Informs the slaves that the step is accepted.
    void AcceptStep(
        boost::chrono::milliseconds timeout,
        AcceptStepHandler onComplete,
        SlaveAcceptStepHandler onSlaveAcceptStepComplete = nullptr);

private:
    std::unique_ptr<ExecutionManagerPrivate> m_private;
};


}} // namespace
#endif // header guard
