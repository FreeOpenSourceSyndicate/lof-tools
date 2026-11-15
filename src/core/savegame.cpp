#include "core/savegame.h"

#include <cstring>

#include "core/checksum.h"

namespace loftools {

void SaveGame::recompute() {
    if (raw.size() >= kSaveStateSize) {
        actual_checksum = lof_checksum(raw.data(), kSaveStateSize);
    }
    checksum_ok = (raw.size() >= kSaveStateSize + kSaveChecksumSize) &&
                  (actual_checksum == stored_checksum);
}

bool SaveGame::intact() const {
    return checksum_ok && tampered_party_count() == 0;
}

size_t SaveGame::tampered_party_count() const {
    size_t n = 0;
    for (const auto& c : party) {
        if (c.present() && c.tampered()) ++n;
    }
    return n;
}

SaveGame parse_save(const std::vector<uint8_t>& file) {
    SaveGame out;
    out.raw = file;

    if (file.size() < kSaveHeaderSize + kSaveChecksumSize) {
        out.error = "file is too small to be a save game";
        return out;
    }
    if (std::memcmp(file.data(), kSaveMagic, 4) != 0) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "bad magic (got %02x %02x %02x %02x, want 00 46 4F 4C)",
                      file[0], file[1], file[2], file[3]);
        out.error = buf;
        return out;
    }
    if (file.size() < kSaveSize) {
        out.error = "file is shorter than the expected " + std::to_string(kSaveSize) +
                    " bytes";
        return out;
    }

    out.ok = true;

    for (size_t i = 0; i < kSavePartySlots; ++i) {
        std::array<uint8_t, kCharSize> raw{};
        const size_t off = kSaveHeaderSize + i * kCharSize;
        std::memcpy(raw.data(), file.data() + off, kCharSize);
        out.party[i] = Character(raw);
    }

    out.stored_checksum = read_u32le(file.data() + kSaveStateSize);
    out.loc_ones = file[kSaveLocOnes];
    out.loc_tens = file[kSaveLocTens];
    out.facing = file[kSaveFacing];
    out.recompute();
    return out;
}

std::vector<uint8_t> serialize_save(const SaveGame& save) {
    std::vector<uint8_t> out = save.raw;
    if (out.size() < kSaveStateSize + kSaveChecksumSize) {
        out.resize(kSaveSize, 0);
    }
    // Write the party characters back into the state block.
    for (size_t i = 0; i < kSavePartySlots; ++i) {
        const size_t off = kSaveHeaderSize + i * kCharSize;
        std::memcpy(out.data() + off, save.party[i].raw().data(), kCharSize);
    }
    // Refresh the whole-file checksum.
    uint8_t tail[4];
    write_u32le(tail, lof_checksum(out.data(), kSaveStateSize));
    std::memcpy(out.data() + kSaveStateSize, tail, 4);
    return out;
}

}  // namespace loftools
