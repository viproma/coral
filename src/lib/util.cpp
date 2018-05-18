/*
Copyright 2013-2017, SINTEF Ocean and the Coral contributors.
This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#include <coral/util.hpp>

#ifdef _WIN32
#   include <Windows.h>
#else
#   include <unistd.h>
#endif

#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iomanip>

#include <boost/filesystem.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <boost/random/random_device.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>


void coral::util::EncodeUint16(std::uint16_t source, char target[2])
{
    target[0] = source & 0xFF;
    target[1] = (source >> 8) & 0xFF;
}


void coral::util::EncodeUint32(std::uint32_t source, char target[4])
{
    target[0] = source & 0xFF;
    target[1] = (source >> 8) & 0xFF;
    target[2] = (source >> 16) & 0xFF;
    target[3] = (source >> 24) & 0xFF;
}


void coral::util::EncodeUint64(std::uint64_t source, char target[8])
{
    target[0] = source & 0xFF;
    target[1] = (source >> 8) & 0xFF;
    target[2] = (source >> 16) & 0xFF;
    target[3] = (source >> 24) & 0xFF;
    target[4] = (source >> 32) & 0xFF;
    target[5] = (source >> 40) & 0xFF;
    target[6] = (source >> 48) & 0xFF;
    target[7] = (source >> 56) & 0xFF;
}


std::uint16_t coral::util::DecodeUint16(const char source[2])
{
    return static_cast<std::uint16_t>(static_cast<unsigned char>(source[0]))
        | (static_cast<std::uint16_t>(static_cast<unsigned char>(source[1])) << 8);
}


std::uint32_t coral::util::DecodeUint32(const char source[4])
{
    return static_cast<std::uint32_t>(static_cast<unsigned char>(source[0]))
        | (static_cast<std::uint32_t>(static_cast<unsigned char>(source[1])) << 8)
        | (static_cast<std::uint32_t>(static_cast<unsigned char>(source[2])) << 16)
        | (static_cast<std::uint32_t>(static_cast<unsigned char>(source[3])) << 24);
}


std::uint64_t coral::util::DecodeUint64(const char source[8])
{
    return static_cast<std::uint64_t>(static_cast<unsigned char>(source[0]))
        | (static_cast<std::uint64_t>(static_cast<unsigned char>(source[1])) << 8)
        | (static_cast<std::uint64_t>(static_cast<unsigned char>(source[2])) << 16)
        | (static_cast<std::uint64_t>(static_cast<unsigned char>(source[3])) << 24)
        | (static_cast<std::uint64_t>(static_cast<unsigned char>(source[4])) << 32)
        | (static_cast<std::uint64_t>(static_cast<unsigned char>(source[5])) << 40)
        | (static_cast<std::uint64_t>(static_cast<unsigned char>(source[6])) << 48)
        | (static_cast<std::uint64_t>(static_cast<unsigned char>(source[7])) << 56);
}


int coral::util::ArrayStringCmp(
    const char* array, size_t length, const char* stringz)
{
    for (size_t i = 0; ; ++i) {
        if (i == length) return -static_cast<int>(stringz[i] != '\0');
        if (stringz[i] == '\0') return 1;
        if (array[i] < stringz[i]) return -1;
        if (array[i] > stringz[i]) return 1;
    }
}


std::string coral::util::RandomUUID()
{
    boost::uuids::random_generator gen;
    return boost::uuids::to_string(gen());
}


std::string coral::util::RandomString(size_t size, const char* charSet)
{
    if (charSet == nullptr) {
        throw std::invalid_argument("charSet is null");
    }
    const auto charSetSize = std::strlen(charSet);
    if (charSetSize < 1) {
        throw std::invalid_argument("Empty character set");
    }
    boost::random::random_device rng;
    auto dist = boost::random::uniform_int_distribution<size_t>{0u, charSetSize-1};
    auto ret = std::string(size, '\xFF');
    for (char& c : ret) {
        c = charSet[dist(rng)];
    }
    return ret;
}


std::string coral::util::Timestamp()
{
    const auto t = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());
    const std::size_t len = 17;
    char buf[len];
    std::strftime(buf, len, "%Y%m%dT%H%M%SZ", std::gmtime(&t));
    return std::string(buf);
}


#ifdef _WIN32
namespace
{
    std::vector<wchar_t> Utf8ToUtf16(const std::string& utf8)
    {
        auto buf = std::vector<wchar_t>();
        if (utf8.empty()) return buf;

        const auto len = MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            utf8.data(),
            boost::numeric_cast<int>(utf8.size()),
            nullptr,
            0);
        if (len == 0) {
            if (GetLastError() == ERROR_NO_UNICODE_TRANSLATION) {
                throw std::runtime_error("Invalid UTF-8 characters in string");
            } else {
                throw std::logic_error("Internal error in " __FUNCTION__);
            }
        }
        buf.resize(len);
        MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            utf8.data(),
            boost::numeric_cast<int>(utf8.size()),
            buf.data(),
            boost::numeric_cast<int>(buf.size()));
        return buf;
    }
}
#endif

#ifndef _WIN32
namespace
{
    bool IsExecutable(const std::string& path)
    {
        return access(path.c_str(), X_OK) == 0;
    }
}
#endif


void coral::util::SpawnProcess(
    const std::string& program,
    const std::vector<std::string>& args)
{
#ifdef _WIN32
    auto cmdLine = Utf8ToUtf16(program);
    for (const auto& arg : args) {
        cmdLine.push_back(' ');
        cmdLine.push_back('"');
        const auto argW = Utf8ToUtf16(arg);
        cmdLine.insert(cmdLine.end(), argW.begin(), argW.end());
        cmdLine.push_back('"');
    }
    cmdLine.push_back(0);

    STARTUPINFOW startupInfo;
    std::memset(&startupInfo, 0, sizeof(startupInfo));
    startupInfo.cb = sizeof(startupInfo);

    PROCESS_INFORMATION processInfo;
    if (CreateProcessW(
        nullptr,
        cmdLine.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NEW_CONSOLE,
        nullptr,
        nullptr,
        &startupInfo,
        &processInfo)) {
        return;
    }

#else // not Win32
    if (!IsExecutable(program)) {
        throw std::runtime_error("Not an executable file: "+program);
    }

    std::vector<const char*> argz;
    argz.push_back(program.c_str());
    for (auto it = begin(args); it != end(args); ++it) {
        argz.push_back(it->c_str());
    }
    argz.push_back(nullptr);

    const auto pid = fork();
    if (pid == 0) {
        // We are in child process; execute program immediately.
        // NOTE: execv() doesn't actually modify the argument vector, so the
        // const_cast is OK. For the gritty details, see:
        // http://pubs.opengroup.org/onlinepubs/9699919799/functions/exec.html
        execv(program.c_str(), const_cast<char* const*>(argz.data()));
        std::perror("coral::util::SpawnProcess(): failed to execute program");
        _exit(1);
    } else if (pid > 0) {
        // We are in parent process; return immediately.
        return;
    }
#endif
    throw std::runtime_error("Failed to start process: " + program);
}


boost::filesystem::path coral::util::ThisExePath()
{
#if defined(_WIN32)
    std::vector<wchar_t> buf(MAX_PATH);
    for (;;) {
        const auto len = GetModuleFileNameW(
            nullptr, buf.data(), boost::numeric_cast<DWORD>(buf.size()));
        if (len == 0) {
            throw std::runtime_error("Failed to determine executable path");
        } else if (len >= buf.size()) {
            buf.resize(len * 2);
        } else {
            return boost::filesystem::path(buf.begin(), buf.begin()+len);
        }
    }
#elif defined(__linux__)
    return boost::filesystem::read_symlink("/proc/self/exe");
#else
    assert (!"ThisExePath() not implemented for POSIX platforms yet");
#endif
}
