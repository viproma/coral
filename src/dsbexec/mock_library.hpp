#ifndef DSBEXEC_MOCK_LIBRARY_HPP
#define DSBEXEC_MOCK_LIBRARY_HPP

#include <memory>
#include "library.hpp"


// Creates a hardcoded mock library for testing purposes.
std::unique_ptr<dsb::library::Library> CreateMockLibrary();


#endif // header guard