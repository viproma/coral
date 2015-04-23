#include "dsb/fmilib/fmu1.hpp"

#include <cassert>
#include <cstdlib>
#include <stdexcept>

#include "dsb/fmilib/fmu.hpp"
#include "dsb/fmilib/importcontext.hpp"


namespace
{
    void StepFinishedPlaceholder(fmi1_component_t, fmi1_status_t)
    {
        assert (!"stepFinished was called, but synchronous FMUs are currently not supported");
    }
}


dsb::fmilib::Fmu1::Fmu1(std::shared_ptr<dsb::fmilib::ImportContext> context,
                       const std::string& dirName)
    : ::dsb::fmilib::Fmu(context),
      m_handle(fmi1_import_parse_xml(context->Handle(), dirName.c_str()))
{
    if (m_handle == nullptr) {
        throw std::runtime_error(Context()->LastErrorMessage());
    }

    fmi1_callback_functions_t callbacks;
    callbacks.allocateMemory = std::calloc;
    callbacks.freeMemory     = std::free;
    callbacks.logger         = fmi1_log_forwarding;
    callbacks.stepFinished   = StepFinishedPlaceholder;
    // WARNING: Using fmi1_log_forwarding above and 'true' below means that
    //          the library is no longer thread safe.
    if (fmi1_import_create_dllfmu(m_handle, callbacks, true) != jm_status_success) {
        fmi1_import_free(m_handle);
        throw std::runtime_error(Context()->LastErrorMessage());
    }
}

dsb::fmilib::Fmu1::~Fmu1()
{
    fmi1_import_destroy_dllfmu(m_handle);
    fmi1_import_free(m_handle);
}


dsb::fmilib::FmiVersion dsb::fmilib::Fmu1::FmiVersion() const
{
    return dsb::fmilib::kFmiVersion1_0;
}


std::string dsb::fmilib::Fmu1::ModelName() const
{
    return std::string(fmi1_import_get_model_name(m_handle));
}


std::string dsb::fmilib::Fmu1::GUID() const
{
    return std::string(fmi1_import_get_GUID(m_handle));
}


std::string dsb::fmilib::Fmu1::Description() const
{
    return std::string(fmi1_import_get_description(m_handle));
}


std::string dsb::fmilib::Fmu1::Author() const
{
    return std::string(fmi1_import_get_author(m_handle));
}


std::string dsb::fmilib::Fmu1::ModelVersion() const
{
    return std::string(fmi1_import_get_model_version(m_handle));
}


std::string dsb::fmilib::Fmu1::GenerationTool() const
{
    return std::string(fmi1_import_get_generation_tool(m_handle));
}


fmi1_import_t* dsb::fmilib::Fmu1::Handle() const
{
    return m_handle;
}
