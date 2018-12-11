/**
\file
\brief Classes for dealing with FMI 1.0 FMUs.
\copyright
    Copyright 2013-present, SINTEF Ocean.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifndef CORAL_FMI_FMU1_HPP
#define CORAL_FMI_FMU1_HPP

#include <memory>
#include <string>
#include <vector>

#include <boost/filesystem/path.hpp>

#include <coral/config.h>
#include <coral/fmi/fmu.hpp>
#include <coral/fmi/importer.hpp>
#include <coral/model.hpp>


// Forward declarations to avoid external dependency on FMI Library
struct fmi1_import_t;
typedef unsigned int fmi1_value_reference_t;


namespace coral
{
namespace fmi
{

#ifdef _WIN32
class AdditionalPath;
#endif
class SlaveInstance1;


/**
\brief  A class which represents an imported FMI 1.0 %FMU.

This class is an implementation of coral::fmi::FMU specialised for FMUs that
implement FMI v1.0.
*/
class FMU1 : public coral::fmi::FMU, public std::enable_shared_from_this<FMU1>
{
private:
    // Only Importer is allowed to instantiate this class.
    friend class Importer;
    FMU1(
        std::shared_ptr<coral::fmi::Importer> importer,
        const boost::filesystem::path& fmuDir);

public:
    // Disable copy and move
    FMU1(const FMU1&) = delete;
    FMU1& operator=(const FMU1&) = delete;
    FMU1(FMU1&&) = delete;
    FMU1& operator=(FMU1&&) = delete;

    ~FMU1();

    // coral::fmi::FMU methods
    coral::fmi::FMIVersion FMIVersion() const override;
    const coral::model::SlaveTypeDescription& Description() const override;
    std::shared_ptr<coral::fmi::SlaveInstance> InstantiateSlave() override;
    std::shared_ptr<coral::fmi::Importer> Importer() const override;

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
        the variable list in the coral::model::SlaveTypeDescription returned by
        Description().
    \throws std::out_of_range
        If there is no variable with the given ID.
    */
    fmi1_value_reference_t FMIValueReference(coral::model::VariableID variable)
        const;

    /// Returns the underlying C API handle (for FMI Library)
    fmi1_import_t* FmilibHandle() const;

private:
    std::shared_ptr<coral::fmi::Importer> m_importer;
    boost::filesystem::path m_dir;

    fmi1_import_t* m_handle;
    std::unique_ptr<coral::model::SlaveTypeDescription> m_description;
    std::vector<fmi1_value_reference_t> m_valueReferences;
    std::vector<std::weak_ptr<SlaveInstance1>> m_instances;

#ifdef _WIN32
    // Workaround for VIPROMA-67 (FMU DLL search paths on Windows).
    std::unique_ptr<AdditionalPath> m_additionalDllSearchPath;
#endif
};


/// An FMI 1.0 co-simulation slave instance.
class SlaveInstance1 : public coral::fmi::SlaveInstance
{
private:
    // Only FMU1 is allowed to instantiate this class.
    friend std::shared_ptr<SlaveInstance1> coral::fmi::FMU1::InstantiateSlave1();
    SlaveInstance1(std::shared_ptr<coral::fmi::FMU1> fmu);

public:
    // Disable copy and move.
    SlaveInstance1(const SlaveInstance1&) = delete;
    SlaveInstance1& operator=(const SlaveInstance1&) = delete;
    SlaveInstance1(SlaveInstance1&&) = delete;
    SlaveInstance1& operator=(SlaveInstance1&&) = delete;

    ~SlaveInstance1() noexcept;

    // coral::slave::Instance methods
    coral::model::SlaveTypeDescription TypeDescription() const override;

    void Setup(
        const std::string& slaveName,
        const std::string& executionName,
        coral::model::TimePoint startTime,
        coral::model::TimePoint stopTime,
        bool adaptiveStepSize,
        double relativeTolerance) override;

    void StartSimulation() override;
    void EndSimulation() override;

    bool DoStep(coral::model::TimePoint currentT, coral::model::TimeDuration deltaT) override;

    double GetRealVariable(coral::model::VariableID variable) const override;
    int GetIntegerVariable(coral::model::VariableID variable) const override;
    bool GetBooleanVariable(coral::model::VariableID variable) const override;
    std::string GetStringVariable(coral::model::VariableID variable) const override;

    bool SetRealVariable(coral::model::VariableID variable, double value) override;
    bool SetIntegerVariable(coral::model::VariableID variable, int value) override;
    bool SetBooleanVariable(coral::model::VariableID variable, bool value) override;
    bool SetStringVariable(coral::model::VariableID variable, const std::string& value) override;

    // coral::fmi::SlaveInstance methods
    std::shared_ptr<coral::fmi::FMU> FMU() const override;

    /// Returns the same object as FMU(), only statically typed as an FMU1.
    std::shared_ptr<coral::fmi::FMU1> FMU1() const;

    /// Returns the underlying C API handle (for FMI Library)
    fmi1_import_t* FmilibHandle() const;

private:
    std::shared_ptr<coral::fmi::FMU1> m_fmu;
    fmi1_import_t* m_handle;

    bool m_setupComplete = false;
    bool m_simStarted = false;

    std::string m_instanceName;
    coral::model::TimePoint m_startTime = 0.0;
    coral::model::TimePoint m_stopTime  = coral::model::ETERNITY;
};


}} // namespace
#endif // header guard
