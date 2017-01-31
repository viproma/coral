/*
Copyright 2013-2017, SINTEF Ocean and the Coral contributors.
This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#include "coral/net/ip.hpp"

#ifdef _WIN32
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#   include <ws2tcpip.h>
#   include <iphlpapi.h>

#   include <algorithm>
#   include <cassert>
#   include <codecvt>
#   include <cstdint>
#   include <locale>

#else // Linux or BSD-like
#   include <arpa/inet.h>
#   include <ifaddrs.h>
#   ifdef __linux__
#       include <net/if.h>
#   endif
#   include <sys/socket.h>

#   include "coral/util.hpp"
#endif


namespace coral
{
namespace net
{
namespace ip
{


#ifdef _WIN32

std::vector<NetworkInterfaceInfo> GetNetworkInterfaces()
{
    // Get the IP address table. We may have to retry the GetIpAddrTable()
    // call a few times due to a race condition:  The buffer size requirement
    // could change between calls due to interfaces being added/enabled.
    // We retry this up to 5 times before giving up.
    ULONG ipTableSize = 0;
    auto ipTableBuffer = std::vector<char>{};
    DWORD giaResult = ERROR_INSUFFICIENT_BUFFER;
    for (int i = 0; giaResult == ERROR_INSUFFICIENT_BUFFER && i < 5; ++i) {
        ipTableBuffer.resize(ipTableSize);
        giaResult = GetIpAddrTable(
            reinterpret_cast<MIB_IPADDRTABLE*>(ipTableBuffer.data()),
            &ipTableSize,
            FALSE /* do not sort by IP address */);
        assert(giaResult != ERROR_INVALID_PARAMETER);
    }
    if (giaResult != NO_ERROR) {
        throw std::runtime_error("Failed to acquire network interface info");
    }
    auto& ipTable = *reinterpret_cast<const MIB_IPADDRTABLE*>(ipTableBuffer.data());

    // Get list of network interfaces. The same method as above applies here.
    ULONG ifInfoSize = 0;
    auto ifInfoBuffer = std::vector<char>{};
    DWORD giiResult = ERROR_INSUFFICIENT_BUFFER;
    for (int i = 0; giiResult == ERROR_INSUFFICIENT_BUFFER && i < 5; ++i) {
        ifInfoBuffer.resize(ifInfoSize);
        giiResult = GetInterfaceInfo(
            reinterpret_cast<IP_INTERFACE_INFO*>(ifInfoBuffer.data()),
            &ifInfoSize);
        assert(giiResult != ERROR_INVALID_PARAMETER);
    }
    if (giiResult != NO_ERROR && giiResult != ERROR_NO_DATA) {
        throw std::runtime_error("Failed to acquire network interface info");
    }
    auto& ifInfo = *reinterpret_cast<const IP_INTERFACE_INFO*>(ifInfoBuffer.data());


    // Build the interface list
    auto interfaces = std::vector<NetworkInterfaceInfo>{};
    for (DWORD i = 0; i < ipTable.dwNumEntries; ++i) {
        const auto& ipEntry = ipTable.table[i];

        NetworkInterfaceInfo nif;
        nif.address.s_addr = ipEntry.dwAddr;
        nif.netmask.s_addr = ipEntry.dwMask;
        nif.broadcastAddress.s_addr = ipEntry.dwAddr | ~ipEntry.dwMask;

        // For each IP address, try to find the name of the interface
        if (giiResult != ERROR_NO_DATA) {
            const auto begin = &ifInfo.Adapter[0];
            const auto end = begin + ifInfo.NumAdapters;
            const auto it = std::find_if(
                begin, end,
                [&](const IP_ADAPTER_INDEX_MAP& e) {
                    return e.Index == ipEntry.dwIndex;
                });
            if (it != end) {
                nif.name = std::wstring_convert<std::codecvt_utf8_utf16<WCHAR>, WCHAR>{}
                    .to_bytes(it->Name);
            }
        }
        interfaces.push_back(nif);
    }
    return interfaces;
}

#else // Linux or BSD-like

std::vector<NetworkInterfaceInfo> GetNetworkInterfaces()
{
    ifaddrs* ifAddrs = nullptr;
    if (getifaddrs(&ifAddrs) == -1) {
        throw std::runtime_error("Failed to acquire network interface info");
    }
    auto freeIfAddrs = coral::util::OnScopeExit([&]() {
        freeifaddrs(ifAddrs);
    });

    auto interfaces = std::vector<NetworkInterfaceInfo>{};
    const ifaddrs* a = ifAddrs;
    while (a) {
        if (a->ifa_addr->sa_family == AF_INET) {
            NetworkInterfaceInfo nif;
            nif.name = a->ifa_name;
            nif.address =
                reinterpret_cast<const sockaddr_in*>(a->ifa_addr)->sin_addr;
            nif.netmask =
                reinterpret_cast<const sockaddr_in*>(a->ifa_netmask)->sin_addr;
            nif.broadcastAddress =
                reinterpret_cast<const sockaddr_in*>(a->ifa_broadaddr)->sin_addr;
            interfaces.push_back(nif);
        }
        a = a->ifa_next;
    }
    return interfaces;
}

#endif


std::string IPAddressToString(in_addr address)
{
    char buffer[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &address, buffer, sizeof(buffer)) == nullptr) {
        throw std::logic_error("IP address conversion failed");
    }
    return buffer;
}


in_addr StringToIPAddress(const std::string& address)
{
    in_addr result;
    const auto status = inet_pton(AF_INET, address.c_str(), &result);
    if (status == 1) {
        return result;
    } else if (status == 0) {
        throw std::invalid_argument("Not an IPv4 address: " + address);
    } else {
        throw std::logic_error("IP address conversion failed");
    }
}


}}} // namespace
