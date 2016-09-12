#ifdef _WIN32
#   define NOMINMAX
#endif
#include <algorithm> // std::max
#include <memory>
#include "gtest/gtest.h"
#include "dsb/net/service.hpp"


TEST(dsb_net_service, Listener)
{
    const std::uint16_t port = 63947;
    auto beacon1 = dsb::net::service::Beacon(
        100,
        "serviceType1",
        "service1",
        "foo", 3,
        std::chrono::milliseconds(100),
        "*",
        port);
    auto beacon2 = dsb::net::service::Beacon(
        100,
        "serviceType2",
        "service2",
        nullptr, 0,
        std::chrono::milliseconds(200),
        "127.0.0.1",
        port);
    auto beacon3 = dsb::net::service::Beacon(
        101,
        "serviceType1",
        "service3",
        "baz", 3,
        std::chrono::milliseconds(200),
        "*",
        port);

    int serviceType1Count = 0;
    int serviceType2Count = 0;
    int bugCount = 0;

    dsb::net::Reactor reactor;
    auto listener = dsb::net::service::Listener{
        reactor,
        100,
        dsb::net::ip::Endpoint{"*", port},
        [&] (const std::string& addr, const std::string& st, const std::string& si, const char* pl, std::size_t pls)
        {
            if (st == "serviceType1" && si == "service1" &&
                    std::string(pl, pls) == "foo") {
                ++serviceType1Count;
            } else if (addr == "127.0.0.1" && st == "serviceType2" &&
                    si == "service2" && pl == nullptr && pls == 0) {
                ++serviceType2Count;
            } else {
                ++bugCount;
            }
        }};
    reactor.AddTimer(
        std::chrono::seconds(2),
        1,
        [] (dsb::net::Reactor& r, int) { r.Stop(); });
    reactor.Run();

    // Note that beacon1 broadcasts on all available interfaces, and we
    // don't really know how many messages we will receive.  beacon2, on
    // the other hand, only broadcasts on the loopback interface, so we
    // should receive about 10 pings.
    EXPECT_GT(serviceType1Count, 16);
    EXPECT_GT(serviceType2Count, 8);
    EXPECT_LT(serviceType2Count, 12);
    EXPECT_EQ(0, bugCount);
}


