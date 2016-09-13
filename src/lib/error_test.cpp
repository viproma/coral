#include <cerrno>
#include "gtest/gtest.h"

#include "coral/error.hpp"


TEST(coral_error, ErrnoMessage)
{
    EXPECT_TRUE(coral::error::ErrnoMessage("", 0).empty());
    EXPECT_EQ("foo", coral::error::ErrnoMessage("foo", 0));
    const auto m1 = coral::error::ErrnoMessage("", EINVAL);
    EXPECT_LT(m1.find("nvalid"), std::string::npos);
    const auto m2 = coral::error::ErrnoMessage("foo", EINVAL);
    EXPECT_LT(m2.find("foo"), std::string::npos);
    EXPECT_LT(m2.find(m1), std::string::npos);
}

TEST(coral_error, sim_error)
{
    // Test implicit conversion from sim_error to error_code
    std::error_code code = coral::error::sim_error::cannot_perform_timestep;
    EXPECT_TRUE(code == coral::error::sim_error::cannot_perform_timestep);
    EXPECT_EQ(coral::error::sim_category(), code.category());
    EXPECT_NE(std::string::npos, code.message().find("time step"));
}
