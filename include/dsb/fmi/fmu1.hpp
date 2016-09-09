/**
\file
\brief Classes for dealing with FMI 1.0 FMUs.
*/
#ifndef DSB_FMI_FMU1_HPP
#define DSB_FMI_FMU1_HPP

#include <memory>
#include <string>
#include <vector>

#include "boost/filesystem/path.hpp"

#include "dsb/config.h"
#include "dsb/fmi/fmu.hpp"
#include "dsb/fmi/importer.hpp"
#include "dsb/model.hpp"


// Forward declarations to avoid external dependency on FMI Library
struct fmi1_import_t;
typedef unsigned int fmi1_value_reference_t;


namespace dsb
{
namespace fmi
{

class SlaveInstance1;


/**
\brief  A class which represents an imported FMI 1.0 %FMU.

This class is an implementation of dsb::fmi::FMU specialised for FMUs that
implement FMI v1.0.
*/
class FMU1 : public dsb::fmi::FMU, public std::enable_shared_from_this<FMU1>
{
private:
    // Only Importer is allowed to instantiate this class.
    friend std::shared_ptr<dsb::fmi::FMU> dsb::fmi::Importer::Import(
        const boost::filesystem::path&);
    FMU1(
        std::shared_ptr<dsb::fmi::Importer> importer,
        const boost::filesystem::path& fmuDir);

public:
    // Disable copy and move
    FMU1(const FMU1&) = delete;
    FMU1& operator=(const FMU1&) = delete;
    FMU1(FMU1&&) = delete;
    FMU1& operator=(FMU1&&) = delete;

    ~FMU1();

    // dsb::fmi::FMU methods
    dsb::fmi::FMIVersion FMIVersion() const override;
    const dsb::model::SlaveTypeDescription& Description() const override;
    std::shared_ptr<dsb::fmi::SlaveInstance> InstantiateSlave() override;
    std::shared_ptr<dsb::fmi::Importer> Importer() const override;

    /**
    \brief  Creates a new co-simulation slave instance.

    This is equivalent to InstantiateSlave(), except that the returned object
    is statically typed as an FMI 1.0 slave.
    */
    std::shared_ptr<SlaveInstance1> InstantiateSlave1();

    /// Returns the path to the directory in which this %FMU was unpacked.
    const boost::filesystem::path& Directory() const;

    /**
    \brief  Returns the FMI value reference for the variable with the given ID.

    \param [in] variable
        The ID of a variable.  Valid variable IDs are obtained by inspecting
        the variable list in the dsb::model::SlaveTypeDescription returned by
        Description().
    \throws std::out_of_range
        If there is no variable with the given ID.
    */
    fmi1_value_reference_t FMIValueReference(dsb::model::VariableID variable)
        const;

    /// Returns the underlying C API handle (for FMI Library)
    fmi1_import_t* FmilibHandle() const;

private:
    std::shared_ptr<dsb::fmi::Importer> m_importer;
    boost::filesystem::path m_dir;

    fmi1_import_t* m_handle;
    std::unique_ptr<dsb::model::SlaveTypeDescription> m_description;
    std::vector<fmi1_value_reference_t> m_valueReferences;
    std::vector<std::weak_ptr<SlaveInstance1>> m_instances;

#ifdef _WIN32
    // Workaround for VIPROMA-67 (FMU DLL search paths on Windows).
    class AdditionalPath;
    std::unique_ptr<AdditionalPath> m_additionalDllSearchPath;
#endif
};


/// An FMI 1.0 co-simulation slave instance.
class SlaveInstance1 : public dsb::fmi::SlaveInstance
{
private:
    // Only FMU1 is allowed to instantiate this class.
    friend std::shared_ptr<SlaveInstance1> dsb::fmi::FMU1::InstantiateSlave1();
    SlaveInstance1(std::shared_ptr<dsb::fmi::FMU1> fmu);

public:
    // Disable copy and move.
    SlaveInstance1(const SlaveInstance1&) = delete;
    SlaveInstance1& operator=(const SlaveInstance1&) = delete;
    SlaveInstance1(SlaveInstance1&&) = delete;
    SlaveInstance1& operator=(SlaveInstance1&&) = delete;

    ~SlaveInstance1() DSB_NOEXCEPT;

    // dsb::slave::Instance methods
    const dsb::model::SlaveTypeDescription& TypeDescription() const override;

    bool Setup(
        dsb::model::TimePoint startTime,
        dsb::model::TimePoint stopTime,
        const std::string& executionName,
        const std::string& slaveName) override;

    double GetRealVariable(dsb::model::VariableID variable) const override;
    int GetIntegerVariable(dsb::model::VariableID variable) const override;
    bool GetBooleanVariable(dsb::model::VariableID variable) const override;
    std::string GetStringVariable(dsb::model::VariableID variable) const override;

    void SetRealVariable(dsb::model::VariableID variable, double value) override;
    void SetIntegerVariable(dsb::model::VariableID variable, int value) override;
    void SetBooleanVariable(dsb::model::VariableID variable, bool value) override;
    void SetStringVariable(dsb::model::VariableID variable, const std::string& value) override;

    bool DoStep(dsb::model::TimePoint currentT, dsb::model::TimeDuration deltaT) override;

    // dsb::fmi::SlaveInstance methods
    std::shared_ptr<dsb::fmi::FMU> FMU() const override;

    /// Returns the same object as FMU(), only statically typed as an FMU1.
    std::shared_ptr<dsb::fmi::FMU1> FMU1() const;

    /// Returns the underlying C API handle (for FMI Library)
    fmi1_import_t* FmilibHandle() const;

private:
    std::shared_ptr<dsb::fmi::FMU1> m_fmu;
    fmi1_import_t* m_handle;
    std::string m_instanceName;

    bool m_initialized = false;
    dsb::model::TimePoint m_startTime = 0.0;
    dsb::model::TimePoint m_stopTime  = dsb::model::ETERNITY;
};


}} // namespace
#endif // header guard
