#include <sstream>
#include <gtest/gtest.h>
#include <coral/util/console.hpp>


TEST(coral_util, CommandLine)
{
    const char *const *const argv0 = nullptr;
    const int argc0 = 0;
    EXPECT_TRUE(coral::util::CommandLine(argc0, argv0).empty());

    const char* argv2[2] = { "hello", "world" };
    const int argc2 = 2;
    const auto cmd2 = coral::util::CommandLine(argc2, argv2);
    EXPECT_EQ(argc2, static_cast<int>(cmd2.size()));
    EXPECT_EQ(argv2[0], cmd2[0]);
    EXPECT_EQ(argv2[1], cmd2[1]);
}


TEST(coral_util, ParseArguments)
{
    using namespace coral::util;
    namespace po = boost::program_options;

    std::vector<std::string> args;
    po::options_description options;
    po::options_description positionalOptions;
    po::positional_options_description positions;
    std::stringstream out;

    std::string commandName = "test";
    std::string commandDescription = "This is a test";

    auto vals = ParseArguments(
        args, options, positionalOptions, positions,
        out, commandName, commandDescription);
    EXPECT_TRUE(!!vals);
    EXPECT_TRUE(vals->empty());

    options.add_options()("switch", po::value<int>());
    vals = ParseArguments(
        args, options, positionalOptions, positions,
        out, commandName, commandDescription);
    EXPECT_TRUE(!!vals);
    EXPECT_TRUE(vals->empty());

    positionalOptions.add_options()("arg", po::value<std::string>());
    positions.add("arg", 1);
    vals = ParseArguments(
        args, options, positionalOptions, positions,
        out, commandName, commandDescription);
    EXPECT_FALSE(!!vals);

    args.push_back("foo");
    vals = ParseArguments(
        args, options, positionalOptions, positions,
        out, commandName, commandDescription);
    EXPECT_TRUE(!!vals);
    EXPECT_EQ(1u, vals->size());
    EXPECT_EQ(1u, vals->count("arg"));
    EXPECT_EQ("foo", (*vals)["arg"].as<std::string>());

    args.push_back("--switch=42");
    vals = ParseArguments(
        args, options, positionalOptions, positions,
        out, commandName, commandDescription);
    EXPECT_TRUE(!!vals);
    EXPECT_EQ(2u, vals->size());
    EXPECT_EQ(1u, vals->count("arg"));
    EXPECT_EQ(1u, vals->count("switch"));
    EXPECT_EQ("foo", (*vals)["arg"].as<std::string>());
    EXPECT_EQ(42, (*vals)["switch"].as<int>());

    args.push_back("--help");
    vals = ParseArguments(
        args, options, positionalOptions, positions,
        out, commandName, commandDescription);
    EXPECT_FALSE(!!vals);
}
