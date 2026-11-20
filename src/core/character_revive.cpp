#include "core/character_revive.h"

namespace loftools {

const char* revive_state_name(ReviveState s) {
    switch (s) {
        case ReviveState::Alive: return "alive";
        case ReviveState::Dead: return "dead";
        case ReviveState::Unrevivable: return "unrevivable";
    }
    return "?";
}

ReviveState revive_state(const Character& c) {
    if (!c.present() || !c.is_dead()) return ReviveState::Alive;
    return c.hp_max() == 0 ? ReviveState::Unrevivable : ReviveState::Dead;
}

ReviveFinding diagnose_revive(const Character& c, uint16_t hp_max) {
    ReviveFinding f;
    f.state = revive_state(c);
    if (f.state != ReviveState::Unrevivable) return f;

    f.detail =
        "status 7 (Dead) with max HP 0. The temple refuses a corpse whose max "
        "HP is 0 (asm 0xA7AA) and nothing else in the game writes max HP back, "
        "so this character can never be revived again.";
    f.changes.push_back({char_off::kHpMax, "max HP", c.hp_max(),
                         (uint32_t)(hp_max ? hp_max : 1)});
    return f;
}

ReviveFinding repair_revive(Character& c, uint16_t hp_max) {
    ReviveFinding f = diagnose_revive(c, hp_max);
    for (const FieldChange& ch : f.changes)
        c.set_word(ch.offset, (uint16_t)ch.new_value);
    return f;
}

}  // namespace loftools
