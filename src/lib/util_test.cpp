#include <gtest/gtest.h>
#include <coral/util.hpp>
#include <functional>
#include <stdexcept>
#include <vector>


using namespace coral::util;

TEST(coral_util, EncodeUint16) {
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

TEST(coral_util, EncodeUint32) {
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

TEST(coral_util, EncodeUint64) {
    char b[8] = { '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0' };
    EncodeUint64(7460587310468789241UL, b);
    EXPECT_EQ('\xF9', b[0]);
    EXPECT_EQ('\x73', b[1]);
    EXPECT_EQ('\x47', b[2]);
    EXPECT_EQ('\x88', b[3]);
    EXPECT_EQ('\xA1', b[4]);
    EXPECT_EQ('\x54', b[5]);
    EXPECT_EQ('\x89', b[6]);
    EXPECT_EQ('\x67', b[7]);
}

TEST(coral_util, DecodeUint16) {
    EXPECT_EQ(    0u, DecodeUint16("\x00\x00"));
    EXPECT_EQ(65535u, DecodeUint16("\xFF\xFF"));
    EXPECT_EQ( 4608u, DecodeUint16("\x00\x12"));
    EXPECT_EQ(   63u, DecodeUint16("\x3F\x00"));
    EXPECT_EQ(15238u, DecodeUint16("\x86\x3B"));
}

TEST(coral_util, DecodeUint32) {
    EXPECT_EQ(         0u, DecodeUint32("\x00\x00\x00\x00"));
    EXPECT_EQ(     65535u, DecodeUint32("\xFF\xFF\x00\x00"));
    EXPECT_EQ(4294967295u, DecodeUint32("\xFF\xFF\xFF\xFF"));
    EXPECT_EQ(2018915346u, DecodeUint32("\x12\x34\x56\x78"));
}

TEST(coral_util, DecodeUint64) {
    EXPECT_EQ(
        7460587310468789241UL,
        DecodeUint64("\xF9\x73\x47\x88\xA1\x54\x89\x67"));
}

TEST(coral_util, ArrayStringCmp) {
    char test[3] = { 'a', 'b', 'c' };
    EXPECT_EQ(0, ArrayStringCmp(test, 3, "abc"));
    EXPECT_GT(0, ArrayStringCmp(test, 3, "abcd"));
    EXPECT_GT(0, ArrayStringCmp(test, 3, "abd"));
    EXPECT_LT(0, ArrayStringCmp(test, 3, "ab"));
    EXPECT_LT(0, ArrayStringCmp(test, 3, "abb"));
}

TEST(coral_util, RandomUUID)
{
    const auto u = RandomUUID();
    EXPECT_EQ(36U, u.size());
    EXPECT_NE(u, RandomUUID());
}

TEST(coral_util, RandomString)
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

TEST(coral_util, MoveAndReplace_value)
{
    int a = 123;
    int b = MoveAndReplace(a, 456);
    EXPECT_EQ(456, a);
    EXPECT_EQ(123, b);
    int c = MoveAndReplace(b);
    EXPECT_EQ(0, b);
    EXPECT_EQ(123, c);
}

TEST(coral_util, MoveAndReplace_class)
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

TEST(coral_util, LastCall)
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

TEST(coral_util, OnScopeExit)
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

TEST(coral_util, ThisExePath)
{
#ifdef _WIN32
    const auto expected = "coral_test.exe";
#else
    const auto expected = "coral_test";
#endif
    EXPECT_EQ(expected, ThisExePath().filename().string());
}
