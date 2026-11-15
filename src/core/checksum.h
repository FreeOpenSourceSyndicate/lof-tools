#ifndef LOFTOOLS_CORE_CHECKSUM_H
#define LOFTOOLS_CORE_CHECKSUM_H

#include <cstddef>
#include <cstdint>

namespace loftools {

// Legend of Faerghail's integrity fold.
//
// Recovered from LOF.EXE (routine at image offset 0x1BF18, far-called as
// 0x1BF1:0x0008) and verified byte-for-byte against every shipped ROST record
// and save game.  It is *not* a plain sum and *not* a CRC: it is an alternating
// ADD / XOR fold with a carry into the high word.
//
//   acc = 1
//   for i in 0 .. n-1:
//       c = data[i]
//       if c == 0: c = 1          # NUL bytes are folded as 1
//       if i is even: acc += c    # ADD, carry propagates into the high word
//       else:         acc ^= c    # XOR
//   return acc & 0xFFFFFFFF
//
// The result is stored little-endian (DX:AX in the engine).
uint32_t lof_checksum(const uint8_t* data, size_t n);

inline uint32_t lof_checksum_u32(uint32_t seed, const uint8_t* data, size_t n) {
    uint32_t acc = seed;
    for (size_t i = 0; i < n; ++i) {
        uint32_t c = data[i] ? data[i] : 1u;
        if ((i & 1u) == 0) acc += c; else acc ^= c;
    }
    return acc;
}

// Read a little-endian uint32 from 4 bytes.
uint32_t read_u32le(const uint8_t* p);
void write_u32le(uint8_t* p, uint32_t v);

}  // namespace loftools

#endif  // LOFTOOLS_CORE_CHECKSUM_H
