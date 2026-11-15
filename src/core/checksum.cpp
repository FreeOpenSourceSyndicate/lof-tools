#include "core/checksum.h"

namespace loftools {

uint32_t lof_checksum(const uint8_t* data, size_t n) {
    return lof_checksum_u32(1u, data, n);
}

uint32_t read_u32le(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

void write_u32le(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xff);
    p[1] = (uint8_t)((v >> 8) & 0xff);
    p[2] = (uint8_t)((v >> 16) & 0xff);
    p[3] = (uint8_t)((v >> 24) & 0xff);
}

}  // namespace loftools
