/**
\file
\brief Slave (instance) functionality.
*/
#ifndef DSB_EXECUTION_SLAVE_HPP
#define DSB_EXECUTION_SLAVE_HPP

#include <chrono>
#include <memory>
#include <stdexcept>
#include "boost/noncopyable.hpp"
#include "dsb/config.h"
#include "dsb/model.hpp"


namespace dsb
{

// Forward declarations to avoid dependencies on internal classes.
namespace bus { class SlaveAgent; }
namespace comm { class Reactor; }

namespace execution
{


/// An interface for classes that represent slave instances.
class ISlaveInstance : boost::noncopyable
{
public:
    /**
    \brief  Performs pre-simulation setup and initialisation.

    This function is called when the slave has connected to an execution.
    The arguments `startTime` and `stopTime` represent the time interval inside
    which the slave's model equations are required to be valid.  (In other
    words, it is guaranteed that DoStep() will never be called with a time point
    outside this interval.)  If the slave is unable to meet this requirement,
    the function must return `false`, and the simulation will not be started.

    \param [in] startTime   The earliest possible time point for the simulation.
    \param [in] stopTime    The latest possible time point for the simulation.
    \return `true` if the slave is ready to perform a simulation in the given
        time interval, `false` otherwise.
    */
    virtual bool Setup(
        dsb::model::TimePoint startTime,
        dsb::model::TimePoint stopTime) = 0;

    /// Returns an object that describes the slave type.
    virtual const dsb::model::SlaveTypeDescription& TypeDescription() const = 0;

    /**
    \brief  Returns the value of a real variable.
    \throws std::logic_error if there is no real variable with the given ID.
    */
    virtual double GetRealVariable(dsb::model::VariableID variable) const = 0;

    /**
    \brief  Returns the value of an integer variable.
    \throws std::logic_error if there is no integer variable with the given ID.
    */
    virtual int GetIntegerVariable(dsb::model::VariableID variable) const = 0;

    /**
    \brief  Returns the value of a boolean variable.
    \throws std::logic_error if there is no boolean variable with the given ID.
    */
    virtual bool GetBooleanVariable(dsb::model::VariableID variable) const = 0;

    /**
    \brief  Returns the value of a string variable.
    \throws std::logic_error if there is no string variable with the given ID.
    */
    virtual std::string GetStringVariable(dsb::model::VariableID variable) const = 0;

    /**
    \brief  Sets the value of a real variable.
    \throws std::logic_error if there is no real variable with the given ID.
    */
    virtual void SetRealVariable(dsb::model::VariableID variable, double value) = 0;

    /**
    \brief  Sets the value of an integer variable.
    \throws std::logic_error if there is no integer variable with the given ID.
    */
    virtual void SetIntegerVariable(dsb::model::VariableID variable, int value) = 0;

    /**
    \brief  Sets the value of a boolean variable.
    \throws std::logic_error if there is no boolean variable with the given ID.
    */
    virtual void SetBooleanVariable(dsb::model::VariableID variable, bool value) = 0;

    /**
    \brief  Sets the value of a string variable.
    \throws std::logic_error if there is no string variable with the given ID.
    */
    virtual void SetStringVariable(dsb::model::VariableID variable, const std::string& value) = 0;

    /**
    \brief  Performs model calculations for the time step which starts at
            the time point `currentT` and has a duration of `deltaT`.

    If this is not the first time step, it can be assumed that the previous
    time step ended at `currentT`.  It can also be assumed that `currentT` is
    greater than or equal to the start time, and `currentT+deltaT` is less than
    or equal to the stop time, specified in the Setup() call.

    \returns `true` if the model calculations for the given time step were
        successfully carried out, or `false` if they were not because the
        time step was too long.  In the latter case the time step may be
        retried (provided that the slave supports storing and restoring state).

    \throws std::runtime_error if the time step could not be carried out for
        some reason (where retrying with a smaller step size will not help).
    */
    virtual bool DoStep(
        dsb::model::TimePoint currentT,
        dsb::model::TimeDuration deltaT) = 0;

    // Virtual destructor to allow deletion through base class reference.
    virtual ~ISlaveInstance() { }
};


class SlaveRunner
{
public:
    SlaveRunner(
        std::shared_ptr<ISlaveInstance> slaveInstance,
        std::string bindURL,
        std::chrono::seconds commTimeout);

    SlaveRunner(SlaveRunner&&) DSB_NOEXCEPT;

    SlaveRunner& operator=(SlaveRunner&&) DSB_NOEXCEPT;

    ~SlaveRunner();

    std::string BoundEndpoint();

    void Run();

private:
    std::shared_ptr<ISlaveInstance> m_slaveInstance;
    std::unique_ptr<dsb::comm::Reactor> m_reactor;
    std::unique_ptr<dsb::bus::SlaveAgent> m_slaveAgent;
};


class TimeoutException : public std::runtime_error
{
public:
    explicit TimeoutException(std::chrono::seconds timeoutDuration)
        : std::runtime_error("Slave timed out due to lack of communication"),
          m_timeoutDuration(timeoutDuration)
    {
    }

    std::chrono::seconds TimeoutDuration() const { return m_timeoutDuration; }

private:
    std::chrono::seconds m_timeoutDuration;
};


}}      // namespace
#endif  // header guard
