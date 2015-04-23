#include <sstream>
#include "gtest/gtest.h"
#include "dsb/fmilib/importcontext.hpp"
#include "dsb/fmilib/streamlogger.hpp"


TEST(dsb_fmilib, ImportContext_basic_functionality) {
    auto c = dsb::fmilib::MakeImportContext();
    ASSERT_NE(nullptr, c->Handle());
    EXPECT_EQ(jm_log_level_info, c->LogLevel());
    c->SetLogLevel(jm_log_level_warning);
    EXPECT_EQ(jm_log_level_warning, c->LogLevel());
    EXPECT_TRUE(c->LastErrorMessage().empty());
}

// NOTE: This test is somewhat fragile, since we don't have any control over
// how (or even whether) FMI Library logs context allocation.  If it turns
// out to break often, we just remove it again.
TEST(dsb_fmilib, ImportContext_verbose_logging) {
    auto s = std::make_shared<std::stringstream>();
    auto l = std::make_shared<dsb::fmilib::StreamLogger>(s);
    auto c = dsb::fmilib::MakeImportContext(l, jm_log_level_verbose);
    ASSERT_NE(nullptr, c->Handle());
    EXPECT_EQ(jm_log_level_verbose, c->LogLevel());
    EXPECT_EQ("VERBOSE: Allocating FMIL context\n", s->str());
}
