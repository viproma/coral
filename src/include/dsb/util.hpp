/**
\file
\brief Main header file for dsb::util.
*/
#ifndef DSB_UTIL_HPP
#define DSB_UTIL_HPP

#include <cstdint>
#include <string>
#include <vector>
#include "boost/filesystem/path.hpp"


namespace dsb
{

/// Misc. utilities (i.e., stuff that didn't really fit anywhere else).
namespace util
{


/// Encodes a 16-bit unsigned integer using little-endian byte order.
void EncodeUint16(uint16_t source, char target[2]);


/// Decodes a 16-bit unsigned integer using little-endian byte order.
uint16_t DecodeUint16(const char source[2]);


/// Returns a string that contains a random UUID.
std::string RandomUUID();


/**
\brief  An RAII object that creates a unique directory on construction,
        and recursively deletes it again on destruction.
*/
class TempDir
{
public:
    TempDir();
    ~TempDir();
    const boost::filesystem::path& Path() const;
private:
    boost::filesystem::path m_path;
};


/**
\brief  Starts a new process.
*/
void SpawnProcess(
    const std::string& program,
    const std::vector<std::string>& args);


}}      // namespace
#endif  // header guard
