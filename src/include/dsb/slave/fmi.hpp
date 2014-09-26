#ifndef DSB_SLAVE_FMI_HPP
#define DSB_SLAVE_FMI_HPP

#include "boost/filesystem.hpp"
#include "fmilibcpp/fmi1/Fmu.hpp"
#include "dsb/bus/slave_agent.hpp"


namespace dsb
{
namespace slave
{

    
// An RAII object that creates a unique directory on construction,
// and recursively deletes it again on destruction.
class TempDir
{
public:
    TempDir();
    ~TempDir();
    const boost::filesystem::path& Path() const;
private:
    boost::filesystem::path m_path;
};


class FmiSlaveInstance : public dsb::bus::ISlaveInstance
{
public:
    FmiSlaveInstance(const std::string& fmuPath);
    ~FmiSlaveInstance();

    void Setup(double startTime, double stopTime) override;
    std::vector<dsb::bus::VariableInfo> Variables() override;
    double GetRealVariable(unsigned varRef) override;
    int GetIntegerVariable(unsigned varRef) override;
    bool GetBooleanVariable(unsigned varRef) override;
    std::string GetStringVariable(unsigned varRef) override;
    void SetRealVariable(unsigned varRef, double value) override;
    void SetIntegerVariable(unsigned varRef, int value) override;
    void SetBooleanVariable(unsigned varRef, bool value) override;
    void SetStringVariable(unsigned varRef, const std::string& value) override;
    bool DoStep(double currentT, double deltaT) override;

private:
    TempDir m_fmuDir;
    std::shared_ptr<fmilib::fmi1::Fmu> m_fmu;
    bool m_initializing;
    double m_startTime, m_stopTime;

    std::vector<fmi1_value_reference_t> m_fmiValueRefs;
    std::vector<dsb::bus::VariableInfo> m_variables;
};


}}      // namespace
#endif  // header guard
