#include "dsb/util/encoding.hpp"
#include <cstdio>

void dsb::util::EncodeUint16(uint16_t source, char target[2])
{
    target[0] = source & 0xFF;
    target[1] = (source >> 8) & 0xFF;
}


uint16_t dsb::util::DecodeUint16(const char source[2])
{
    return static_cast<unsigned char>(source[0])
        | (static_cast<unsigned char>(source[1]) << 8);
}
