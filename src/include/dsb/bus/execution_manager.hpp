/**
\file
\brief  Defines the dsb::bus::ExecutionManager class
*/
#ifndef DSB_BUS_EXECUTION_MANAGER_HPP
#define DSB_BUS_EXECUTION_MANAGER_HPP

#include <chrono>
#include <functional>
#include <memory>
#include <system_error>
#include <utility>
#include <vector>

#include "boost/noncopyable.hpp"

#include "dsb/config.h"
#include "dsb/model.hpp"
#include "dsb/net.hpp"

#include "dsb/bus/slave_controller.hpp"
#include "dsb/bus/slave_setup.hpp"
#include "dsb/net/reactor.hpp"


namespace dsb
{
/// Functions and classes related to the simulation bus structure
namespace bus
{


/**
\brief  Used in `ExecutionManager::Reconstitute()` to specify a slave which
        should be added to the simulation.
*/
struct AddedSlave
{
    /// The slave's network location
    dsb::net::SlaveLocator locator;

    /// A name for the slave, unique in the execution
    std::string name;

    /// Default constructor
    AddedSlave() DSB_NOEXCEPT { }

    /// Constructor which sets the `#locator` and `#name` fields.
    AddedSlave(dsb::net::SlaveLocator locator_, std::string name_)
        : locator{std::move(locator_)}, name(std::move(name_))
    { }
};

/**
\brief  Used in `ExecutionManager::Reconfigure()` to specify variable value
        and connection changes.
*/
struct SlaveConfig
{
    dsb::model::SlaveID slaveID;
    std::vector<dsb::model::VariableSetting> variableSettings;

    SlaveConfig() DSB_NOEXCEPT { }
    SlaveConfig(
        dsb::model::SlaveID slaveID_,
        std::vector<dsb::model::VariableSetting> variableSettings_)
        : slaveID{slaveID_}
        , variableSettings(variableSettings_)
    { }
};


class ExecutionManagerPrivate;


/// Manages and coordinates all participants in an execution.
class ExecutionManager : boost::noncopyable
{
public:
    /**
    \brief  Constructs an object which manages an execution.

    \param [in] reactor
        The dsb::net::Reactor object used for communication.
    \param [in] executionName
        A (preferably unique) name for the execution.
    \param [in] startTime
        The start time of the simulation.
    \param [in] maxTime
        The maximum simulation time.
    */
    ExecutionManager(
        dsb::net::Reactor& reactor,
        const std::string& executionName,
        dsb::model::TimePoint startTime = 0.0,
        dsb::model::TimePoint maxTime = dsb::model::ETERNITY);

    ~ExecutionManager();

    /// Completion handler type for Reconstitute().
    typedef std::function<void(const std::error_code&)> ReconstituteHandler;

    /// Per-slave completion handler type for Reconstitute()
    typedef std::function<void(const std::error_code&, dsb::model::SlaveID, std::size_t)>
        SlaveReconstituteHandler;

    /**
    \brief  Adds new slaves to the execution.

    The naming of this function reflects the fact that, in a future version,
    it is intended to also support *removing* slaves from an execution, and
    not just adding.

    \param [in] slavesToAdd
        A list of slaves to add.  If empty, `onComplete` is called
        immediately and the function returns without doing anything else.
        Contains information about the slaves on output.
    \param [in] commTimeout
        The communications timeout used to detect loss of communication
        with the slaves.
    \param [in] onComplete
        Handler callback which is called when the entire operation is
        complete.
    \param [in] onSlaveComplete
        Handler callback which is called once for each slave, and which
        receives the slave's assigned ID number (or an error code, in case
        the operation failed).  It also receives a `size_t` variable which
        contains the slave's index in the `slavesToAdd` vector.
    */
    void Reconstitute(
        const std::vector<AddedSlave>& slavesToAdd,
        std::chrono::milliseconds commTimeout,
        ReconstituteHandler onComplete,
        SlaveReconstituteHandler onSlaveComplete = SlaveReconstituteHandler{});

    /// Completion handler type for Reconfigure().
    typedef std::function<void(const std::error_code&)> ReconfigureHandler;

    /// Per-slave completion handler type for Reconfigure()
    typedef std::function<void(const std::error_code&, dsb::model::SlaveID, std::size_t)>
        SlaveReconfigureHandler;

    /**
    \brief  Sets the values of and/or connects variables.

    When a connection is made between an output variable and an input
    variable, or such a connection is to be broken, this is specified in
    the `SlaveConfig` object for the slave which owns the *input* variable.

    \param [in] slaveConfigs
        A list of slave configuration change specifications, at most one
        entry per slave.
    \param [in] commTimeout
        The communications timeout used to detect loss of communication
        with the slaves.
    \param [in] onComplete
        Handler callback which is called when the entire operation is
        complete.
    \param [in] onSlaveComplete
        Handler callback which is called once for each slave, and which
        receives an error code in case of error.  It also receives the
        slave ID and a `size_t` variable which contains the slave's index
        in the `slavesToAdd` vector.
    */
    void Reconfigure(
        const std::vector<SlaveConfig>& slaveConfigs,
        std::chrono::milliseconds commTimeout,
        ReconfigureHandler onComplete,
        SlaveReconfigureHandler onSlaveComplete = SlaveReconfigureHandler{});

    /// Completion handler type for the Step() function.
    typedef std::function<void(const std::error_code&)> StepHandler;

    /// Completion handler type for the Step() function of individual slaves.
    typedef std::function<void(const std::error_code&, dsb::model::SlaveID)>
        SlaveStepHandler;

    /// Steps the simulation forward.
    void Step(
        dsb::model::TimeDuration stepSize,
        std::chrono::milliseconds timeout,
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
        std::chrono::milliseconds timeout,
        AcceptStepHandler onComplete,
        SlaveAcceptStepHandler onSlaveAcceptStepComplete = nullptr);

    /// Terminates the entire execution and all associated slaves.
    void Terminate();

    /// Gets the name of the slave with the given ID.
    const std::string& SlaveName(dsb::model::SlaveID id) const;

private:
    std::unique_ptr<ExecutionManagerPrivate> m_private;
};


}} // namespace
#endif // header guard
