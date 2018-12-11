/**
\file
\brief  Defines the coral::slave::LoggingInstance class.
\copyright
    Copyright 2013-present, SINTEF Ocean.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifndef CORAL_SLAVE_LOGGING_HPP_INCLUDED
#define CORAL_SLAVE_LOGGING_HPP_INCLUDED

#include <fstream>
#include <memory>
#include <string>

#include <coral/slave/instance.hpp>


namespace coral
{
namespace slave
{


/// A slave instance wrapper that logs variable values to a file.
class LoggingInstance : public Instance
{
public:
    /**
    \brief  Constructs a LoggingInstance that wraps the given slave
            instance and adds logging to it.

    \param [in] instance
        The slave instance to be wrapped by this one.
    \param [in] outputFilePrefix
        A directory and prefix for a CSV output file.  An execution- and
        slave-specific name as well as a ".csv" extension will be appended
        to this name.  If no prefix is required, and the string only
        contains a directory name, it should end with a directory separator
        (a slash).
    */
    explicit LoggingInstance(
        std::shared_ptr<Instance> instance,
        const std::string& outputFilePrefix = std::string{});

    // slave::Instance methods.
    coral::model::SlaveTypeDescription TypeDescription() const override;
    void Setup(
        const std::string& slaveName,
        const std::string& executionName,
        coral::model::TimePoint startTime,
        coral::model::TimePoint stopTime,
        bool adaptiveStepSize,
        double relativeTolerance) override;
    void StartSimulation() override;
    void EndSimulation() override;
    bool DoStep(coral::model::TimePoint currentT, coral::model::TimeDuration deltaT) override;
    double GetRealVariable(coral::model::VariableID variable) const override;
    int GetIntegerVariable(coral::model::VariableID variable) const override;
    bool GetBooleanVariable(coral::model::VariableID variable) const override;
    std::string GetStringVariable(coral::model::VariableID variable) const override;
    bool SetRealVariable(coral::model::VariableID variable, double value) override;
    bool SetIntegerVariable(coral::model::VariableID variable, int value) override;
    bool SetBooleanVariable(coral::model::VariableID variable, bool value) override;
    bool SetStringVariable(coral::model::VariableID variable, const std::string& value) override;

private:
    std::shared_ptr<Instance> m_instance;
    std::string m_outputFilePrefix;
    std::ofstream m_outputStream;
};


}} // namespace
#endif // header guard
