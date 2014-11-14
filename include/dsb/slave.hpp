#ifndef DSB_SLAVE_HPP
#define DSB_SLAVE_HPP

#include "boost/noncopyable.hpp"
#include "dsb/model.hpp"


namespace dsb
{
namespace slave
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

    /**
    \brief  Returns the value of a real variable.
    \throws std::logic_error if there is no real variable with the given ID.
    */
    virtual double GetRealVariable(dsb::model::VariableID variable) = 0;

    /**
    \brief  Returns the value of an integer variable.
    \throws std::logic_error if there is no integer variable with the given ID.
    */
    virtual int GetIntegerVariable(dsb::model::VariableID variable) = 0;

    /**
    \brief  Returns the value of a boolean variable.
    \throws std::logic_error if there is no boolean variable with the given ID.
    */
    virtual bool GetBooleanVariable(dsb::model::VariableID variable) = 0;

    /**
    \brief  Returns the value of a string variable.
    \throws std::logic_error if there is no string variable with the given ID.
    */
    virtual std::string GetStringVariable(dsb::model::VariableID variable) = 0;

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


}}      // namespace
#endif  // header guard
