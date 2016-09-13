#include "gtest/gtest.h"
#include "coral/net.hpp"


TEST(coral_net, Endpoint)
{
    const auto e = coral::net::Endpoint{"foo", "bar"};
    EXPECT_EQ("foo", e.Transport());
    EXPECT_EQ("bar", e.Address());
    EXPECT_EQ("foo://bar", e.URL());

    const auto f = coral::net::Endpoint{"bar://baz"};
    EXPECT_EQ("bar", f.Transport());
    EXPECT_EQ("baz", f.Address());
    EXPECT_EQ("bar://baz", f.URL());
}


TEST(coral_net_ip, Address_any)
{
    const auto a = coral::net::ip::Address{};
    EXPECT_TRUE(a.IsAnyAddress());
    EXPECT_EQ("*", a.ToString());
    EXPECT_EQ(INADDR_ANY, a.ToInAddr().s_addr);

    const auto b = coral::net::ip::Address{"*"};
    EXPECT_TRUE(b.IsAnyAddress());
    EXPECT_EQ("*", b.ToString());
    EXPECT_EQ(INADDR_ANY, b.ToInAddr().s_addr);

    in_addr ci;
    ci.s_addr = INADDR_ANY;
    const auto c = coral::net::ip::Address{ci};
    EXPECT_TRUE(c.IsAnyAddress());
    EXPECT_EQ("*", c.ToString());
    EXPECT_EQ(INADDR_ANY, c.ToInAddr().s_addr);

    const auto d = coral::net::ip::Address{"0.0.0.0"};
    EXPECT_TRUE(d.IsAnyAddress());
    EXPECT_EQ("*", d.ToString());
    EXPECT_EQ(INADDR_ANY, d.ToInAddr().s_addr);
}


TEST(coral_net_ip, Address_IPv4)
{
    const std::string ipText = "10.0.213.45";
    const std::uint8_t ipBinary[4] = { 0x0A, 0x00, 0xD5, 0x2D};

    const auto a = coral::net::ip::Address{ipText};
    EXPECT_FALSE(a.IsAnyAddress());
    EXPECT_EQ(ipText, a.ToString());
    EXPECT_EQ(
        *reinterpret_cast<const std::uint32_t*>(ipBinary),
        a.ToInAddr().s_addr);

    in_addr bi;
    bi.s_addr = *reinterpret_cast<const std::uint32_t*>(ipBinary);
    const auto b = coral::net::ip::Address{bi};
    EXPECT_FALSE(b.IsAnyAddress());
    EXPECT_EQ(ipText, b.ToString());
    EXPECT_EQ(
        *reinterpret_cast<const std::uint32_t*>(ipBinary),
        b.ToInAddr().s_addr);
}


TEST(coral_net_ip, Address_textual)
{
    const auto a = coral::net::ip::Address("foo.com");
    EXPECT_FALSE(a.IsAnyAddress());
    EXPECT_EQ("foo.com", a.ToString());
    EXPECT_THROW(a.ToInAddr(), std::logic_error);
}


TEST(coral_net_ip, Port_integer)
{
    const std::uint8_t portBinary[2] = { 0x12, 0x6F };

    const auto p = coral::net::ip::Port{4719};
    EXPECT_TRUE(p.IsNumber());
    EXPECT_FALSE(p.IsAnyPort());
    EXPECT_EQ(4719, p.ToNumber());
    EXPECT_EQ("4719", p.ToString());
    EXPECT_EQ(
        *reinterpret_cast<const std::uint16_t*>(portBinary),
        p.ToNetworkByteOrder());

    const auto q = coral::net::ip::Port::FromNetworkByteOrder(
        *reinterpret_cast<const std::uint16_t*>(portBinary));
    EXPECT_TRUE(q.IsNumber());
    EXPECT_FALSE(q.IsAnyPort());
    EXPECT_EQ(4719, q.ToNumber());
    EXPECT_EQ("4719", q.ToString());
    EXPECT_EQ(
        *reinterpret_cast<const std::uint16_t*>(portBinary),
        q.ToNetworkByteOrder());
}


TEST(coral_net_ip, Port_string)
{
    const auto p = coral::net::ip::Port{"4719"};
    EXPECT_TRUE(p.IsNumber());
    EXPECT_FALSE(p.IsAnyPort());
    EXPECT_EQ(4719, p.ToNumber());
    EXPECT_EQ("4719", p.ToString());
    const std::uint8_t portBinary[2] = { 0x12, 0x6F };
    EXPECT_EQ(
        *reinterpret_cast<const std::uint16_t*>(portBinary),
        p.ToNetworkByteOrder());

    EXPECT_THROW(coral::net::ip::Port{"foo"}, std::invalid_argument);
    EXPECT_THROW(coral::net::ip::Port{"-1"}, std::out_of_range);
    EXPECT_THROW(coral::net::ip::Port{"65536"}, std::out_of_range);
}


TEST(coral_net_ip, Port_any)
{
    const auto p = coral::net::ip::Port{"*"};
    EXPECT_FALSE(p.IsNumber());
    EXPECT_TRUE(p.IsAnyPort());
    EXPECT_THROW(p.ToNumber(), std::logic_error);
    EXPECT_EQ("*", p.ToString());
    EXPECT_THROW(p.ToNetworkByteOrder(), std::logic_error);
}


TEST(coral_net_ip, Endpoint)
{
    const std::string address = "10.0.213.45";
    const std::uint8_t addressBin[4] = { 0x0A, 0x00, 0xD5, 0x2D};
    const std::uint16_t port = 4719;
    const std::uint8_t portBin[2] = { 0x12, 0x6F };

    auto e = coral::net::ip::Endpoint{address, port};
    EXPECT_EQ(*reinterpret_cast<const std::uint32_t*>(addressBin), e.Address().ToInAddr().s_addr);
    EXPECT_EQ(*reinterpret_cast<const std::uint16_t*>(portBin), e.Port().ToNetworkByteOrder());
    EXPECT_EQ("tcp://10.0.213.45:4719", e.ToEndpoint("tcp").URL());
    EXPECT_THROW(e.ToEndpoint(""), std::invalid_argument);
    auto esin = e.ToSockaddrIn();
    EXPECT_EQ(AF_INET, esin.sin_family);
    EXPECT_EQ(*reinterpret_cast<const std::uint32_t*>(addressBin), esin.sin_addr.s_addr);
    EXPECT_EQ(*reinterpret_cast<const std::uint16_t*>(portBin), esin.sin_port);

    e.SetAddress(std::string("*"));
    e.SetPort_(std::string("*"));
    EXPECT_EQ("*", e.Address().ToString());
    EXPECT_EQ("*", e.Port().ToString());
    EXPECT_EQ("tcp://*:*", e.ToEndpoint("tcp").URL());
    EXPECT_THROW(e.ToSockaddrIn(), std::logic_error);
}


TEST(coral_net_ip, Endpoint_stringSpec)
{
    const auto e = coral::net::ip::Endpoint{"foo:321"};
    EXPECT_EQ("foo", e.Address().ToString());
    EXPECT_EQ(321, e.Port().ToNumber());

    EXPECT_THROW(coral::net::ip::Endpoint{"foo:bar"}, std::invalid_argument);
    EXPECT_THROW(coral::net::ip::Endpoint{"foo:-10"}, std::out_of_range);
}
