#ifndef LOFTOOLS_CORE_SAVEGAME_H
#define LOFTOOLS_CORE_SAVEGAME_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "core/character.h"

namespace loftools {

// A saved game (GAMES/GAMEn).  Layout, verified against the shipped saves and
// the engine's loader (asm 0x1C426..):
//
//   off 0      : 4-byte header  {0x00, 'F', 'O', 'L'}
//   off 4      : 6 party characters, 410 bytes each (the same struct ROST uses)
//   ...        : further game state (location, facing, map cache, ...)
//   off 11834  : 4-byte little-endian integrity checksum of bytes [0, 11834)
//   off 11838  : 1296-byte (36x36) map region, NOT covered by the checksum
//
// Total size is 11834 + 4 + 1296 = 13134 bytes.
//
// The engine validates the whole-file checksum on load (asm 0x1C823, comparing
// lof_checksum(state, 11834) with the stored DX:AX); on a mismatch it prints
// " is not a valid game:" and refuses the file.  Characters additionally carry
// the 0xFC tamper flag at byte 0x0C, acted on at asm 0xB0CB.
constexpr size_t kSaveHeaderSize   = 4;
constexpr size_t kSavePartySlots   = 6;
constexpr size_t kSaveStateSize    = 11834;  // bytes covered by the checksum
constexpr size_t kSaveChecksumSize = 4;
constexpr size_t kSaveMapOffset    = kSaveStateSize + kSaveChecksumSize;  // 11838
constexpr size_t kSaveMapSize      = 1296;   // 36 * 36
constexpr size_t kSaveSize         = kSaveMapOffset + kSaveMapSize;       // 13134

constexpr uint8_t kSaveMagic[4] = {0x00, 'F', 'O', 'L'};

// Party-character field offsets that are meaningful inside a save (relative to
// the whole file).
constexpr size_t kSaveLocOnes = 0x9a2;  // byte  ones digit of the dungeon id
constexpr size_t kSaveLocTens = 0x9a4;  // byte  tens digit of the dungeon id
constexpr size_t kSaveFacing  = 0x9a6;  // byte  party facing (0=N .. 3=W)
constexpr uint8_t kSaveFacingInvalid = 0xff;

struct SaveGame {
    bool ok = false;          // header magic and size are sane
    std::string error;        // human-readable reason when !ok

    std::vector<uint8_t> raw;  // the whole file, authoritative

    std::array<Character, kSavePartySlots> party;

    uint32_t stored_checksum = 0;   // trailer as found in the file
    uint32_t actual_checksum = 0;   // lof_checksum() of raw[0, kSaveStateSize)
    bool checksum_ok = false;

    uint8_t loc_ones = 0;
    uint8_t loc_tens = 0;
    uint8_t facing = kSaveFacingInvalid;

    // Recompute the whole-file checksum from the current bytes.
    void recompute();

    // True when the checksum matches and no party member carries the tamper flag.
    bool intact() const;
    // Party members whose byte 0x0C carries the 0xFC marker.
    size_t tampered_party_count() const;

    uint16_t location() const {
        return (uint16_t)(loc_tens * 10 + loc_ones);
    }
};

// Parse a save image held in memory.  Never throws; sets ok=false + error on
// malformed input.
SaveGame parse_save(const std::vector<uint8_t>& file);

// Serialize back to a byte image (updates the checksum trailer from raw).
std::vector<uint8_t> serialize_save(const SaveGame& save);

}  // namespace loftools

#endif  // LOFTOOLS_CORE_SAVEGAME_H
