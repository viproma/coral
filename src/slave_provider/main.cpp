#include <exception>
#include <iostream>
#include <string>

#include "boost/filesystem/path.hpp"

#include "dsb/domain/slave_provider.hpp"
#include "dsb/fmi.hpp"


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

    auto fmu = dsb::fmi::MakeSlaveType(fmuPath);
    dsb::domain::SlaveProvider(reportEndpoint, infoEndpoint, *fmu);
} catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
}
return 0;
}