TEST(dsb_net_service, Tracker)
{
    namespace sc = std::chrono;

    const int partitionID = 0;
    const std::uint16_t port = 63948;
    std::unique_ptr<dsb::net::service::Beacon> beacon11, beacon12, beacon21, beacon31;
    const auto service11StartTime = sc::milliseconds(300);
    const auto service12StartTime = sc::milliseconds(1200);
    const auto service21StartTime = sc::milliseconds(600);
    const auto service31StartTime = sc::milliseconds(900);
    const auto service11ChangeTime = sc::milliseconds(1500);
    const auto serviceStopTime = sc::milliseconds(1800);
    const auto serviceType1Period = sc::milliseconds(200);
    const auto serviceType2Period = sc::milliseconds(100);
    const auto serviceType3Period = sc::milliseconds(200);
    const auto serviceType1Timeout = 3 * serviceType1Period;
    const auto serviceType2Timeout = 3 * serviceType2Period;
    const auto serviceType3Timeout = 3 * serviceType3Period;
    const auto testStopTime = serviceStopTime
        + std::max({serviceType1Timeout, serviceType2Timeout, serviceType3Timeout});
    const std::string service11Payload1 = "aaa";
    const std::string service11Payload2 = "aaab";
    const std::string service12Payload = "bbb";
    const std::string service21Payload = "ccc";
    const std::string service31Payload = "ddd";

    dsb::net::Reactor reactor;
    reactor.AddTimer(
        service11StartTime, 1,
        [&] (dsb::net::Reactor&, int) {
            beacon11 = std::make_unique<dsb::net::service::Beacon>(
                partitionID,
                "serviceType1",
                "service1.1",
                service11Payload1.data(), service11Payload1.size(),
                serviceType1Period,
                "*",
                port);
        });
    reactor.AddTimer(
        service12StartTime, 1,
        [&] (dsb::net::Reactor&, int) {
            beacon12 = std::make_unique<dsb::net::service::Beacon>(
                partitionID,
                "serviceType1",
                "service1.2",
                service12Payload.data(), service12Payload.size(),
                serviceType1Period,
                "*",
                port);
        });
    reactor.AddTimer(
        service21StartTime, 1,
        [&] (dsb::net::Reactor&, int) {
            beacon21 = std::make_unique<dsb::net::service::Beacon>(
                partitionID,
                "serviceType2",
                "service2.1",
                service21Payload.data(), service21Payload.size(),
                serviceType2Period,
                "*",
                port);
        });
    reactor.AddTimer(
        service31StartTime, 1,
        [&] (dsb::net::Reactor&, int) {
            beacon31 = std::make_unique<dsb::net::service::Beacon>(
                partitionID,
                "serviceType3",
                "service3.1",
                service31Payload.data(), service31Payload.size(),
                serviceType3Period,
                "*",
                port);
        });
    reactor.AddTimer(
        service11ChangeTime, 1,
        [&] (dsb::net::Reactor&, int) {
            beacon11 = std::make_unique<dsb::net::service::Beacon>(
                partitionID,
                "serviceType1",
                "service1.1",
                service11Payload2.data(), service11Payload2.size(),
                serviceType1Period,
                "*",
                port);
        });
    reactor.AddTimer(
        serviceStopTime, 1,
        [&] (dsb::net::Reactor&, int) {
            beacon11->Stop();
            beacon12->Stop();
            beacon21->Stop();
            beacon31->Stop();
        });
    reactor.AddTimer(
        testStopTime, 1,
        [&] (dsb::net::Reactor&, int) { reactor.Stop(); });

    bool service11DiscoveredOnTime = false;
    bool service12DiscoveredOnTime = false;
    bool service21DiscoveredOnTime = false;
    bool service11UpdatedOnTime = false;
    bool service11LostOnTime = false;
    bool service12LostOnTime = false;
    bool service21LostOnTime = false;
    bool bug = false;

    sc::steady_clock::time_point start; // We set this later
    const auto NowIs = [&start] (sc::milliseconds t, sc::milliseconds dt)
    {
        const auto error = sc::milliseconds(20);
        const auto minimum = start + t - error;
        const auto maximum = start + t + dt + error;
        const auto actual = sc::steady_clock::now();
        return minimum <= actual && actual <= maximum;
    };

    auto tracker = dsb::net::service::Tracker{
        reactor,
        0,
        dsb::net::ip::Endpoint{"*", port}};
    tracker.AddTrackedServiceType(
        "serviceType1", serviceType1Timeout,
        [&] (
                const std::string& /*address*/,
                const std::string& serviceType,
                const std::string& serviceID,
                const char* payload,
                std::size_t payloadSize) {
            ASSERT_EQ("serviceType1", serviceType);
            if (serviceID == "service1.1"
                    && std::string(payload, payloadSize) == service11Payload1
                    && NowIs(service11StartTime, serviceType1Period)) {
                service11DiscoveredOnTime = true;
            } else if (serviceID == "service1.2"
                    && std::string(payload, payloadSize) == service12Payload
                    && NowIs(service12StartTime, serviceType1Period)) {
                service12DiscoveredOnTime = true;
            } else {
                bug = true;
            }
        },
        [&] (
                const std::string& /*address*/,
                const std::string& serviceType,
                const std::string& serviceID,
                const char* payload,
                std::size_t payloadSize) {
            ASSERT_EQ("serviceType1", serviceType);
            if (serviceID == "service1.1"
                    && std::string(payload, payloadSize) == service11Payload2
                    && NowIs(service11ChangeTime, serviceType1Period)) {
                service11UpdatedOnTime = true;
            } else {
                bug = true;
            }
        },
        [&] (
                const std::string& serviceType,
                const std::string& serviceID) {
            ASSERT_EQ("serviceType1", serviceType);
            if (serviceID == "service1.1"
                    && NowIs(serviceStopTime, serviceType1Timeout)) {
                service11LostOnTime = true;
            } else if (serviceID == "service1.2"
                    && NowIs(serviceStopTime, serviceType1Timeout)) {
                service12LostOnTime = true;
            } else {
                bug = true;
            }
        });
    tracker.AddTrackedServiceType(
        "serviceType2", serviceType2Timeout,
        [&] (
                const std::string& /*address*/,
                const std::string& serviceType,
                const std::string& serviceID,
                const char* payload,
                std::size_t payloadSize) {
            ASSERT_EQ("serviceType2", serviceType);
            if (serviceID == "service2.1"
                    && std::string(payload, payloadSize) == service21Payload
                    && NowIs(service21StartTime, serviceType2Period)) {
                service21DiscoveredOnTime = true;
            } else {
                bug = true;
            }
        },
        nullptr, // no "changed" handler necessary
        [&] (
                const std::string& serviceType,
                const std::string& serviceID) {
            ASSERT_EQ("serviceType2", serviceType);
            if (serviceID == "service2.1"
                    && NowIs(serviceStopTime, serviceType2Timeout)) {
                service21LostOnTime = true;
            } else {
                bug = true;
            }
        });

    start = sc::steady_clock::now();
    reactor.Run();

    EXPECT_TRUE(service11DiscoveredOnTime);
    EXPECT_TRUE(service12DiscoveredOnTime);
    EXPECT_TRUE(service21DiscoveredOnTime);
    EXPECT_TRUE(service11UpdatedOnTime);
    EXPECT_TRUE(service11LostOnTime);
    EXPECT_TRUE(service12LostOnTime);
    EXPECT_TRUE(service21LostOnTime);
    EXPECT_FALSE(bug);
}
