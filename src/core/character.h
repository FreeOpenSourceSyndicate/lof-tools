#ifndef LOFTOOLS_CORE_CHARACTER_H
#define LOFTOOLS_CORE_CHARACTER_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "core/tables.h"

namespace loftools {

// The engine's saved-character struct is 410 (0x19a) bytes.  In a ROST file it
// is followed by a 4-byte integrity trailer (stride 414); in a save game the
// records are contiguous (stride 410) and the whole file carries one checksum.
//
// Offsets below are grounded in the decompilation of LOF.EXE (character init /
// erase 0x1B95A.., character sheet builder 0x2676E, training 0x9144/0x95B0) and
// cross-checked byte-for-byte against the shipped saves and the roster the user
// supplied.  Every field in the "verified" block below reproduced the user's
// hand-made character sheet exactly (7 characters, 20 fields).  See AGENTS.md.
constexpr size_t kCharSize = 410;

namespace char_off {
constexpr size_t kPresent    = 0x00;  // byte  non-zero: slot holds a character
constexpr size_t kName       = 0x01;  // 14    NUL-terminated name
constexpr size_t kTamperFlag = 0x0c;  // byte  0xFC marks a "fraud" character
constexpr size_t kLevel      = 0x17;  // byte  level / rank
constexpr size_t kSex        = 0x18;  // byte  0 = female, 1 = male
constexpr size_t kAlignment  = 0x19;  // byte  0 = lawful, 1 = chaotic
constexpr size_t kRace       = 0x1a;  // byte  0 Human .. 5 Half-Orc
constexpr size_t kProfession = 0x1b;  // byte  index into the profession table (0x0c/0xff = none)
constexpr size_t kDefence    = 0x1e;  // byte  total defence / armour class
constexpr size_t kStatus     = 0x1f;  // byte  condition index (0 Good .. 7 Dead)
constexpr size_t kHpMax      = 0x20;  // word  maximum hit points
constexpr size_t kHpCur      = 0x22;  // word  current hit points
// The nine skill values (each 0..100), in the engine's own display order.  The
// character-sheet builder reads exactly these offsets, in this order (see the
// skill-print sequence at asm 0x2717B..0x273A3).
constexpr size_t kSkillNegotiating  = 0x25;
constexpr size_t kSkillAttack       = 0x27;
constexpr size_t kSkillDefence      = 0x28;
constexpr size_t kSkillConcentration = 0x2b;
constexpr size_t kSkillPickpocket   = 0x2d;
constexpr size_t kSkillStalking     = 0x30;
constexpr size_t kSkillTrapDetect   = 0x32;
constexpr size_t kSkillTrapDisarm   = 0x34;
constexpr size_t kSkillLockpicking  = 0x36;
constexpr size_t kCon        = 0x44;  // byte  Constitution
constexpr size_t kStr        = 0x45;  // byte  Strength
constexpr size_t kDex        = 0x46;  // byte  Dexterity
constexpr size_t kInt        = 0x47;  // byte  Intelligence
constexpr size_t kWis        = 0x48;  // byte  Wisdom
constexpr size_t kWeightMax  = 0x64;  // word  max carryable weight (game shows /10)
constexpr size_t kWeightCur  = 0x66;  // word  currently carried weight (game shows /10)
constexpr size_t kMpMax      = 0x68;  // byte  maximum magic points
constexpr size_t kMpCur      = 0x69;  // byte  current magic points
constexpr size_t kSpellCount = 0x6a;  // byte  number of learned spells
constexpr size_t kInvUsed    = 0x6b;  // byte  used inventory-slot count (0..48)
constexpr size_t kExperience = 0x6c;  // int32 experience (signed; can be negative)
constexpr size_t kRations    = 0x70;  // byte  rations
constexpr size_t kGold       = 0x72;  // uint32 gold in the purse
constexpr size_t kDeposit    = 0x76;  // uint32 gold on deposit (the bank)
// The eight language flags, one byte each: non-zero => the character knows that
// language.  Order matches the language table (Common, Animal, Orc, ...).
constexpr size_t kLanguages  = 0x7a;
// The 48 four-byte inventory slots.
constexpr size_t kInventory  = 0x82;
constexpr size_t kInventorySlots = 48;
constexpr size_t kInventoryStride = 4;
// The spell array: 34 slots of (spell index, remaining casts) starting at
// 0x142.  A spell index of 0 is an empty slot.  The width is proven two ways:
// the engine's spell scan loop runs `cmp word ptr [bp-0x34], 0x22` (34) over
// `es:[bx+si+0x142]` with `bx = slot*2` (asm 0x1387D/0x1389E), and
// 0x142 + 34*2 = 0x186, exactly where the next character field begins (asm
// 0x29195).  `kSpellCount` (0x6a) says how many of the slots are in use.
constexpr size_t kSpells     = 0x142;
constexpr size_t kSpellSlots = 34;  // 0x142 .. 0x185 inclusive
}  // namespace char_off

// The value the engine writes into char_off::kTamperFlag when the stored
// checksum does not match the record (asm 0x2A1F6).  It is acted on at asm
// 0xB0CB (prints the "...caught... trying to fraud" message and applies a
// penalty).
constexpr uint8_t kTamperFlagValue = 0xfc;

// One decoded inventory slot (4 bytes).  See char_off::kInventory.
struct InvSlot {
    uint8_t item = 0;       // 1-based item-table index (0 = empty slot)
    uint8_t equipped = 0;   // 1 = worn / wielded, 0 = carried only
    uint8_t charges = 0;    // remaining count / charges
    uint8_t condition = 0;  // durability 0..100 (100 = pristine, 0 = broken)
};

// One decoded spell entry (2 bytes).  See char_off::kSpells.
struct SpellSlot {
    uint8_t id = 0;      // 1-based spell-table index (0 = empty)
    uint8_t casts = 0;   // remaining casts
};

// How sure we are about a field's meaning.  The editor only exposes fields that
// are at least kLikely; kUncertain fields are reported but not offered for
// editing (see character_fields()).
enum class FieldConfidence { Firm, Likely, Uncertain };

// A single editable/displayable character field.  The editor builds its form
// from a table of these, so a field can be added or hidden in one place.
struct FieldInfo {
    const char* key;         // stable identifier
    const char* label;       // human-readable label
    const char* group;       // form group (e.g. "Identity", "Attributes")
    FieldConfidence confidence;
    const char* note;        // optional explanation (may be empty)
};

// The field descriptors the editor iterates.  Ordered as they should appear.
const std::vector<FieldInfo>& character_fields();

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
    int32_t sword(size_t off) const { return (int32_t)dword(off); }
    void set_sword(size_t off, int32_t v) { set_dword(off, (uint32_t)v); }

