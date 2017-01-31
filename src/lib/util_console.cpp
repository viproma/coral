/*
Copyright 2013-2017, SINTEF Ocean and the Coral contributors.
This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#include "coral/util/console.hpp"

#include <cassert>
#include <sstream>
#include <utility>


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
