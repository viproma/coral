#include <cerrno>
#include "gtest/gtest.h"

#include "dsb/error.hpp"


TEST(dsb_error, ErrnoMessage)
{
    EXPECT_TRUE(dsb::error::ErrnoMessage("", 0).empty());
    EXPECT_EQ("foo", dsb::error::ErrnoMessage("foo", 0));
    const auto m1 = dsb::error::ErrnoMessage("", EINVAL);
    EXPECT_LT(m1.find("nvalid"), std::string::npos);
    const auto m2 = dsb::error::ErrnoMessage("foo", EINVAL);
    EXPECT_LT(m2.find("foo"), std::string::npos);
    EXPECT_LT(m2.find(m1), std::string::npos);
}

TEST(dsb_error, sim_error)
{
    // Test implicit conversion from sim_error to error_code
    std::error_code code = dsb::error::sim_error::cannot_perform_timestep;
    EXPECT_TRUE(code == dsb::error::sim_error::cannot_perform_timestep);
    EXPECT_EQ(dsb::error::sim_category(), code.category());
    EXPECT_NE(std::string::npos, code.message().find("time step"));
}
