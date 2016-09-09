#ifndef DSB_SLAVE_LOGGING_HPP_INCLUDED
#define DSB_SLAVE_LOGGING_HPP_INCLUDED

#include <fstream>
#include <memory>
#include <string>

#include "dsb/slave/instance.hpp"


namespace dsb
{
namespace slave
{


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
        dsb::model::TimePoint startTime,
        dsb::model::TimePoint stopTime,
        const std::string& executionName,
        const std::string& slaveName) override;
    const dsb::model::SlaveTypeDescription& TypeDescription() const override;
    double GetRealVariable(dsb::model::VariableID variable) const override;
    int GetIntegerVariable(dsb::model::VariableID variable) const override;
    bool GetBooleanVariable(dsb::model::VariableID variable) const override;
    std::string GetStringVariable(dsb::model::VariableID variable) const override;
    void SetRealVariable(dsb::model::VariableID variable, double value) override;
    void SetIntegerVariable(dsb::model::VariableID variable, int value) override;
    void SetBooleanVariable(dsb::model::VariableID variable, bool value) override;
    void SetStringVariable(dsb::model::VariableID variable, const std::string& value) override;
    bool DoStep(dsb::model::TimePoint currentT, dsb::model::TimeDuration deltaT) override;

private:
    std::shared_ptr<Instance> m_instance;
    std::string m_outputFilePrefix;
    std::ofstream m_outputStream;
};


}} // namespace
#endif // header guard
