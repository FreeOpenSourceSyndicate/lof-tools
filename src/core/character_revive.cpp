#include "core/character_revive.h"

namespace loftools {
namespace {

// Max HP is the midpoint of the engine's own ranges, so the estimate lands in
// the middle of what the game could legitimately have rolled.  Checked against
// the maintainer's roster: excluding the unfinished slot and the two cheat
// characters, this is within 1.5 HP on average and 7 HP at worst.
constexpr double kSeedMean = 11.5;      // mean of the creation roll
constexpr double kLevelGainMean = 5.5;  // mean of the 1..10 per-level gain

}  // namespace

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

std::string revive_state_detail(const Character& c) {
    switch (revive_state(c)) {
        case ReviveState::Alive:
            return "";
        case ReviveState::Dead:
            return "dead (status 7); the temple can still revive this character";
        case ReviveState::Unrevivable:
            return "dead (status 7) with max HP 0, so the temple will refuse to "
                   "revive it. A failed revive zeroes max HP and the game never "
                   "restores it.";
    }
    return "";
}

uint16_t estimate_hp_max(const Character& c) {
    // Level is stored as a byte and counts level-ups already taken; the shipped
    // level-1 characters carry 1 and a fresh character carries 0.
    const int level = c.level();
    const int levels_gained = level > 1 ? level - 1 : 0;
    double hp = kSeedMean + kLevelGainMean * levels_gained;
    if (hp < 1) hp = 1;
    if (hp > 32767) hp = 32767;
    return (uint16_t)(hp + 0.5);
}

bool restore_revivability(Character& c) {
    if (revive_state(c) != ReviveState::Unrevivable) return false;
    c.set_hp_max(estimate_hp_max(c));
    return true;
}

}  // namespace loftools
