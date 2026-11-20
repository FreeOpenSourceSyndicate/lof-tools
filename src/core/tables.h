#ifndef LOFTOOLS_CORE_TABLES_H
#define LOFTOOLS_CORE_TABLES_H

#include <cstddef>
#include <cstdint>
#include <string>

namespace loftools {

// ---------------------------------------------------------------------------
// Engine name/data tables, transcribed from LOF.EXE.  The item and spell
// tables live inside the executable (no data file carries them), so the tool
// keeps its own copy.  Source: the lof2 project transcriptions in
// port/char_items.inc, port/char_spells.inc and port/char_names.inc.
// ---------------------------------------------------------------------------

// One row of the item table.  `weight` is the raw engine value (item +0x14);
// the game shows it divided by 10 as pounds.
struct ItemInfo {
    const char* name;
    int weight;
};

extern const ItemInfo kItems[];      // 185 rows, index 0 = "Club"
extern const size_t kItemCount;
extern const char* const kSpellNames[];  // 141 rows, index 0 = "Burning hands"
extern const size_t kSpellCount;

// Counts for the per-character name tables.
constexpr size_t kSexCount = 2;
constexpr size_t kRaceCount = 6;
constexpr size_t kProfessionCount = 12;
constexpr size_t kAlignmentCount = 2;
constexpr size_t kSkillCount = 9;
constexpr size_t kLanguageCount = 8;

// The character condition/status table (char offset 0x1f).  Eight entries; the
// engine's own table is 8 fixed 6-byte short forms at image 0x3cca0 with the
// NUL-separated long forms at 0x3ccd0.  See docs/character-status-and-revive.md.
constexpr size_t kStatusCount = 8;
constexpr uint8_t kStatusGood = 0;
constexpr uint8_t kStatusDead = 7;

// Save-game status tables.  The party's location indexes the scenario titles
// (the engine's status box shows the region the location belongs to); the
// facing digit indexes the compass heading.  Transcribed from the lof2 project
// (port/char_names.inc), cross-checked against the shipped saves.
constexpr size_t kScenarioCount = 8;
constexpr size_t kFacingCount = 4;

// 1-based item lookup as stored in an inventory slot (0 = empty slot).  Returns
// nullptr when `index1based` is out of range.
const ItemInfo* item_by_index(int index1based);

// 1-based spell lookup as stored in a character's spell array (0 = empty).
// Returns an empty string when out of range.
const char* spell_name(int index1based);

// Name lookups for the character's enumerated fields.  Out-of-range values
// return a descriptive placeholder (e.g. "none") rather than nullptr.
const char* sex_name(uint8_t index);          // 0 female, 1 male
const char* race_name(uint8_t index);         // 0 Human .. 5 Half-Orc
const char* profession_name(uint8_t index);   // 0 Warrior .. 11 Monk
const char* alignment_name(uint8_t index);    // 0 Lawful, 1 Chaotic
const char* skill_name(size_t index);         // 0 .. 8
const char* language_name(size_t index);      // 0 .. 7
const char* status_short_name(uint8_t index); // 0 Good .. 7 Dead
const char* status_long_name(uint8_t index);  // 0 Good .. 7 Dead

// Scenario title for a party location (the ones digit indexes this table), and
// the compass heading for a facing digit.  Out-of-range values return a
// descriptive placeholder.
const char* scenario_name(uint8_t index);     // 0 .. 7
const char* facing_name(uint8_t index);       // 0 North .. 3 West

}  // namespace loftools

#endif  // LOFTOOLS_CORE_TABLES_H
