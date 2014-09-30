#include <map>
#include <string>
#include <list>
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
    auto r = ElementsOf(a, 2);
    r.Next();
    auto v = &r.Next();
    EXPECT_EQ(v, a + 1);
}


TEST(dsb_sequence, RefSemantics)
{
    int a[] = { 1, 2, 3 };
    auto r1 = ElementsOf(a, 3);
    auto r2 = r1;
    EXPECT_EQ(1, r2.Next());
    EXPECT_EQ(2, r1.Next());
    EXPECT_EQ(3, r2.Next());
}


TEST(dsb_sequence, ConstElements)
{
    const int a[] = { 1, 2 };
    auto r = ElementsOf(a, 2);
    ASSERT_FALSE(r.Empty());
    EXPECT_EQ(1, r.Next());
    ASSERT_FALSE(r.Empty());
    EXPECT_EQ(2, r.Next());
    EXPECT_TRUE(r.Empty());
}


TEST(dsb_sequence, IteratorSequence)
{
    std::list<std::string> v;
    v.push_back("foo");
    v.push_back("bar");
    v.push_back("baz");

    auto r = ElementsOf(v.begin(), v.end());
    ASSERT_FALSE(r.Empty());
    EXPECT_EQ("foo", r.Next());
    ASSERT_FALSE(r.Empty());
    EXPECT_EQ("bar", r.Next());
    ASSERT_FALSE(r.Empty());
    EXPECT_EQ("baz", r.Next());
    EXPECT_TRUE(r.Empty());
}


TEST(dsb_sequence, ContainerSequence)
{
    std::list<std::string> v;
    v.push_back("foo");
    v.push_back("bar");
    v.push_back("baz");

    auto r = ElementsOf(v);
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
    const std::list<std::string> v(2);
    auto r = ElementsOf(v);
    ASSERT_FALSE(r.Empty());
    EXPECT_EQ("", r.Next());
    ASSERT_FALSE(r.Empty());
    EXPECT_EQ("", r.Next());
    EXPECT_TRUE(r.Empty());
}


TEST(dsb_sequence, ElementsOf)
{
    int a[] = { 3, 1, 4 };
    auto r = ElementsOf(a, 3);
    ASSERT_FALSE(r.Empty());
    EXPECT_EQ(3, r.Next());
    ASSERT_FALSE(r.Empty());
    EXPECT_EQ(1, r.Next());
    ASSERT_FALSE(r.Empty());
    EXPECT_EQ(4, r.Next());
    EXPECT_TRUE(r.Empty());
}


TEST(dsb_sequence, ValuesOf)
{
    std::map<int, std::string> m;
    m[123] = "foo";
    m[7] = "bar";
    auto s = ValuesOf(m);
    ASSERT_FALSE(s.Empty());
    EXPECT_EQ("bar", s.Next());
    ASSERT_FALSE(s.Empty());
    EXPECT_EQ("foo", s.Next());
    EXPECT_TRUE(s.Empty());
}


TEST(dsb_sequence, EmptySequence)
{
    Sequence<int> s1 = EmptySequence<int>();
    EXPECT_TRUE(s1.Empty());
}


TEST(dsb_sequence, Only)
{
    Sequence<int> s = Only(123);
    ASSERT_FALSE(s.Empty());
    EXPECT_EQ(123, s.Next());
    EXPECT_TRUE(s.Empty());
}


TEST(dsb_sequence, Only_ref)
{
    int i = 123;
    Sequence<int> s = Only(i);
    ASSERT_FALSE(s.Empty());
    EXPECT_TRUE(&i == &(s.Next()));
    EXPECT_TRUE(s.Empty());
}


TEST(dsb_sequence, Only_const_ref)
{
    const int i = 123;
    Sequence<const int> s = Only(i);
    ASSERT_FALSE(s.Empty());
    EXPECT_TRUE(&i == &(s.Next()));
    EXPECT_TRUE(s.Empty());
}


TEST(dsb_sequence, ReadOnly)
{
    std::list<std::string> v;
    v.push_back("foo");
    v.push_back("bar");
    v.push_back("baz");

    Sequence<const std::string> r = ReadOnly(ElementsOf(v));
    ASSERT_FALSE(r.Empty());
    EXPECT_EQ("foo", r.Next());
    ASSERT_FALSE(r.Empty());
    EXPECT_EQ("bar", r.Next());
    ASSERT_FALSE(r.Empty());
    EXPECT_EQ("baz", r.Next());
    EXPECT_TRUE(r.Empty());
}
