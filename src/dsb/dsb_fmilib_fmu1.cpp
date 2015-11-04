#include "dsb/fmilib/fmu1.hpp"

#ifdef _WIN32
#   define NOMINMAX
#   include <Windows.h>
#endif

#include <cassert>
#include <cstdlib>
#include <stdexcept>

#include "boost/thread/lock_guard.hpp"
#include "boost/thread/mutex.hpp"

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

// Given "path/to/fmu", returns "path/to/fmu/binaries/<platform>"
boost::filesystem::path FMUBinariesDir(const boost::filesystem::path& baseDir)
{
#ifdef _WIN64
    const auto platformSubdir = L"win64";
#else
    const auto platformSubdir = L"win32";
#endif // _WIN64
    return baseDir / L"binaries" / platformSubdir;
}


/*
This class adds the FMU's binaries/<platform> directory to Windows' DLL search
path (by adding it to the PATH environment variable for the current process),
and removes it again on desctruction.  This solves a problem where Windows
was unable to locate some DLLs that are indirectly loaded.  Specifically, the
problem has been observed when the main FMU model DLL runs Java code (through
JNI), and that Java code loaded a second DLL, which again was linked to further
DLLs.  The latter were located in the binaries/<platform> directory, but were
not found by the dynamic loader because that directory was not in the search
path.

Since environment variables are shared by the entire process, the functions
use a mutex to protect against concurrent access to the PATH variable while
it's being read, modified and written.  (This does not protect against
access by client code, of course, which is a potential source of bugs.
See VIPROMA-67 for more info.)
*/
class dsb::fmilib::Fmu1::AdditionalPath
{
    static const size_t MAX_ENV_VAR_SIZE = 32767;

public:
    AdditionalPath(const boost::filesystem::path& p)
    {
        auto lock = boost::make_lock_guard(m_pathEnvVarMutex);

        WCHAR currentPathZ[MAX_ENV_VAR_SIZE];
        const auto currentPathLen =
            GetEnvironmentVariableW(L"PATH", currentPathZ, MAX_ENV_VAR_SIZE);
        const auto currentPath = std::wstring(currentPathZ, currentPathLen);

        if (currentPathLen > 0) {
            m_addedPath = L";";
        }
        m_addedPath += p.wstring();

        const auto newPath = currentPath + m_addedPath;
        if (!SetEnvironmentVariableW(L"PATH", newPath.c_str())) {
            assert(!"Failed to modify PATH environment variable");
        }
    }

    ~AdditionalPath()
    {
        auto lock = boost::make_lock_guard(m_pathEnvVarMutex);

        WCHAR currentPathZ[MAX_ENV_VAR_SIZE];
        const auto currentPathLen =
            GetEnvironmentVariableW(L"PATH", currentPathZ, MAX_ENV_VAR_SIZE);
        const auto currentPath = std::wstring(currentPathZ, currentPathLen);

        const auto pos = currentPath.find(m_addedPath);
        if (pos < std::wstring::npos) {
            auto newPath = currentPath.substr(0, pos)
                + currentPath.substr(pos + m_addedPath.size());
            if (!SetEnvironmentVariableW(L"PATH", newPath.c_str())) {
                assert(!"Failed to reset PATH environment variable");
            }
        }
    }

private:
    static boost::mutex m_pathEnvVarMutex;
    std::wstring m_addedPath;
};

boost::mutex dsb::fmilib::Fmu1::AdditionalPath::m_pathEnvVarMutex = boost::mutex();

#endif // _WIN32


dsb::fmilib::Fmu1::Fmu1(
    std::shared_ptr<dsb::fmilib::ImportContext> context,
    const boost::filesystem::path& dirName)
    : ::dsb::fmilib::Fmu(context),
      m_handle(fmi1_import_parse_xml(context->Handle(), dirName.string().c_str()))
#ifdef _WIN32
      , m_additionalDllSearchPath(
        std::make_unique<AdditionalPath>(FMUBinariesDir(dirName)))
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
