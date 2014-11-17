#include <exception>
#include <iostream>
#include <string>
#include <vector>

#include "boost/filesystem.hpp"
#include "boost/lexical_cast.hpp"

#include "dsb/domain/slave_provider.hpp"
#include "dsb/fmi.hpp"
#include "dsb/util.hpp"


void StartSlave(
    dsb::model::SlaveID slaveID,
    const dsb::execution::Locator& executionLocator,
    const std::string& fmuPath)
{
    const auto slaveIdString = boost::lexical_cast<std::string>(slaveID);
    const auto outputFile = "slave_" + slaveIdString + ".csv";
    std::vector<std::string> args;
    args.push_back(slaveIdString);
    args.push_back(executionLocator.SlaveEndpoint());
    args.push_back(executionLocator.VariablePubEndpoint());
    args.push_back(executionLocator.VariableSubEndpoint());
    args.push_back(fmuPath);
    args.push_back(outputFile);

    std::cout << "\nStarting slave " << slaveID << '\n'
        << "  Execution: " << executionLocator.SlaveEndpoint() << '\n'
        << "  FMU      : " << fmuPath << '\n'
        << "  Output   : " << outputFile << '\n'
        << std::flush;
    // TODO: You know what:
    dsb::util::SpawnProcess(
        "C:\\Users\\larky\\Development\\viproma\\dsb\\build32\\src\\slave\\Debug\\slave.exe",
        args);
}


void ScanDirectoryForFMUs(const std::string& directory, std::vector<std::string>& fmuPaths)
{
    namespace fs = boost::filesystem;
    for (auto it = fs::recursive_directory_iterator(directory);
         it != fs::recursive_directory_iterator();
         ++it)
    {
        if (it->path().extension() == ".fmu") {
            fmuPaths.push_back(it->path().string());
        }
    }
}


int main(int argc, const char** argv)
{
try {
    if (argc < 4) {
        const auto self = boost::filesystem::path(argv[0]).stem().string();
        std::cerr << "Usage: " << self << " <report> <info> <fmus...>\n"
                  << "  report   = Slave provider report endpoint (e.g. tcp://myhost:5432)\n"
                  << "  info     = Slave provider info endpoint (e.g. tcp://myhost:5432)\n"
                  << "  fmus     = Paths to FMI1 FMU and/or directories containing FMUs.\n"
                  << "             Directories will be scanned recursively for files with .fmu extensions."
                  << std::endl;
        return 0;
    }
    const auto reportEndpoint = std::string(argv[1]);
    const auto infoEndpoint = std::string(argv[2]);
    std::vector<std::string> fmuPaths;
    for (auto i = 3; i < argc; ++i) {
        if (boost::filesystem::is_directory(argv[i])) {
            ScanDirectoryForFMUs(argv[i], fmuPaths);
        } else {
            fmuPaths.push_back(argv[i]);
        }
    }

    std::vector<std::unique_ptr<dsb::domain::ISlaveType>> fmus;
    std::vector<dsb::domain::ISlaveType*> fmuPtrs;
    for (auto it = fmuPaths.begin(); it != fmuPaths.end(); ++it) {
        fmus.push_back(dsb::fmi::MakeSlaveType(*it, StartSlave));
        fmuPtrs.push_back(fmus.back().get());
        std::clog << "FMU loaded: " << *it << std::endl;
    }
    std::clog << fmus.size() << " FMUs loaded" << std::endl;
    dsb::domain::SlaveProvider(reportEndpoint, infoEndpoint, fmuPtrs);
} catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
}
return 0;
}
