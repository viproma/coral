#include "gtest/gtest.h"

#include "dsb/error.hpp"


TEST(dsb_error, sim_error)
{
    // Test implicit conversion from sim_error to error_code
    std::error_code code = dsb::error::sim_error::cannot_perform_timestep;
    EXPECT_TRUE(code == dsb::error::sim_error::cannot_perform_timestep);
    EXPECT_EQ(dsb::error::sim_category(), code.category());
    EXPECT_NE(std::string::npos, code.message().find("time step"));
}
