#ifndef DSBEXEC_MODEL_PARSER_HPP
#define DSBEXEC_MODEL_PARSER_HPP

#include <string>
#include "library.hpp"
#include "model.hpp"


/**
\brief  Constructs a model based on a configuration file.

\param [in] path        The path to the configuration file.
\param [in] library     The slave library.

\throws std::runtime_error if there were errors in the model file.
*/
dsb::model::Model ParseModelFile(const std::string& path,
                                 dsb::library::Library& library);

#endif // header guard
