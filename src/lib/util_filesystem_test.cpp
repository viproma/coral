#include <utility>
#include <boost/filesystem.hpp>
#include <gtest/gtest.h>
#include <coral/util/filesystem.hpp>


TEST(coral_util_filesystem, TempDir)
{
    namespace fs = boost::filesystem;
    fs::path d;
    {
        auto tmp = coral::util::TempDir();
        d = tmp.Path();
        ASSERT_FALSE(d.empty());
        ASSERT_TRUE(fs::exists(d));
        EXPECT_TRUE(fs::is_directory(d));
        EXPECT_TRUE(fs::is_empty(d));

        auto tmp2 = std::move(tmp);
        EXPECT_TRUE(tmp.Path().empty());
        EXPECT_EQ(d, tmp2.Path());
        EXPECT_TRUE(fs::exists(d));

        auto tmp3 = coral::util::TempDir();
        const auto d3 = tmp3.Path();
        EXPECT_TRUE(fs::exists(d3));
        ASSERT_FALSE(fs::equivalent(d, d3));

        tmp3 = std::move(tmp2);
        EXPECT_TRUE(tmp2.Path().empty());
        EXPECT_FALSE(fs::exists(d3));
        EXPECT_EQ(d, tmp3.Path());
        EXPECT_TRUE(fs::exists(d));
    }
    EXPECT_FALSE(fs::exists(d));
}
