#ifndef DSB_FMILIB_FMU1_HPP
#define DSB_FMILIB_FMU1_HPP

#include <memory>
#include <string>
#include "boost/filesystem/path.hpp"
#include "fmilib.h"

#include "dsb/config.h"
#include "dsb/fmilib/fmu.hpp"


namespace dsb
{
namespace fmilib
{


/// A reference to an imported FMU that implements FMI 1.0.
class Fmu1 : public dsb::fmilib::Fmu
{
public:
    /**
    \brief  Constructor.

    It is recommended to use dsb::fmilib::ImportContext::Import() to import FMUs.
    That said, this constructor may be used if the FMU has already been
    unzipped into `dirName`, and it is known that it implements FMI 1.0

    \param [in] context     The library context.
    \param [in] dirName     The directory into which the contents of the FMU
                            have been unzipped.
    */
    Fmu1(std::shared_ptr<dsb::fmilib::ImportContext> context,
        const boost::filesystem::path& dirName);

    ~Fmu1();

    /// Implements dsb::fmilib::Fmu::FmiVersion()
    dsb::fmilib::FmiVersion FmiVersion() const DSB_FINAL override;

    /// Implements dsb::fmilib::Fmu::ModelName()
    std::string ModelName() const DSB_FINAL override;

    /// Implements dsb::fmilib::Fmu::GUID()
    std::string GUID() const DSB_FINAL override;

    /// Implements dsb::fmilib::Fmu::Description()
    std::string Description() const DSB_FINAL override;

    /// Implements dsb::fmilib::Fmu::Author()
    std::string Author() const DSB_FINAL override;

    /// Implements dsb::fmilib::Fmu::ModelVersion()
    std::string ModelVersion() const DSB_FINAL override;

    /// Implements dsb::fmilib::Fmu::GenerationTool()
    std::string GenerationTool() const DSB_FINAL override;

    /// Returns the C API handle for this FMU.
    fmi1_import_t* Handle() const;

private:
    fmi1_import_t* m_handle;

#ifdef _WIN32
    // Workaround for VIPROMA-42 (Resources and Binary\<Platform> fmu folders
    // should be added to search path).
    class AdditionalDllDirectory;
    std::unique_ptr<AdditionalDllDirectory> m_additionalDllDirectory;
#endif
};


}} // namespace
#endif // header guard
