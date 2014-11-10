#include <exception>
#include <iostream>
#include <string>
#include <vector>

#include "boost/filesystem/path.hpp"
#include "boost/lexical_cast.hpp"

#include "dsb/domain/slave_provider.hpp"
#include "dsb/fmi.hpp"
#include "dsb/util.hpp"


void StartSlave(
    dsb::model::SlaveID slaveID,
    const dsb::execution::Locator& executionLocator,
    const std::string& fmuPath)
{
    std::cout << "Starting slave with id=" << slaveID << " by connecting to "
        << executionLocator.SlaveEndpoint() << ", FMU file is: "
        << fmuPath << std::endl;
    const auto slaveIdString = boost::lexical_cast<std::string>(slaveID);
    std::vector<std::string> args;
    args.push_back(slaveIdString);
    args.push_back(executionLocator.SlaveEndpoint());
    args.push_back(executionLocator.VariablePubEndpoint());
    args.push_back(executionLocator.VariableSubEndpoint());
    args.push_back(fmuPath);
    args.push_back("slave_" + slaveIdString + ".csv");
    dsb::util::SpawnProcess(
        "C:\\Users\\larky\\Development\\viproma\\dsb\\build32\\src\\slave\\Debug\\slave.exe",
        args);
}


int main(int argc, const char** argv)
{
try {
    if (argc < 4) {
        const auto self = boost::filesystem::path(argv[0]).stem().string();
        std::cerr << "Usage: " << self << " <report> <info> <fmu path>\n"
                  << "  report   = Slave provider report endpoint (e.g. tcp://myhost:5432)\n"
                  << "  info     = Slave provider info endpoint (e.g. tcp://myhost:5432)\n"
                  << "  fmu path = Path to FMI1 FMU"
                  << std::endl;
        return 0;
    }
    const auto reportEndpoint = std::string(argv[1]);
    const auto infoEndpoint = std::string(argv[2]);
    const auto fmuPath = std::string(argv[3]);

    auto fmu = dsb::fmi::MakeSlaveType(fmuPath, StartSlave);
    dsb::domain::SlaveProvider(reportEndpoint, infoEndpoint, *fmu);
} catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
}
return 0;
}
