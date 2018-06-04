/*
Copyright 2013-present, SINTEF Ocean.
This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#include <coral/fmi/windows.hpp>
#ifdef _WIN32

#include <Windows.h>

#include <cassert>
#include <cstddef>
#include <mutex>


namespace coral
{
namespace fmi
{

namespace
{
    // Maximum size of environment variables
    static const std::size_t MAX_ENV_VAR_SIZE = 32767;

    // Mutex to protect against concurrent access to PATH
    std::mutex g_pathEnvVarMutex{};
}


AdditionalPath::AdditionalPath(const boost::filesystem::path& p)
{
    std::lock_guard<std::mutex> lock(g_pathEnvVarMutex);

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


AdditionalPath::~AdditionalPath()
{
    std::lock_guard<std::mutex> lock(g_pathEnvVarMutex);

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


boost::filesystem::path FMUBinariesDir(const boost::filesystem::path& baseDir)
{
#ifdef _WIN64
    const auto platformSubdir = L"win64";
#else
    const auto platformSubdir = L"win32";
#endif // _WIN64
    return baseDir / L"binaries" / platformSubdir;
}


}} // namespace
#endif // _WIN32
