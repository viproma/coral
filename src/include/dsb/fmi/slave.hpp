/**
\file
\brief FMI 1.0 slave implementation.
*/
#ifndef DSB_FMI_SLAVE_HPP
#define DSB_FMI_SLAVE_HPP

#include <memory>
#include <ostream>
#include <string>

#include "dsb/fmilib/fmu1.hpp"
#include "dsb/execution/slave.hpp"
#include "dsb/util.hpp"


namespace dsb
{
namespace fmi
{


/**
\brief A slave instance based on an FMI 1.0 FMU.
*/
class FmiSlaveInstance : public dsb::execution::ISlaveInstance
{
public:
    /**
    \brief  Constructs a new slave instance.

    The files in the FMU will be unpacked to a temporary directory, which will
    be automatically deleted again when the object is destroyed.

    As a *temporary* measure until the DSB supports proper observers, an output
    path/prefix may optionally be provided for logging of variable values to a
    CSV file.

    \param [in] fmuPath
        The FMU file path.
    \param [in] outputFilePrefix
        A directory and prefix for a CSV output file, if logging of variable
        values is desired.  An execution- and slave-specific name as well as
        a ".csv" extension will be appended to this name.  If no prefix is
        required, and the string only contains a directory name, it should
        end with a directory separator (a slash).

    \throws std::runtime_error if `fmuPath` does not refer to an FMU that
        implements FMI 1.0.
    */
    explicit FmiSlaveInstance(
        const std::string& fmuPath,
        const std::string* outputFilePrefix = nullptr);

    ~FmiSlaveInstance();

    // ISlaveInstance methods.
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
    dsb::util::TempDir m_fmuDir;
    std::shared_ptr<dsb::fmilib::Fmu1> m_fmu;
    bool m_initialized;
    dsb::model::TimePoint m_startTime, m_stopTime;

    std::vector<fmi1_value_reference_t> m_fmiValueRefs;
    std::unique_ptr<dsb::model::SlaveTypeDescription> m_typeDescription;

    std::string m_outputFilePrefix;
    std::unique_ptr<std::ostream> m_outputStream;
};


}}      // namespace
#endif  // header guard
