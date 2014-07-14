#include "gtest/gtest.h"
#include "dsb/compat_helpers.hpp"


namespace
{
    // Helper classes for testing make_unique()
    // The constructors only take rvalue references, to help verify that
    // make_unique() uses perfect forwarding.
    class C0 { public: C0() { } };
    class C1 { public: int m1;          C1(int&& a1)                     : m1(a1) { } };
    class C2 { public: int m1, m2;      C2(int&& a1, int&& a2)           : m1(a1), m2(a2) { } };
    class C3 { public: int m1, m2, m3;  C3(int&& a1, int&& a2, int&& a3) : m1(a1), m2(a2), m3(a3) { } };
}

// NOTE: We compile and run these tests with C++14-compliant compilers too,
// to ensure that we don't accidentally test non-standard behaviour.
TEST(dsb_compat_helpers, make_unique)
{
    std::unique_ptr<C0> c0 = std::make_unique<C0>();
    ASSERT_NE(nullptr, c0.get());
    std::unique_ptr<C1> c1 = std::make_unique<C1>(1);
    ASSERT_NE(nullptr, c1.get());
    EXPECT_EQ(1, c1->m1);
    std::unique_ptr<C2> c2 = std::make_unique<C2>(2, 20);
    ASSERT_NE(nullptr, c2.get());
    EXPECT_EQ(2, c2->m1);
    EXPECT_EQ(20, c2->m2);
    std::unique_ptr<C3> c3 = std::make_unique<C3>(3, 30, 300);
    ASSERT_NE(nullptr, c3.get());
    EXPECT_EQ(3, c3->m1);
    EXPECT_EQ(30, c3->m2);
    EXPECT_EQ(300, c3->m3);
}
