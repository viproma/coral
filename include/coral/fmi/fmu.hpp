/**
\file
\brief Defines a version-independent FMU interface.
\copyright
    Copyright 2013-2017, SINTEF Ocean and the Coral contributors.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#ifndef CORAL_FMI_FMU_HPP
#define CORAL_FMI_FMU_HPP

#include <coral/model.hpp>
#include <coral/slave/instance.hpp>


namespace coral
{
namespace fmi
{


/**
\brief  Constants that refer to FMI version numbers.
\see coral::fmilib::Fmu::FmiVersion()
*/
enum class FMIVersion
{
    /// Unknown (or possibly unsupported)
    unknown = 0,

    /// FMI 1.0
    v1_0 = 10000,

    /// FMI 2.0
    v2_0 = 20000,
};


class Importer;
class SlaveInstance;

/**
\brief  An interface for classes that represent imported FMUs.

This is an abstract class which only defines the functions that are common
between different FMI versions.  Use coral::fmi::Importer::Import() to import
an %FMU and create a coral::fmi::FMU object.
*/
class FMU
{
public:
    /// Which FMI standard version is used in this %FMU.
    virtual coral::fmi::FMIVersion FMIVersion() const = 0;

    /// A description of this %FMU.
    virtual const coral::model::SlaveTypeDescription& Description() const = 0;

    /// Creates a co-simulation slave instance of this %FMU.
    virtual std::shared_ptr<SlaveInstance> InstantiateSlave() = 0;

    /// Returns the coral::fmi::Importer which was used to import this %FMU.
    virtual std::shared_ptr<coral::fmi::Importer> Importer() const = 0;

    virtual ~FMU() { }
};


/// An FMI co-simulation slave instance.
class SlaveInstance : public coral::slave::Instance
{
public:
    /// Returns a reference to the %FMU of which this is an instance.
    virtual std::shared_ptr<coral::fmi::FMU> FMU() const = 0;

    virtual ~SlaveInstance() { }
};


}} // namespace
#endif // header guard
