#include "dsb/fmi.hpp"

#include "dsb/fmi/slave.hpp"
#include "dsb/fmi/slave_type.hpp"


std::unique_ptr<dsb::domain::ISlaveType> dsb::fmi::MakeSlaveType(
    const boost::filesystem::path& fmu,
    SlaveStarter slaveStarterFunction)
{
    return std::make_unique<FmiSlaveType>(fmu.string(), slaveStarterFunction);
}


std::unique_ptr<dsb::execution::ISlaveInstance> dsb::fmi::MakeSlaveInstance(
    const boost::filesystem::path& fmu,
    const std::string* outputFilePrefix)
{
    return std::make_unique<FmiSlaveInstance>(fmu.string(), outputFilePrefix);
}
