/*
Copyright 2013-present, SINTEF Ocean.
This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#include <coral/util/console.hpp>

#ifdef _WIN32
#   include <process.h>
#else
#   include <unistd.h>
#endif

#include <cassert>
#include <fstream>
#include <sstream>
#include <utility>

#include <boost/filesystem.hpp>

#include <coral/log.hpp>
#include <coral/util.hpp>


std::vector<std::string> coral::util::CommandLine(int argc, char const *const * argv)
{
    std::vector<std::string> args;
    args.reserve(argc);
    for (int i = 0; i < argc; ++i) args.emplace_back(argv[i]);
    return args;
}


namespace
{
    void ReplaceAll(
        std::string& str,
        const std::string& lookFor,
        const std::string& replaceWith)
    {
        for (auto i = str.find(lookFor); i != std::string::npos; i = str.find(lookFor, i)) {
            str.replace(i, lookFor.size(), replaceWith);
            i += replaceWith.size();
        }
    }

    std::string SwitchesToArgs(
        const boost::program_options::options_description& args)
    {
        // This makes some assumptions about how Boost Program Options formats
        // its output.
        std::stringstream ss;
        ss << args;
        auto s = ss.str();
        ReplaceAll(s, "  --", "    "); // Drop "--" from all switches
        ReplaceAll(s, "\n  ", "\n");       // Reduce indentation by 2 spaces
        ReplaceAll(s, " arg ", "     ");   // Drop "arg" from all switches
        return s;
    }

    void PrintHelp(
        std::ostream& out,
        const boost::program_options::options_description& options,
        const boost::program_options::options_description& positionalOptions,
        const boost::program_options::positional_options_description& positions,
        const std::string& commandName,
        const std::string& commandDescriptionHelp,
        const std::string& extraHelp)
    {
        // Header / command description.
        out << commandDescriptionHelp << '\n';

        // "Usage" section
        out << '\n'
            << "Usage:\n"
            << "  " << commandName;
        if (positions.max_total_count() > 0) {
            out << " <";
            for (unsigned i = 0; i < positions.max_total_count(); ++i) {
                if (i > 0) {
                    if (positions.name_for_position(i) == positions.name_for_position(i-1)) {
                        out << "...";
                        break;
                    } else {
                        out << "> <";
                    }
                }
                out << positions.name_for_position(i);
            }
            out << '>';
        }
        out << " [options...]\n";

        // "Arguments" section
        if (!positionalOptions.options().empty()) {
            out
                << '\n'
                << SwitchesToArgs(positionalOptions); // yeah, this is somewhat inelegant
        }

        // "Options" section
        out
            << '\n'
            << options;

        // Additional sections
        if (!extraHelp.empty()) {
            out
                << '\n'
                << extraHelp << (extraHelp.back() == '\n' ? "" : "\n");
        }
    }
}


boost::optional<boost::program_options::variables_map> coral::util::ParseArguments(
    const std::vector<std::string>& args,
    boost::program_options::options_description options,
    const boost::program_options::options_description& positionalOptions,
    const boost::program_options::positional_options_description& positions,
    std::ostream& helpOutput,
    const std::string& commandName,
    const std::string& commandDescriptionHelp,
    const std::string& extraHelp)
{
    namespace po = boost::program_options;
    options.add_options()("help", "Display a help message and exit.");

    po::options_description allArgs;
    allArgs.add(options);
    allArgs.add(positionalOptions);
    po::variables_map values;
    po::store(
        po::command_line_parser(args).options(allArgs).positional(positions).run(),
        values);

    if ((args.empty() && !positionalOptions.options().empty()) || values.count("help")) {
        PrintHelp(helpOutput,
            options, positionalOptions, positions,
            commandName, commandDescriptionHelp, extraHelp);
        return boost::none;
    } else {
        return std::move(values);
    }
}


void coral::util::AddLoggingOptions(
    boost::program_options::options_description& options)
{
    namespace po = boost::program_options;
    options.add_options()
        ("log-level", po::value<std::string>()->default_value("warning"),
            "The lowest level of messages to log. Available levels are, from "
            "lowest to highest: trace, debug, info, warning, error.  Note that "
            "certain trace and debug messages are only printed if the program "
            "itself was compiled in debug mode.")
        ("log-file",
            "Enable logging to file.")
        ("log-file-dir", po::value<std::string>()->default_value("."),
            "Output directory for log files.")
        ;
}


void coral::util::UseLoggingArguments(
    const boost::program_options::variables_map& arguments,
    const std::string& logFilePrefix)
{
    const auto logLevel =
        coral::log::ParseLevel(arguments["log-level"].as<std::string>());
    coral::log::AddSink(coral::log::CLogPtr(), logLevel);

    if (arguments.count("log-file")) {
        const auto logFileDir =
            boost::filesystem::path(arguments["log-file-dir"].as<std::string>());
        boost::filesystem::create_directories(logFileDir);
        const auto logFileName =
            logFilePrefix + "_"
            + std::to_string(getpid()) + "_"
            + coral::util::Timestamp()
            + ".log";
        coral::log::AddSink(
            std::make_shared<std::ofstream>((logFileDir/logFileName).string()),
            logLevel);
    }

}
