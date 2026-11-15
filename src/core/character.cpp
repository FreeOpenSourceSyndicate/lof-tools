#include "core/character.h"

namespace loftools {

// Profession indices are 0..11; the engine rejects values > 0x0b (the display
// routine at image 0x2FB4A) and the SNSC seed writes 0x0c ("no profession").
// 0xff marks an untouched template slot.
const char* const kProfessionNames[12] = {
    "Warrior", "Ranger",  "Monk",    "Paladin", "Thief",     "Bard",
    "Magic user", "Cleric", "Druid", "Rogue",   "Healer",    "Alchemist",
};

std::string profession_name(uint8_t index) {
    if (index < 12) return kProfessionNames[index];
    return "none";
}

}  // namespace loftools
