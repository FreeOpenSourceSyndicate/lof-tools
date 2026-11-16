// Per-character name tables for Legend of Faerghail, transcribed from the DOS
// release (they live inside LOF.EXE).  Source: the lof2 project transcription in
// port/char_names.inc.  These index the character struct's enumerated fields.
#include "core/tables.h"

namespace loftools {

namespace {

const char* const kSexNames[kSexCount] = {
    "Female",
    "Male",
};

const char* const kRaceNames[kRaceCount] = {
    "Human",
    "Half-Elf",
    "Elf",
    "Halfling",
    "Dwarf",
    "Half-Orc",
};

const char* const kProfessionNames[kProfessionCount] = {
    "Warrior",
    "Barbarian",
    "Rogue",
    "Smith",
    "Scout",
    "Priest",
    "Druid",
    "Magician",
    "Illusionist",
    "Paladin",
    "Healer",
    "Monk",
};

const char* const kAlignmentNames[kAlignmentCount] = {
    "Lawful",
    "Chaotic",
};

const char* const kSkillNames[kSkillCount] = {
    "Negotiating",
    "Attack",
    "Defence",
    "Concentration",
    "Pick-pocketing",
    "Stalking",
    "Trap detecting",
    "Trap disarming",
    "Lock picking",
};

const char* const kLanguageNames[kLanguageCount] = {
    "Common tongue",
    "Animal tongue",
    "Orc tongue",
    "Lizard tongue",
    "Dwarven tongue",
    "Elven tongue",
    "Dark tongue",
    "Magic tongue",
};

// Status-box tables from LOF.EXE: the scenario titles at image 0x42240 (8 x 22,
// naming the region a party location belongs to) and the compass headings at
// 0x422f0 (4 x 22, "Facing North".."Facing West").  The leading spaces the
// engine uses for centring are trimmed here.
const char* const kScenarioNames[kScenarioCount] = {
    "Valley of Faerghail",
    "Monastery of Sagacita",
    "Sagacita catakombs",
    "The Mines",
    "The Pyramid",
    "The Temple",
    "The Castle",
    "The Mountain",
};

const char* const kFacingNames[kFacingCount] = {
    "North",
    "East",
    "South",
    "West",
};

}  // namespace

const ItemInfo* item_by_index(int index1based) {
    if (index1based < 1 || (size_t)index1based > kItemCount) return nullptr;
    return &kItems[index1based - 1];
}

const char* spell_name(int index1based) {
    if (index1based < 1 || (size_t)index1based > kSpellCount) return "";
    return kSpellNames[index1based - 1];
}

const char* sex_name(uint8_t index) {
    return index < kSexCount ? kSexNames[index] : "?";
}

const char* race_name(uint8_t index) {
    return index < kRaceCount ? kRaceNames[index] : "?";
}

const char* profession_name(uint8_t index) {
    return index < kProfessionCount ? kProfessionNames[index] : "none";
}

const char* alignment_name(uint8_t index) {
    return index < kAlignmentCount ? kAlignmentNames[index] : "?";
}

const char* skill_name(size_t index) {
    return index < kSkillCount ? kSkillNames[index] : "?";
}

const char* language_name(size_t index) {
    return index < kLanguageCount ? kLanguageNames[index] : "?";
}

const char* scenario_name(uint8_t index) {
    return index < kScenarioCount ? kScenarioNames[index] : "?";
}

const char* facing_name(uint8_t index) {
    return index < kFacingCount ? kFacingNames[index] : "?";
}

}  // namespace loftools
