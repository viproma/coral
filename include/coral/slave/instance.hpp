/**
\file
\brief  Defines the coral::slave::Instance interface.
\copyright
    Copyright 2013-present, SINTEF Ocean.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifndef CORAL_SLAVE_INSTANCE_HPP
#define CORAL_SLAVE_INSTANCE_HPP

#include <string>
#include <coral/model.hpp>

namespace coral
{
namespace slave
{


/**
\brief  An interface for classes that represent slave instances.

The function call sequence is as follows:

  1. `Setup()`:
        Configure the slave and enter initialisation mode.
  2. `Get...Variable()`, `Set...Variable()`:
        Variable initialisation.  The functions may be called multiple times
        in any order.
  3. `StartSimulation()`:
        End initialisation mode, start simulation.
  4. `DoStep()`, `Get...Variable()`, `Set...Variable()`:
        Simulation.  The functions may be called multiple times in any order.
  5. `EndSimulation()`:
        End simulation.

Any method may throw an exception, after which the slave instance is considered
to be "broken" and no further method calls will be made.
*/
class Instance
{
public:
    /// Returns an object that describes the slave type.
    virtual coral::model::SlaveTypeDescription TypeDescription() const = 0;

    /**
    \brief  Instructs the slave to perform pre-simulation setup and enter
            initialisation mode.

    This function is called when the slave has been added to an execution.
    The arguments `startTime` and `stopTime` represent the time interval inside
    which the slave's model equations are required to be valid.  (In other
    words, it is guaranteed that DoStep() will never be called with a time point
    outside this interval.)

    \param [in] slaveName
        The name of the slave in the current execution.  May be empty if this
        feature is not used.
    \param [in] executionName
        The name of the current execution.  May be empty if this feature is
        not used.
    \param [in] startTime
        The earliest possible time point for the simulation.
    \param [in] stopTime
        The latest possible time point for the simulation.  May be infinity if
        there is no defined stop time.
    \param [in] adaptiveStepSize
        Whether the step size is being controlled by error estimation.
    \param [in] relativeTolerance
        Only used if `adaptiveStepSize == true`, and then contains the relative
        tolerance of the step size controller.  The slave may then use this for
        error estimation in its internal integrator.
    */
    virtual void Setup(
        const std::string& slaveName,
        const std::string& executionName,
        coral::model::TimePoint startTime,
        coral::model::TimePoint stopTime,
        bool adaptiveStepSize,
        double relativeTolerance) = 0;

    /**
    \brief  Informs the slave that the initialisation stage ends and the
            simulation begins.
    */
    virtual void StartSimulation() = 0;

    /// Informs the slave that the simulation run has ended.
    virtual void EndSimulation() = 0;

    /**
    \brief  Performs model calculations for the time step which starts at
            the time point `currentT` and has a duration of `deltaT`.

    If this is not the first time step, it can be assumed that the previous
    time step ended at `currentT`.  It can also be assumed that `currentT` is
    greater than or equal to the start time, and `currentT+deltaT` is less than
    or equal to the stop time, specified in the Setup() call.

    \returns
        `true` if the model calculations for the given time step were
        successfully carried out, or `false` if they were not because the
        time step was too long.

    \note
        Currently, retrying a failed time step is not supported, but this is
        planned for a future version.
    */
    virtual bool DoStep(
        coral::model::TimePoint currentT,
        coral::model::TimeDuration deltaT) = 0;

    /**
    \brief  Returns the value of a real variable.
    \throws std::logic_error if there is no real variable with the given ID.
    */
    virtual double GetRealVariable(coral::model::VariableID variable) const = 0;

    /**
    \brief  Returns the value of an integer variable.
    \throws std::logic_error if there is no integer variable with the given ID.
    */
    virtual int GetIntegerVariable(coral::model::VariableID variable) const = 0;

    /**
    \brief  Returns the value of a boolean variable.
    \throws std::logic_error if there is no boolean variable with the given ID.
    */
    virtual bool GetBooleanVariable(coral::model::VariableID variable) const = 0;

    /**
    \brief  Returns the value of a string variable.
    \throws std::logic_error if there is no string variable with the given ID.
    */
    virtual std::string GetStringVariable(coral::model::VariableID variable) const = 0;

    /**
    \brief  Sets the value of a real variable.

    \returns
        Whether the value was set successfully.  This could be `false` if,
        for example, the value is out of range.
    \throws std::logic_error
        If there is no real variable with the given ID.
    */
    virtual bool SetRealVariable(coral::model::VariableID variable, double value) = 0;

    /**
    \brief  Sets the value of an integer variable.

    \returns
        Whether the value was set successfully.  This could be `false` if,
        for example, the value is out of range.
    \throws std::logic_error
        If there is no integer variable with the given ID.
    */
    virtual bool SetIntegerVariable(coral::model::VariableID variable, int value) = 0;

    /**
    \brief  Sets the value of a boolean variable.

    \returns
        Whether the value was set successfully.  This could be `false` if,
        for example, the value is out of range.
    \throws std::logic_error
        If there is no boolean variable with the given ID.
    */
    virtual bool SetBooleanVariable(coral::model::VariableID variable, bool value) = 0;

    /**
    \brief  Sets the value of a string variable.

    \returns
        Whether the value was set successfully.  This could be `false` if,
        for example, the value is out of range.
    \throws std::logic_error
        If there is no string variable with the given ID.
    */
    virtual bool SetStringVariable(coral::model::VariableID variable, const std::string& value) = 0;

    // Because it's an interface:
    virtual ~Instance() { }
};


}}
#endif // header guard
