#include <map>
#include <string>
#include <vector>
#include "gtest/gtest.h"
#include "dsb/sequence.hpp"

using namespace dsb::sequence;


TEST(dsb_sequence, NullSequence)
{
    Sequence<int> s;
    EXPECT_TRUE(s.Empty());
}


TEST(dsb_sequence, ValueRefs)
{
    int a[] = { 1, 2 };
    auto r = ArraySequence(a, 2);
    r.Next();
    auto v = &r.Next();
    EXPECT_EQ(v, a + 1);
}


TEST(dsb_sequence, RefSemantics)
{
    int a[] = { 1, 2, 3 };
    auto r1 = ArraySequence(a, 3);
    auto r2 = r1;
    EXPECT_EQ(1, r2.Next());
    EXPECT_EQ(2, r1.Next());
    EXPECT_EQ(3, r2.Next());
}


TEST(dsb_sequence, ConstElements)
{
    const int a[] = { 1, 2 };
    auto r = ArraySequence(a, 2);
    ASSERT_FALSE(r.Empty());
    EXPECT_EQ(1, r.Next());
    ASSERT_FALSE(r.Empty());
    EXPECT_EQ(2, r.Next());
    EXPECT_TRUE(r.Empty());
}


TEST(dsb_sequence, ContainerSequence)
{
    std::vector<std::string> v;
    v.push_back("foo");
    v.push_back("bar");
    v.push_back("baz");

    auto r = ContainerSequence(v);
    ASSERT_FALSE(r.Empty());
    EXPECT_EQ("foo", r.Next());
    ASSERT_FALSE(r.Empty());
    EXPECT_EQ("bar", r.Next());
    ASSERT_FALSE(r.Empty());
    EXPECT_EQ("baz", r.Next());
    EXPECT_TRUE(r.Empty());
}


TEST(dsb_sequence, ConstContainerSequence)
{
    const std::vector<std::string> v(2);
    auto r = ContainerSequence(v);
    ASSERT_FALSE(r.Empty());
    EXPECT_EQ("", r.Next());
    ASSERT_FALSE(r.Empty());
    EXPECT_EQ("", r.Next());
    EXPECT_TRUE(r.Empty());
}


TEST(dsb_sequence, ArraySequence)
{
    int a[] = { 3, 1, 4 };
    auto r = ArraySequence(a, 3);
    ASSERT_FALSE(r.Empty());
    EXPECT_EQ(3, r.Next());
    ASSERT_FALSE(r.Empty());
    EXPECT_EQ(1, r.Next());
    ASSERT_FALSE(r.Empty());
    EXPECT_EQ(4, r.Next());
    EXPECT_TRUE(r.Empty());
}


TEST(dsb_sequence, MapValueSequence)
{
    std::map<int, std::string> m;
    m[123] = "foo";
    m[7] = "bar";
    auto s = MapValueSequence(m);
    ASSERT_FALSE(s.Empty());
    EXPECT_EQ("bar", s.Next());
    ASSERT_FALSE(s.Empty());
    EXPECT_EQ("foo", s.Next());
    EXPECT_TRUE(s.Empty());
}
