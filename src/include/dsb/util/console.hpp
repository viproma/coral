/**
\file
\brief  Utilities for writing console applications
*/
#ifndef DSB_UTIL_CONSOLE_HPP
#define DSB_UTIL_CONSOLE_HPP

#include <ostream>
#include <string>
#include <vector>
#include "boost/optional.hpp"
#include "boost/program_options.hpp"

namespace dsb
{
namespace util
{

/**
\brief  Returns a string vector with the same contents as the standard C
        program argument array.
*/
std::vector<std::string> CommandLine(int argc, char const *const * argv);


/**
\brief  Parses program arguments and, if necessary, prints a help message.

This is a convenience function which takes two sets of program options,
`options` and `positionalOptions`, where the former contains the ones that
should be specified by the user with switches (e.g. `--foo`) and the latter
contains the ones that are specified using "normal" (positional) arguments,
and performs the following actions:

 1. Adds the `--help` option to `options`.
 2. Parses the arguments given in `args`, mapping them to options specified
    in `options` and `positionalOptions`.
 3. If the `--help` option was specified, or positional arguments were
    expected and `args` is empty, prints a help message and returns an
    empty/false object.
 4. Otherwise, returns the mapped option values.

 If an empty/false object is returned, it is recommended that the program
 exits more or less immediately.

 The help message is constructed using `boost::program_options` own formatted
 output functions, so it is recommended to include helpful option descriptions
 when adding options to `options` and `positionalOptions`.

\param [in] args
    The command-line arguments as they were passed to the program
    (not including the program name).
\param [in] options
    The options that should be specified with command-line switches
    (i.e., --switch or -s).
\param [in] positionalOptions
    The options that should be interpreted as positional arguments.
\param [in] positions
    An object that describes how to map positional arguments to options.
\param [in,out] helpOutput
    The output stream to which a help message should be written.
\param [in] commandName
    The command name, as it should be displayed in the help message.
\param [in] commandDescription
    A description of what the command does, for the help message.
\param [in] extraHelp
    Text to output below the standard help message.

\returns
    A map which contains the parsed program options, or, if --help was
    specified or no arguments were given, an empty/false value.
*/
boost::optional<boost::program_options::variables_map> ParseArguments(
    const std::vector<std::string>& args,
    boost::program_options::options_description options,
    const boost::program_options::options_description& positionalOptions,
    const boost::program_options::positional_options_description& positions,
    std::ostream& helpOutput,
    const std::string& commandName,
    const std::string& commandDescription,
    const std::string& extraHelp = std::string());

}} // namespace
#endif // header guard
