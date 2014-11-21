#ifndef DSBEXEC_CONFIG_PARSER_HPP
#define DSBEXEC_CONFIG_PARSER_HPP

#include <string>
#include "dsb/domain/controller.hpp"
#include "dsb/execution/controller.hpp"


/**
\brief  Sets up the system to be simulated in an execution based on a
        configuration file.

\param [in] path        The path to the configuration file.
\param [in] execution   The execution controller.

\throws std::runtime_error if there were errors in the configuraiton file.
*/
//TODO: Split this into two functions: one which reads the configuration
// and one which applies it to the controller.
void ParseSystemConfig(
    const std::string& path,
    dsb::domain::Controller& domain,
    dsb::execution::Controller& execution,
    const dsb::execution::Locator& executionLocator);


/// Configuration parameters for an execution run.
struct ExecutionConfig
{
    ExecutionConfig();
    double startTime;   ///< Simulation start time.
    double stopTime;    ///< Simulation stop time.
    double stepSize;    ///< Simulation step size.
};


/**
\brief  Parses an execution configuration file.
\throws std::runtime_error if there were errors in the configuration file.
*/
ExecutionConfig ParseExecutionConfig(const std::string& path);


#endif // header guard
