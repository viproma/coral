#include <exception>
#include <fstream>
#include <iostream>

#include "boost/filesystem/path.hpp"
#include "boost/lexical_cast.hpp"

#include "dsb/execution/slave.hpp"
#include "dsb/fmi.hpp"


int main(int argc, const char** argv)
{
try {
    if (argc < 6) {
        const auto self = boost::filesystem::path(argv[0]).stem().string();
        std::cerr << "Slave demonstrator.\n"
                  << "This program is designed to be run by a slave provider, and should normally not be run manually.\n\n"
                  << "Usage: " << self << " <id> <control> <data pub> <data sub> <fmu path> [output file]\n"
                  << "  id          = a number in the range 1 - 65535\n"
                  << "  control     = Control socket endpoint (e.g. tcp://myhost:5432)\n"
                  << "  data pub    = Publisher socket endpoint\n"
                  << "  data sub    = Subscriber socket endpoint\n"
                  << "  fmu path    = Path to FMI1 FMU\n"
                  << "  output file = Name of output file to write.\n"
                  << "\n"
                  << "The output file will be written in CSV format.  If no output file name is given, no output will be written."
                  << std::endl;
        return 0;
    }
    const auto id = boost::lexical_cast<uint16_t>(argv[1]);
    if (id == 0) {
        std::cerr << "0 is not a valid slave ID" << std::endl;
        return 1;
    }
    const auto controlEndpoint = std::string(argv[2]);
    const auto dataPubEndpoint = std::string(argv[3]);
    const auto dataSubEndpoint = std::string(argv[4]);
    const auto fmuPath = std::string(argv[5]);
    std::clog << "DSB slave running FMU: " << fmuPath << std::endl;

    std::ofstream csvOutput;
    if (argc > 6)
    {
        csvOutput.open(argv[6], std::ios_base::out | std::ios_base::trunc
#ifdef _WIN32
            , _SH_DENYWR // Don't let other processes/threads write to the file
#endif
        );
        if (!csvOutput) {
            std::cerr << "Error opening output file for writing: " << argv[6] << std::endl;
            return 1;
        }
        std::clog << "Output printed to: " << argv[6] << std::endl;
    }

    auto fmiSlave = dsb::fmi::MakeSlaveInstance(
        fmuPath, csvOutput.is_open() ? &csvOutput : nullptr);
    dsb::execution::RunSlave(id, controlEndpoint, dataPubEndpoint, dataSubEndpoint, *fmiSlave);
    std::cout << "Slave shut down normally" << std::endl;

} catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 2;
}
return 0;
}