    bool present() const { return raw_[char_off::kPresent] != 0; }
    void set_present(bool p) { raw_[char_off::kPresent] = p ? 1 : 0; }

    uint8_t tamper_flag() const { return raw_[char_off::kTamperFlag]; }
    bool tampered() const { return tamper_flag() == kTamperFlagValue; }
    void clear_tamper_flag() { raw_[char_off::kTamperFlag] = 0; }

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

    // --- identity ---
    uint8_t level() const { return raw_[char_off::kLevel]; }
    void set_level(uint8_t v) { raw_[char_off::kLevel] = v; }
    uint8_t sex() const { return raw_[char_off::kSex]; }
    void set_sex(uint8_t v) { raw_[char_off::kSex] = v; }
    uint8_t alignment() const { return raw_[char_off::kAlignment]; }
    void set_alignment(uint8_t v) { raw_[char_off::kAlignment] = v; }
    uint8_t race() const { return raw_[char_off::kRace]; }
    void set_race(uint8_t v) { raw_[char_off::kRace] = v; }
    uint8_t profession() const { return raw_[char_off::kProfession]; }
    void set_profession(uint8_t v) { raw_[char_off::kProfession] = v; }
    uint8_t defence() const { return raw_[char_off::kDefence]; }

    // --- vital statistics ---
    // The condition byte.  It indexes the engine's status table (0 Good, 1 Mad,
    // 2 Ill, 3 Poisoned, 4 Blind, 5 Stunned, 6 Stoned, 7 Dead).  Death sets it
    // to 7 while leaving max HP alone; a *failed* temple revive zeroes both HP
    // fields and leaves this at 7.  See docs/character-status-and-revive.md.
    uint8_t status() const { return raw_[char_off::kStatus]; }
    void set_status(uint8_t v) { raw_[char_off::kStatus] = v; }
    bool is_dead() const { return status() == kStatusDead; }

    // A character that died and then had a temple revive *fail*: status is Dead
    // but max HP is 0.  Normal death never zeroes max HP, and the revive
    // routine refuses a corpse with max HP 0 (asm 0xA7AA), so such a character
    // can never be revived again in game.  The lost max-HP value is not stored
    // anywhere in the 410-byte struct.  The character editor restores a
    // plausible value via core/character_revive.h; prefer that module over this
    // predicate.
    bool unrevivable() const { return is_dead() && hp_max() == 0; }

    uint16_t hp_max() const { return word(char_off::kHpMax); }
    void set_hp_max(uint16_t v) { set_word(char_off::kHpMax, v); }
    uint16_t hp_cur() const { return word(char_off::kHpCur); }
    void set_hp_cur(uint16_t v) { set_word(char_off::kHpCur, v); }
    uint8_t mp_max() const { return raw_[char_off::kMpMax]; }
    void set_mp_max(uint8_t v) { raw_[char_off::kMpMax] = v; }
    uint8_t mp_cur() const { return raw_[char_off::kMpCur]; }
    void set_mp_cur(uint8_t v) { raw_[char_off::kMpCur] = v; }

