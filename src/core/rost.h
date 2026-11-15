#ifndef LOFTOOLS_CORE_ROST_H
#define LOFTOOLS_CORE_ROST_H

#include <cstdint>
#include <string>
#include <vector>

#include "core/character.h"

namespace loftools {

// A ROST/ROST file: one count byte, then N records of 414 bytes.
//
// Each record is a 410-byte character struct followed by a 4-byte integrity
// trailer: the little-endian lof_checksum() of the preceding 410 bytes.  The
// engine validates the trailer when it loads the roster; on a mismatch it sets
// character byte 0x0C to 0xFC, which the play loop treats as cheating.
constexpr size_t kRostRecordSize = 414;

// Per-record integrity state.
enum class RecordStatus {
    Ok,        // trailer matches and the tamper flag is clear
    BadChecksum,  // trailer does not match the record bytes
    Tampered,  // trailer matches but byte 0x0C still carries the 0xFC marker
};

struct RostRecord {
    Character character;
    uint32_t stored_checksum = 0;   // trailer as found in the file
    uint32_t actual_checksum = 0;   // lof_checksum() of the 410 struct bytes
    RecordStatus status = RecordStatus::Ok;

    bool present() const { return character.present(); }
    const std::string name() const { return character.name(); }

    // Recompute the trailer from the record bytes.
    void recompute();
};

struct RostFile {
    bool ok = false;                 // header/size sane
    std::string error;               // human-readable reason when !ok
    uint8_t declared_count = 0;      // the leading count byte
    std::vector<RostRecord> records; // decoded records (declared_count of them)
    bool count_matches = true;       // declared_count == (size-1)/414

    // True when every present record passes the integrity check.
    bool all_intact() const;
    // Records that need repair (bad checksum and/or stale tamper flag).
    size_t damaged_count() const;
};

// Parse a ROST image held in memory.  `file` must include the leading count
// byte.  Never throws; sets `out.ok=false` with an `error` on malformed input.
RostFile parse_rost(const std::vector<uint8_t>& file);

// Serialize back to a byte image (count byte + records, with trailers).
std::vector<uint8_t> serialize_rost(const RostFile& rost);

}  // namespace loftools

#endif  // LOFTOOLS_CORE_ROST_H
