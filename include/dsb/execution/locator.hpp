#ifndef DSB_EXECUTION_LOCATOR
#define DSB_EXECUTION_LOCATOR

#include <string>
#include "boost/chrono.hpp"


namespace dsb
{
namespace execution
{


class Locator
{
public:
    Locator() { }

    Locator(
        const std::string& masterEndpoint,
        const std::string& slaveEndpoint,
        const std::string& variablePubEndpoint,
        const std::string& variableSubEndpoint,
        const std::string& execTerminationEndpoint,
        const std::string& execID,
        boost::chrono::seconds commTimeout);

    const std::string& MasterEndpoint() const;
    const std::string& SlaveEndpoint() const;
    const std::string& VariablePubEndpoint() const;
    const std::string& VariableSubEndpoint() const;
    const std::string& ExecTerminationEndpoint() const;
    const std::string& ExecName() const;
    boost::chrono::seconds CommTimeout() const;

private:
    std::string m_masterEndpoint;
    std::string m_slaveEndpoint;
    std::string m_variablePubEndpoint;
    std::string m_variableSubEndpoint;
    std::string m_execTerminationEndpoint;
    std::string m_execName;

    // The communications timeout for the whole execution.  If no communication
    // happens between participants for this amount of time, everyone is
    // responsible for shutting down themselves.
    // TODO: Re-evaluate whether this should be in a Locator, as it doesn't
    //       strictly have anything to do with location.
    boost::chrono::seconds m_commTimeout;
};


}}      // namespace
#endif  // header guard
