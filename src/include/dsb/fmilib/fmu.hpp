#ifndef DSB_FMILIB_FMU_HPP
#define DSB_FMILIB_FMU_HPP

#include <memory>
#include <string>


namespace dsb
{
namespace fmilib
{


class ImportContext;


/**
\brief  Constants that refer to FMI version numbers.
\see dsb::fmilib::Fmu::FmiVersion()
*/
enum FmiVersion
{
    /// FMI 1.0
    kFmiVersion1_0 = 100,

    /// FMI 2.0
    kFmiVersion2_0 = 200,
};


/**
\brief  A reference to an imported FMU.

This is an abstract class which only defines the functions that are common
to both FMI 1.0 and FMI 2.0.  Use dsb::fmilib::ImportContext::Import() to import
an FMU and create an dsb::fmilib::Fmu object.
*/
class Fmu
{
public:
    /// The library context that was used to import the FMU.
    std::shared_ptr<dsb::fmilib::ImportContext> Context() const;

    /// Which FMI standard version is used in this FMU.
    virtual dsb::fmilib::FmiVersion FmiVersion() const = 0;

    /// The model name.
    virtual std::string ModelName() const = 0;

    /// The FMU GUID.
    virtual std::string GUID() const = 0;

    /// The FMU description.
    virtual std::string Description() const = 0;

    /// The FMU author.
    virtual std::string Author() const = 0;

    /// The FMU version.
    virtual std::string ModelVersion() const = 0;

    /// The FMU generation tool.
    virtual std::string GenerationTool() const = 0;

    virtual ~Fmu() { }

protected:
    /**
    \brief  Constructor which stores a reference to the context.

    The purpose is to keep the context "alive" for the lifetime of the Fmu
    object.
    */
    Fmu(std::shared_ptr<dsb::fmilib::ImportContext> context);

private:
    std::shared_ptr<dsb::fmilib::ImportContext> m_context;
};


}} // namespace
#endif // header guard
