#include "gtest/gtest.h"

#include "dsb/protocol/discovery.hpp"


TEST(dsb_protocol, ServiceListener)
{
    const std::uint16_t port = 63947;
    auto beacon1 = dsb::protocol::ServiceBeacon(
        100,
        "serviceType1",
        "service1",
        "foo", 3,
        std::chrono::milliseconds(100),
        port);
    auto beacon2 = dsb::protocol::ServiceBeacon(
        100,
        "serviceType2",
        "service2",
        nullptr, 0,
        std::chrono::milliseconds(200),
        port);
    auto beacon3 = dsb::protocol::ServiceBeacon(
        101,
        "serviceType1",
        "service3",
        "baz", 3,
        std::chrono::milliseconds(200),
        port);

    int serviceType1Count = 0;
    int serviceType2Count = 0;
    int bugCount = 0;

    dsb::comm::Reactor reactor;
    auto listener = dsb::protocol::ServiceListener(reactor, 100, port,
        [&] (const std::string& addr, const std::string& st, const std::string& si, const char* pl, std::size_t pls)
        {
            if (st == "serviceType1" && si == "service1" &&
                    std::string(pl, pls) == "foo") {
                ++serviceType1Count;
            } else if (st == "serviceType2" && si == "service2" &&
                     pl == nullptr && pls == 0) {
                ++serviceType2Count;
            } else {
                ++bugCount;
            }
        });
    reactor.AddTimer(
        std::chrono::seconds(2),
        1,
        [] (dsb::comm::Reactor& r, int) { r.Stop(); });
    reactor.Run();

    EXPECT_GT(serviceType1Count, 16);
    EXPECT_LT(serviceType1Count, 22);
    EXPECT_GT(serviceType2Count, 8);
    EXPECT_LT(serviceType2Count, 11);
    EXPECT_EQ(0, bugCount);
}
