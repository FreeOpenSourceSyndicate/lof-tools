#ifndef LOFTOOLS_CORE_CHARACTER_H
#define LOFTOOLS_CORE_CHARACTER_H

#include <array>
#include <cstdint>
#include <string>

namespace loftools {

// The engine's saved-character struct is 410 (0x19a) bytes.  In a ROST file it
// is followed by a 4-byte integrity trailer (stride 414); in a save game the
// records are contiguous (stride 410) and the whole file carries one checksum.
//
// Offsets below are grounded in the decompilation of LOF.EXE (character init /
// erase 0x1B95A.., character sheet builder 0x2676E, training 0x9144/0x95B0) and
// cross-checked against the shipped saves.  See docs/AGENTS.md.
constexpr size_t kCharSize = 410;

namespace char_off {
constexpr size_t kPresent    = 0x00;  // byte  non-zero: slot holds a character
constexpr size_t kName       = 0x01;  // 14    NUL-terminated name
constexpr size_t kTamperFlag = 0x0c;  // byte  0xFC marks a "fraud" character
constexpr size_t kLevel      = 0x17;  // byte  level / rank
constexpr size_t kClassFlag  = 0x18;  // byte  1 = player character, 2 = monster
constexpr size_t kProfession = 0x1b;  // byte  index into kProfessionNames (0xff = none)
constexpr size_t kDefence    = 0x1e;  // byte  total defence / armour class
constexpr size_t kHpMax      = 0x20;  // word
constexpr size_t kHpCur      = 0x22;  // word
constexpr size_t kCon        = 0x44;  // byte  Constitution
constexpr size_t kStr        = 0x45;  // byte  Strength
constexpr size_t kDex        = 0x46;  // byte  Dexterity
constexpr size_t kInt        = 0x47;  // byte  Intelligence
constexpr size_t kWis        = 0x48;  // byte  Wisdom
constexpr size_t kWeightMax  = 0x64;  // word
constexpr size_t kWeightCur  = 0x66;  // word
constexpr size_t kMpMax      = 0x68;  // byte
constexpr size_t kMpCur      = 0x69;  // byte
constexpr size_t kInvUsed    = 0x6b;  // byte  used inventory-slot count (0..48)
constexpr size_t kExperience = 0x6c;  // dword
constexpr size_t kRations    = 0x70;  // byte
constexpr size_t kGold       = 0x72;  // dword
}  // namespace char_off

// The value the engine writes into char_off::kTamperFlag when the stored
// checksum does not match the record (asm 0x2A1F6).  It is acted on at asm
// 0xB0CB (prints the "...caught... trying to fraud" message and applies a
// penalty).
constexpr uint8_t kTamperFlagValue = 0xfc;

// Profession names, indexed by char_off::kProfession (0..11).  Index 0xff
// ("none") is used by the unfinished template slot.
extern const char* const kProfessionNames[12];

// Human-readable profession for a raw index; "none" for out-of-range values.
std::string profession_name(uint8_t index);

// A decoded 410-byte character record.  The raw bytes are always authoritative;
// the accessors decode/encode in place so the struct can be used both for
// read-only display and for editing.
class Character {
public:
    Character() { raw_.fill(0); }
    explicit Character(const std::array<uint8_t, kCharSize>& raw) : raw_(raw) {}

    const std::array<uint8_t, kCharSize>& raw() const { return raw_; }
    std::array<uint8_t, kCharSize>& raw() { return raw_; }

    uint8_t byte(size_t off) const { return raw_[off]; }
    void set_byte(size_t off, uint8_t v) { raw_[off] = v; }

    uint16_t word(size_t off) const {
        return (uint16_t)(raw_[off] | ((uint16_t)raw_[off + 1] << 8));
    }
    void set_word(size_t off, uint16_t v) {
        raw_[off] = (uint8_t)(v & 0xff);
        raw_[off + 1] = (uint8_t)(v >> 8);
    }

    uint32_t dword(size_t off) const {
        return (uint32_t)raw_[off] | ((uint32_t)raw_[off + 1] << 8) |
               ((uint32_t)raw_[off + 2] << 16) | ((uint32_t)raw_[off + 3] << 24);
    }
    void set_dword(size_t off, uint32_t v) {
        raw_[off] = (uint8_t)(v & 0xff);
        raw_[off + 1] = (uint8_t)((v >> 8) & 0xff);
        raw_[off + 2] = (uint8_t)((v >> 16) & 0xff);
        raw_[off + 3] = (uint8_t)((v >> 24) & 0xff);
    }

    bool present() const { return raw_[char_off::kPresent] != 0; }
    uint8_t tamper_flag() const { return raw_[char_off::kTamperFlag]; }
    bool tampered() const { return tamper_flag() == kTamperFlagValue; }

    std::string name() const {
        std::string s;
        for (size_t i = 0; i < 14; ++i) {
            char c = (char)raw_[char_off::kName + i];
            if (c == '\0') break;
            s.push_back(c);
        }
        return s;
    }
    void set_name(const std::string& n) {
        for (size_t i = 0; i < 14; ++i)
            raw_[char_off::kName + i] = i < n.size() ? (uint8_t)n[i] : 0;
    }

    uint8_t level() const { return raw_[char_off::kLevel]; }
    uint8_t profession() const { return raw_[char_off::kProfession]; }
    std::string profession_text() const { return profession_name(profession()); }
    uint8_t defence() const { return raw_[char_off::kDefence]; }

    uint16_t hp_max() const { return word(char_off::kHpMax); }
    uint16_t hp_cur() const { return word(char_off::kHpCur); }
    uint8_t con() const { return raw_[char_off::kCon]; }
    uint8_t str() const { return raw_[char_off::kStr]; }
    uint8_t dex() const { return raw_[char_off::kDex]; }
    uint8_t intel() const { return raw_[char_off::kInt]; }
    uint8_t wis() const { return raw_[char_off::kWis]; }

    uint32_t experience() const { return dword(char_off::kExperience); }
    uint32_t gold() const { return dword(char_off::kGold); }
    uint8_t rations() const { return raw_[char_off::kRations]; }

private:
    std::array<uint8_t, kCharSize> raw_;
};

}  // namespace loftools

#endif  // LOFTOOLS_CORE_CHARACTER_H
