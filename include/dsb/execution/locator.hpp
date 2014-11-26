#ifndef DSB_EXECUTION_LOCATOR
#define DSB_EXECUTION_LOCATOR

#include <string>


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
        const std::string& execID);

    const std::string& MasterEndpoint() const;
    const std::string& SlaveEndpoint() const;
    const std::string& VariablePubEndpoint() const;
    const std::string& VariableSubEndpoint() const;
    const std::string& ExecTerminationEndpoint() const;
    const std::string& ExecName() const;

private:
    std::string m_masterEndpoint;
    std::string m_slaveEndpoint;
    std::string m_variablePubEndpoint;
    std::string m_variableSubEndpoint;
    std::string m_execTerminationEndpoint;
    std::string m_execName;
};


}}      // namespace
#endif  // header guard
