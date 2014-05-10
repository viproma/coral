#ifndef DSB_UTIL_ENCODING_HPP
#define DSB_UTIL_ENCODING_HPP

#include <cstdint>

namespace dsb { namespace util
{

/// Encodes a 16-bit unsigned integer using little-endian byte order.
void EncodeUint16(uint16_t source, char target[2]);

/// Decodes a 16-bit unsigned integer using little-endian byte order.
uint16_t DecodeUint16(const char source[2]);

}}      // namespace
#endif  // header guard
