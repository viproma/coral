/**
\file
\brief Defines a version-independent FMU interface.
*/
#ifndef DSB_FMI_FMU_HPP
#define DSB_FMI_FMU_HPP

#include "dsb/model.hpp"
#include "dsb/slave/instance.hpp"


namespace dsb
{
namespace fmi
{


/**
\brief  Constants that refer to FMI version numbers.
\see dsb::fmilib::Fmu::FmiVersion()
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
between different FMI versions.  Use dsb::fmi::Importer::Import() to import
an %FMU and create a dsb::fmi::FMU object.
*/
class FMU
{
public:
    /// Which FMI standard version is used in this %FMU.
    virtual dsb::fmi::FMIVersion FMIVersion() const = 0;

    /// A description of this %FMU.
    virtual const dsb::model::SlaveTypeDescription& Description() const = 0;

    /// Creates a co-simulation slave instance of this %FMU.
    virtual std::shared_ptr<SlaveInstance> InstantiateSlave() = 0;

    /// Returns the dsb::fmi::Importer which was used to import this %FMU.
    virtual std::shared_ptr<dsb::fmi::Importer> Importer() const = 0;

    virtual ~FMU() { }
};


/// An FMI co-simulation slave instance.
class SlaveInstance : public dsb::slave::Instance
{
public:
    /// Returns a reference to the %FMU of which this is an instance.
    virtual std::shared_ptr<dsb::fmi::FMU> FMU() const = 0;

    virtual ~SlaveInstance() { }
};


}} // namespace
#endif // header guard
