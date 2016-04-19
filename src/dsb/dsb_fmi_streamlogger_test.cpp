#include <sstream>
#include "gtest/gtest.h"
#include "fmilib.h"
#include "dsb/fmi/streamlogger.hpp"


TEST(dsb_fmi, StreamLogger_default_format) {
    auto stream = std::make_shared<std::stringstream>();
    auto logger = std::make_shared<dsb::fmi::StreamLogger>(stream);
    logger->Log("mod1", jm_log_level_error, "message1");
    logger->Log("mod2", jm_log_level_warning, "message2");
    EXPECT_EQ(std::string("ERROR: message1\nWARNING: message2\n"), stream->str());
}


TEST(dsb_fmi, StreamLogger_custom_format) {
    auto stream = std::make_shared<std::stringstream>();
    auto logger = std::make_shared<dsb::fmi::StreamLogger>(stream, "%2$03d-%3$s: %4$s (%1$s); ");
    logger->Log("mod1", jm_log_level_error, "message1");
    logger->Log("mod2", jm_log_level_warning, "message2");
    EXPECT_EQ(
        std::string("002-ERROR: message1 (mod1); 003-WARNING: message2 (mod2); "),
        stream->str());
}
