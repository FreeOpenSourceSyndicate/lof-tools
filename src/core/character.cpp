#include "core/character.h"

namespace loftools {

// Field descriptors drive the editor's form and the "uncertain" marking.  Every
// field here is either byte-for-byte verified against the user's character sheet
// (Firm) or grounded in the engine's own code (Likely).  Anything we could only
// guess at is deliberately *absent* rather than shown as Uncertain, so the
// editor never offers a value we do not trust.
const std::vector<FieldInfo>& character_fields() {
    static const std::vector<FieldInfo> fields = {
        {"name", "Name", "Identity", FieldConfidence::Firm, ""},
        {"level", "Rank (level)", "Identity", FieldConfidence::Firm, ""},
        {"sex", "Sex", "Identity", FieldConfidence::Firm, ""},
        {"race", "Race", "Identity", FieldConfidence::Firm, ""},
        {"profession", "Profession", "Identity", FieldConfidence::Firm, ""},
        {"alignment", "Alignment", "Identity", FieldConfidence::Firm, ""},

        {"str", "Strength", "Attributes", FieldConfidence::Firm, ""},
        {"con", "Constitution", "Attributes", FieldConfidence::Firm, ""},
        {"dex", "Dexterity", "Attributes", FieldConfidence::Firm, ""},
        {"int", "Intelligence", "Attributes", FieldConfidence::Firm, ""},
        {"wis", "Wisdom", "Attributes", FieldConfidence::Firm, ""},

        {"hp_cur", "Hit points (current)", "Vitals", FieldConfidence::Firm, ""},
        {"hp_max", "Hit points (max)", "Vitals", FieldConfidence::Firm, ""},
        {"mp_cur", "Magic points (current)", "Vitals", FieldConfidence::Firm, ""},
        {"mp_max", "Magic points (max)", "Vitals", FieldConfidence::Firm, ""},

        {"exp", "Experience", "Progress", FieldConfidence::Firm,
         "Signed 32-bit; the game allows negative experience."},
        {"gold", "Gold (purse)", "Money", FieldConfidence::Firm, ""},
        {"deposit", "Gold (deposit)", "Money", FieldConfidence::Firm, ""},
        {"rations", "Rations", "Money", FieldConfidence::Firm, ""},

        {"skill_negotiating", "Negotiating", "Skills", FieldConfidence::Firm, ""},
        {"skill_attack", "Attack", "Skills", FieldConfidence::Firm, ""},
        {"skill_defence", "Defence", "Skills", FieldConfidence::Firm, ""},
        {"skill_concentration", "Concentration", "Skills", FieldConfidence::Firm, ""},
        {"skill_pickpocket", "Pick-pocketing", "Skills", FieldConfidence::Firm, ""},
        {"skill_stalking", "Stalking", "Skills", FieldConfidence::Firm,
         "Shown as \"Sneak\" on the in-game sheet."},
        {"skill_trap_detect", "Trap detecting", "Skills", FieldConfidence::Firm, ""},
        {"skill_trap_disarm", "Trap disarming", "Skills", FieldConfidence::Firm, ""},
        {"skill_lockpicking", "Lock picking", "Skills", FieldConfidence::Firm, ""},
    };
    return fields;
}

}  // namespace loftools
