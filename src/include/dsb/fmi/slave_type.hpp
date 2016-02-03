/**
\file
\brief FMI 1.0 slave provider support.
*/
#ifndef DSB_FMI_SLAVE_TYPE_HPP
#define DSB_FMI_SLAVE_TYPE_HPP

#include <chrono>
#include <functional>
#include <string>

#include "fmilib.h"
#include "dsb/fmilib/fmu1.hpp"

#include "dsb/domain/slave_provider.hpp"
#include "dsb/fmi.hpp"
#include "dsb/net.hpp"
#include "dsb/util.hpp"


namespace dsb
{
namespace fmi
{


/**
\brief  A slave type description based on an FMI 1.0 FMU.
*/
class FmiSlaveType : public dsb::domain::ISlaveType
{
public:
    /**
    \brief  Constructs a new slave type description by reading an FMI 1.0 FMU.

    The files in the FMU will be unpacked to a temporary directory, which will
    be automatically deleted again when the object is destroyed.

    \param [in] fmuPath
        The FMU file path.
    \param [in] slaveStarterFunction
        The function that is called by InstantiateAndConnect() to create a new
        instance of this FMU.

    \throws std::runtime_error if `fmuPath` does not refer to an FMU that
        implements FMI 1.0.
    */
    FmiSlaveType(
        const std::string& fmuPath,
        SlaveStarter slaveStarterFunction);

    // Implementations of ISlaveType methods.
    const dsb::model::SlaveTypeDescription& Description() const override;
    virtual bool Instantiate(
        std::chrono::milliseconds timeout,
        dsb::net::SlaveLocator& slaveLocator) override;
    virtual std::string InstantiationFailureDescription() const override;

private:
    const std::string m_fmuPath;
    const SlaveStarter m_slaveStarterFunction;
    dsb::util::TempDir m_unzipDir;
    std::shared_ptr<dsb::fmilib::Fmu1> m_fmu;
    std::unique_ptr<dsb::model::SlaveTypeDescription> m_description;
    std::string m_instantiationFailureDescription;
};


}}      // namespace
#endif  // header guard