    // --- attributes ---
    uint8_t str() const { return raw_[char_off::kStr]; }
    void set_str(uint8_t v) { raw_[char_off::kStr] = v; }
    uint8_t con() const { return raw_[char_off::kCon]; }
    void set_con(uint8_t v) { raw_[char_off::kCon] = v; }
    uint8_t dex() const { return raw_[char_off::kDex]; }
    void set_dex(uint8_t v) { raw_[char_off::kDex] = v; }
    uint8_t intel() const { return raw_[char_off::kInt]; }
    void set_intel(uint8_t v) { raw_[char_off::kInt] = v; }
    uint8_t wis() const { return raw_[char_off::kWis]; }
    void set_wis(uint8_t v) { raw_[char_off::kWis] = v; }

    // --- money and encumbrance ---
    int32_t experience() const { return sword(char_off::kExperience); }
    void set_experience(int32_t v) { set_sword(char_off::kExperience, v); }
    uint32_t gold() const { return dword(char_off::kGold); }
    void set_gold(uint32_t v) { set_dword(char_off::kGold, v); }
    uint32_t deposit() const { return dword(char_off::kDeposit); }
    void set_deposit(uint32_t v) { set_dword(char_off::kDeposit, v); }
    uint8_t rations() const { return raw_[char_off::kRations]; }
    void set_rations(uint8_t v) { raw_[char_off::kRations] = v; }
    uint16_t weight_max() const { return word(char_off::kWeightMax); }
    uint16_t weight_cur() const { return word(char_off::kWeightCur); }

    // --- skills (0..8, in character_fields()-defined order) ---
    uint8_t skill(size_t index) const {
        return index < 9 ? raw_[skill_offsets()[index]] : 0;
    }
    void set_skill(size_t index, uint8_t v) {
        if (index < 9) raw_[skill_offsets()[index]] = v;
    }

    // --- languages (one flag byte per language) ---
    bool knows_language(size_t index) const {
        return index < 8 && raw_[char_off::kLanguages + index] != 0;
    }
    void set_language(size_t index, bool known) {
        if (index < 8) raw_[char_off::kLanguages + index] = known ? 1 : 0;
    }

    // --- inventory ---
    uint8_t inv_used() const { return raw_[char_off::kInvUsed]; }
    InvSlot inventory(size_t slot) const {
        InvSlot s;
        if (slot < char_off::kInventorySlots) {
            const size_t o = char_off::kInventory + slot * char_off::kInventoryStride;
            s.item = raw_[o];
            s.equipped = raw_[o + 1];
            s.charges = raw_[o + 2];
            s.condition = raw_[o + 3];
        }
        return s;
    }
    void set_inventory(size_t slot, const InvSlot& s) {
        if (slot < char_off::kInventorySlots) {
            const size_t o = char_off::kInventory + slot * char_off::kInventoryStride;
            raw_[o] = s.item;
            raw_[o + 1] = s.equipped;
            raw_[o + 2] = s.charges;
            raw_[o + 3] = s.condition;
        }
    }

    // --- spells ---
    // The count of learned spells.  The engine never recomputes it: it is
    // raised when a spell is learned and left alone when a spell is used up, so
    // a slot can be blank while still being counted (the engine zeroes the id
    // once the last cast is spent, asm 0x12A5E).  Edit it explicitly.
    uint8_t spell_count() const { return raw_[char_off::kSpellCount]; }
    void set_spell_count(uint8_t v) { raw_[char_off::kSpellCount] = v; }
    SpellSlot spell(size_t slot) const {
        SpellSlot s;
        if (slot < char_off::kSpellSlots) {
            s.id = raw_[char_off::kSpells + slot * 2];
            s.casts = raw_[char_off::kSpells + slot * 2 + 1];
        }
        return s;
    }
    void set_spell(size_t slot, SpellSlot s) {
        if (slot < char_off::kSpellSlots) {
            raw_[char_off::kSpells + slot * 2] = s.id;
            raw_[char_off::kSpells + slot * 2 + 1] = s.casts;
        }
    }

private:
    static const size_t* skill_offsets() {
        static const size_t offs[9] = {
            char_off::kSkillNegotiating, char_off::kSkillAttack,
            char_off::kSkillDefence, char_off::kSkillConcentration,
            char_off::kSkillPickpocket, char_off::kSkillStalking,
            char_off::kSkillTrapDetect, char_off::kSkillTrapDisarm,
            char_off::kSkillLockpicking};
        return offs;
    }

    std::array<uint8_t, kCharSize> raw_;
};

}  // namespace loftools

#endif  // LOFTOOLS_CORE_CHARACTER_H
