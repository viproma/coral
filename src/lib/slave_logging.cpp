#include "coral/slave/logging.hpp"

#include <cassert>
#include <cerrno>
#include <ios>
#include <stdexcept>

#include "coral/error.hpp"
#include "coral/log.hpp"
#include "coral/util.hpp"


namespace coral
{
namespace slave
{


LoggingInstance::LoggingInstance(
    std::shared_ptr<Instance> instance,
    const std::string& outputFilePrefix)
    : m_instance{instance}
    , m_outputFilePrefix(outputFilePrefix)
{
    if (m_outputFilePrefix.empty()) m_outputFilePrefix = "./";
}


bool LoggingInstance::Setup(
    coral::model::TimePoint startTime,
    coral::model::TimePoint stopTime,
    const std::string& executionName,
    const std::string& slaveName)
{
    const auto ret =
        m_instance->Setup(startTime, stopTime, executionName, slaveName);

    auto outputFileName = m_outputFilePrefix;
    if (executionName.empty()) {
        outputFileName += coral::util::Timestamp();
    } else {
        outputFileName += executionName;
    }
    outputFileName += '_';
    if (slaveName.empty()) {
        outputFileName += TypeDescription().Name() + '_'
            + coral::util::RandomString(6, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
    } else {
        outputFileName += slaveName;
    }
    outputFileName += ".csv";

    CORAL_LOG_TRACE("LoggingInstance: Opening " + outputFileName);
    m_outputStream.open(
        outputFileName,
        std::ios_base::out | std::ios_base::trunc
#ifdef _MSC_VER
        , _SH_DENYWR // Don't let other processes/threads write to the file
#endif
        );
    if (!m_outputStream.is_open()) {
        const int e = errno;
        throw std::runtime_error(coral::error::ErrnoMessage(
            "Error opening file \"" + outputFileName + "\" for writing",
            e));
    }

    m_outputStream << "Time";
    for (const auto& var : TypeDescription().Variables()) {
        m_outputStream << "," << var.Name();
    }
    m_outputStream << std::endl;

    return ret;
}


const coral::model::SlaveTypeDescription& LoggingInstance::TypeDescription() const
{
    return m_instance->TypeDescription();
}


double LoggingInstance::GetRealVariable(coral::model::VariableID varRef) const
{
    return m_instance->GetRealVariable(varRef);
}


int LoggingInstance::GetIntegerVariable(coral::model::VariableID varRef) const
{
    return m_instance->GetIntegerVariable(varRef);
}


bool LoggingInstance::GetBooleanVariable(coral::model::VariableID varRef) const
{
    return m_instance->GetBooleanVariable(varRef);
}


std::string LoggingInstance::GetStringVariable(coral::model::VariableID varRef) const
{
    return m_instance->GetStringVariable(varRef);
}


void LoggingInstance::SetRealVariable(coral::model::VariableID varRef, double value)
{
    m_instance->SetRealVariable(varRef, value);
}


void LoggingInstance::SetIntegerVariable(coral::model::VariableID varRef, int value)
{
    m_instance->SetIntegerVariable(varRef, value);
}


void LoggingInstance::SetBooleanVariable(coral::model::VariableID varRef, bool value)
{
    m_instance->SetBooleanVariable(varRef, value);
}


void LoggingInstance::SetStringVariable(coral::model::VariableID varRef, const std::string& value)
{
    m_instance->SetStringVariable(varRef, value);
}


namespace
{
    void PrintVariable(
        std::ostream& out,
        const coral::model::VariableDescription& varInfo,
        Instance& slaveInstance)
    {
        out << ",";
        switch (varInfo.DataType()) {
            case coral::model::REAL_DATATYPE:
                out << slaveInstance.GetRealVariable(varInfo.ID());
                break;
            case coral::model::INTEGER_DATATYPE:
                out << slaveInstance.GetIntegerVariable(varInfo.ID());
                break;
            case coral::model::BOOLEAN_DATATYPE:
                out << slaveInstance.GetBooleanVariable(varInfo.ID());
                break;
            case coral::model::STRING_DATATYPE:
                out << slaveInstance.GetStringVariable(varInfo.ID());
                break;
            default:
                assert (false);
        }
    }
}


bool LoggingInstance::DoStep(
    coral::model::TimePoint currentT,
    coral::model::TimeDuration deltaT)
{
    const auto ret = m_instance->DoStep(currentT, deltaT);

    m_outputStream << (currentT + deltaT);
    for (const auto& var : TypeDescription().Variables()) {
        PrintVariable(m_outputStream, var, *this);
    }
    m_outputStream << std::endl;

    return ret;
}


}} // namespace
