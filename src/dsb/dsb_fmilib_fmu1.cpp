#include "dsb/fmilib/fmu1.hpp"

#define NOMINMAX
#include <Windows.h>

#include <cassert>
#include <cstdlib>
#include <stdexcept>

#include "dsb/compat_helpers.hpp"
#include "dsb/fmilib/fmu.hpp"
#include "dsb/fmilib/importcontext.hpp"
#include "dsb/util.hpp"


namespace
{
    void StepFinishedPlaceholder(fmi1_component_t, fmi1_status_t)
    {
        assert (!"stepFinished was called, but synchronous FMUs are currently not supported");
    }
}


#ifdef _WIN32
/*
This class adds the FMU's binaries/<platform> directory to Windows' DLL search
path, and removes it again on desctruction.  This solves a problem where Windows
was unable to locate some DLLs that are indirectly loaded.  Specifically, the
problem has been observed when the main FMU model DLL runs Java code (through
JNI), and that Java code loaded a second DLL, which again was linked to further
DLLs.  The latter were located in the binaries/<platform> directory, but were
not found by the dynamic loader because that directory was not in the search
path.

This requires the use of a few functions that were added with Windows security
update KB2533623.  If these are not present, the directory will simply not be
added to the DLL search path.  For details, see:
https://msdn.microsoft.com/en-us/library/windows/desktop/hh310513%28v=vs.85%29.aspx
*/
#ifndef LOAD_LIBRARY_SEARCH_DEFAULT_DIRS
#	define LOAD_LIBRARY_SEARCH_DEFAULT_DIRS 0x00001000
#endif

class dsb::fmilib::Fmu1::AdditionalDllDirectory
{
    typedef void* DLL_DIRECTORY_COOKIE;
    typedef DLL_DIRECTORY_COOKIE (WINAPI * AddDllDirectoryT)(PCWSTR);
    typedef BOOL (WINAPI * RemoveDllDirectoryT)(DLL_DIRECTORY_COOKIE);
    typedef BOOL (WINAPI * SetDefaultDllDirectoriesT)(DWORD);

public:
    AdditionalDllDirectory(const boost::filesystem::path& baseDir)
        : m_binariesDir(nullptr)
    {
        assert(!baseDir.empty());
        if (auto kernel32Handle = GetModuleHandleA("kernel32")) {
            auto addDllDirectory = reinterpret_cast<AddDllDirectoryT>(
                GetProcAddress(kernel32Handle, "AddDllDirectory"));
            m_removeDllDirectory = reinterpret_cast<RemoveDllDirectoryT>(
                GetProcAddress(kernel32Handle, "RemoveDllDirectory"));
            auto setDefaultDllDirectories = reinterpret_cast<SetDefaultDllDirectoriesT>(
                GetProcAddress(kernel32Handle, "SetDefaultDllDirectories"));

            if (addDllDirectory && m_removeDllDirectory && setDefaultDllDirectories) {
                if (!setDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS)) {
                    assert(!"SetDefaultDllDirectories() failed");
                }
#ifdef _WIN64
                const auto platformSubdir = L"win64";
#else
                const auto platformSubdir = L"win32";
#endif // _WIN64
                const auto fmuBinariesDir = baseDir / L"binaries" / platformSubdir;
                m_binariesDir = addDllDirectory(fmuBinariesDir.c_str());
                assert(m_binariesDir && "AddDllDirectory() failed");
            }
        }
    }

    ~AdditionalDllDirectory()
    {
        if (m_binariesDir) m_removeDllDirectory(m_binariesDir);
    }

private:
    AdditionalDllDirectory(const AdditionalDllDirectory&);
    AdditionalDllDirectory& operator=(const AdditionalDllDirectory&);

    RemoveDllDirectoryT m_removeDllDirectory;
    DLL_DIRECTORY_COOKIE m_binariesDir;
};
#endif // _WIN32


dsb::fmilib::Fmu1::Fmu1(std::shared_ptr<dsb::fmilib::ImportContext> context,
                       const boost::filesystem::path& dirName)
    : ::dsb::fmilib::Fmu(context),
      m_handle(fmi1_import_parse_xml(context->Handle(), dirName.string().c_str()))
#ifdef _WIN32
      , m_additionalDllDirectory(std::make_unique<AdditionalDllDirectory>(dirName))
#endif
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
