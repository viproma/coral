/**
\file
\brief FMI 1.0 slave provider support.
*/
#ifndef DSB_FMI_SLAVE_TYPE_HPP
#define DSB_FMI_SLAVE_TYPE_HPP

#include <functional>
#include <string>

#include "fmilib.h"
#include "fmilibcpp/fmi1/Fmu.hpp"

#include "dsb/domain/slave_provider.hpp"
#include "dsb/execution/locator.hpp"
#include "dsb/fmi.hpp"
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

    ~FmiSlaveType();

    // Implementations of ISlaveType methods.
    std::string Name() const override;
    std::string Uuid() const override;
    std::string Description() const override;
    std::string Author() const override;
    std::string Version() const override;
    size_t VariableCount() const override;
    virtual dsb::model::Variable Variable(size_t index) const override;
    virtual bool InstantiateAndConnect(
        dsb::model::SlaveID slaveID,
        const dsb::execution::Locator& executionLocator) override;
    virtual std::string InstantiationFailureDescription() const override;

private:
    const std::string m_fmuPath;
    const SlaveStarter m_slaveStarterFunction;
    dsb::util::TempDir m_unzipDir;
    std::shared_ptr<fmilib::fmi1::Fmu> m_fmu;
    fmi1_import_variable_list_t* m_varList;
    std::string m_instantiationFailureDescription;
};


}}      // namespace
#endif  // header guard
