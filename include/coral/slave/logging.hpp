/**
\file
\brief  Defines the coral::slave::LoggingInstance class.
*/
#ifndef CORAL_SLAVE_LOGGING_HPP_INCLUDED
#define CORAL_SLAVE_LOGGING_HPP_INCLUDED

#include <fstream>
#include <memory>
#include <string>

#include "coral/slave/instance.hpp"


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
    bool Setup(
        coral::model::TimePoint startTime,
        coral::model::TimePoint stopTime,
        const std::string& executionName,
        const std::string& slaveName) override;
    const coral::model::SlaveTypeDescription& TypeDescription() const override;
    double GetRealVariable(coral::model::VariableID variable) const override;
    int GetIntegerVariable(coral::model::VariableID variable) const override;
    bool GetBooleanVariable(coral::model::VariableID variable) const override;
    std::string GetStringVariable(coral::model::VariableID variable) const override;
    void SetRealVariable(coral::model::VariableID variable, double value) override;
    void SetIntegerVariable(coral::model::VariableID variable, int value) override;
    void SetBooleanVariable(coral::model::VariableID variable, bool value) override;
    void SetStringVariable(coral::model::VariableID variable, const std::string& value) override;
    bool DoStep(coral::model::TimePoint currentT, coral::model::TimeDuration deltaT) override;

private:
    std::shared_ptr<Instance> m_instance;
    std::string m_outputFilePrefix;
    std::ofstream m_outputStream;
};


}} // namespace
#endif // header guard
