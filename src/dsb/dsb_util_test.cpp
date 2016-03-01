#include "gtest/gtest.h"
#include "dsb/util.hpp"
#include <functional>
#include <stdexcept>
#include <vector>
#include "boost/filesystem.hpp"


using namespace dsb::util;

TEST(dsb_util, EncodeUint16) {
    char b[2] = { '\xFF', '\xFF' };

    EncodeUint16(0, b);
    EXPECT_EQ('\x00', b[0]);
    EXPECT_EQ('\x00', b[1]);

    EncodeUint16(65535, b);
    EXPECT_EQ('\xFF', b[0]);
    EXPECT_EQ('\xFF', b[1]);

    EncodeUint16(4608, b);
    EXPECT_EQ('\x00', b[0]);
    EXPECT_EQ('\x12', b[1]);

    EncodeUint16(63, b);
    EXPECT_EQ('\x3F', b[0]);
    EXPECT_EQ('\x00', b[1]);

    EncodeUint16(15238, b);
    EXPECT_EQ('\x86', b[0]);
    EXPECT_EQ('\x3B', b[1]);
}

TEST(dsb_util, EncodeUint32) {
    char b[4] = { '\xFF', '\xFF', '\xFF', '\xFF' };

    EncodeUint32(0, b);
    EXPECT_EQ('\x00', b[0]);
    EXPECT_EQ('\x00', b[1]);
    EXPECT_EQ('\x00', b[2]);
    EXPECT_EQ('\x00', b[3]);

    EncodeUint32(65535, b);
    EXPECT_EQ('\xFF', b[0]);
    EXPECT_EQ('\xFF', b[1]);
    EXPECT_EQ('\x00', b[2]);
    EXPECT_EQ('\x00', b[3]);

    EncodeUint32(4294967295, b);
    EXPECT_EQ('\xFF', b[0]);
    EXPECT_EQ('\xFF', b[1]);
    EXPECT_EQ('\xFF', b[2]);
    EXPECT_EQ('\xFF', b[3]);

    EncodeUint32(2018915346, b);
    EXPECT_EQ('\x12', b[0]);
    EXPECT_EQ('\x34', b[1]);
    EXPECT_EQ('\x56', b[2]);
    EXPECT_EQ('\x78', b[3]);
}

TEST(dsb_util, DecodeUint16) {
    EXPECT_EQ(    0u, DecodeUint16("\x00\x00"));
    EXPECT_EQ(65535u, DecodeUint16("\xFF\xFF"));
    EXPECT_EQ( 4608u, DecodeUint16("\x00\x12"));
    EXPECT_EQ(   63u, DecodeUint16("\x3F\x00"));
    EXPECT_EQ(15238u, DecodeUint16("\x86\x3B"));
}

TEST(dsb_util, DecodeUint32) {
    EXPECT_EQ(         0u, DecodeUint32("\x00\x00\x00\x00"));
    EXPECT_EQ(     65535u, DecodeUint32("\xFF\xFF\x00\x00"));
    EXPECT_EQ(4294967295u, DecodeUint32("\xFF\xFF\xFF\xFF"));
    EXPECT_EQ(2018915346u, DecodeUint32("\x12\x34\x56\x78"));
}

TEST(dsb_util, RandomUUID)
{
    const auto u = RandomUUID();
    EXPECT_EQ(36U, u.size());
    EXPECT_NE(u, RandomUUID());
}

TEST(dsb_util, RandomString)
{
    const auto s = RandomString(10, "abcdefghijklmnopqrstuvwxyz");
    ASSERT_EQ(10u, s.size());
    for (char c : s) {
        EXPECT_GE(c, 'a');
        EXPECT_LE(c, 'z');
    }
    EXPECT_NE(s, RandomString(10, "abcdefghijklmnopqrstuvwxyz"));
    EXPECT_EQ("aaaa", RandomString(4, "a"));
    EXPECT_TRUE(RandomString(0, "abcd").empty());
    EXPECT_THROW(RandomString(4, nullptr), std::invalid_argument);
    EXPECT_THROW(RandomString(4, ""), std::invalid_argument);
}

TEST(dsb_util, MoveAndReplace_value)
{
    int a = 123;
    int b = MoveAndReplace(a, 456);
    EXPECT_EQ(456, a);
    EXPECT_EQ(123, b);
    int c = MoveAndReplace(b);
    EXPECT_EQ(0, b);
    EXPECT_EQ(123, c);
}

TEST(dsb_util, MoveAndReplace_class)
{
    std::vector<int> a;
    a.push_back(123);
    const auto dataPtr = a.data();
    std::vector<int> r;
    r.push_back(456);
    r.push_back(789);

    std::vector<int> b = MoveAndReplace(a, r);
    ASSERT_EQ(2U, a.size());
    EXPECT_EQ(456, a[0]);
    EXPECT_EQ(789, a[1]);

    // The following test is (most likely) not specified C++ behaviour, but
    // it would be a strange vector implementation that didn't implement a move
    // as a pointer move...
    EXPECT_EQ(1U, b.size());
    EXPECT_EQ(dataPtr, b.data());

    std::vector<int> c = MoveAndReplace(b);
    EXPECT_TRUE(b.empty());
    EXPECT_EQ(1U, c.size());
    EXPECT_EQ(dataPtr, c.data());
}

TEST(dsb_util, LastCall)
{
    int i = -1;

    std::function<void()> f0 = [&]() {
        ++i;
        EXPECT_FALSE(f0);
    };
    EXPECT_TRUE(!!f0);
    LastCall(f0);
    EXPECT_EQ(0, i);

    std::function<void(int)> f1 = [&](int x) {
        ++i;
        EXPECT_EQ(123, x);
        EXPECT_FALSE(f1);
    };
    EXPECT_TRUE(!!f1);
    LastCall(f1, 123);
    EXPECT_EQ(1, i);
}

TEST(dsb_util, OnScopeExit)
{
    int i = 0;
    {
        auto setToOne = OnScopeExit([&i]() { i = 1; });
        EXPECT_EQ(0, i);
    }
    EXPECT_EQ(1, i);
    try {
        auto setToTwo = OnScopeExit([&i]() { i = 2; });
        EXPECT_EQ(1, i);
        throw 0;
    } catch (...) {
        EXPECT_EQ(2, i);
        i = 3;
    }
    EXPECT_EQ(3, i);
}

TEST(dsb_util, TempDir)
{
    boost::filesystem::path d;
    {
        auto tmp = TempDir();
        d = tmp.Path();
        ASSERT_FALSE(d.empty());
        EXPECT_TRUE(boost::filesystem::is_directory(d));
        EXPECT_TRUE(boost::filesystem::is_empty(d));
    }
    EXPECT_FALSE(boost::filesystem::exists(d));
}

TEST(dsb_util, ThisExePath)
{
#ifdef _WIN32
    const auto expected = "dsb_test.exe";
#else
    const auto expected = "dsb_test";
#endif
    EXPECT_EQ(expected, ThisExePath().filename().string());
}
