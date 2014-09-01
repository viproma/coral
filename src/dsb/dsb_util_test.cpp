#include "gtest/gtest.h"
#include "dsb/util.hpp"

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

TEST(dsb_util, DecodeUint16) {
    EXPECT_EQ(    0, DecodeUint16("\x00\x00"));
    EXPECT_EQ(65535, DecodeUint16("\xFF\xFF"));
    EXPECT_EQ( 4608, DecodeUint16("\x00\x12"));
    EXPECT_EQ(   63, DecodeUint16("\x3F\x00"));
    EXPECT_EQ(15238, DecodeUint16("\x86\x3B"));
}

TEST(dsb_util, RandomUUID)
{
    const auto u = RandomUUID();
    EXPECT_EQ(36U, u.size());
    EXPECT_NE(u, RandomUUID());
}
