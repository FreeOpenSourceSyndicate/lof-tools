#ifndef LOFTOOLS_CORE_CHARACTER_REVIVE_H
#define LOFTOOLS_CORE_CHARACTER_REVIVE_H

#include <cstdint>
#include <string>
#include <vector>

#include "core/character.h"

namespace loftools {

// The values the game uses to mark a character Dead, and to mark a corpse it
// will no longer raise.  Both are ordinary game states written by the engine
// itself, not corruption: a normal death writes one set of values and a failed
// temple revive writes another.  See docs/character-status-and-revive.md.
//
//   field      offset   death writes   failed revive writes
//   status     +0x1f    7 (Dead)       7 (unchanged)
//   hp_max     +0x20    unchanged      0
//   hp_cur     +0x22    0              0
//
// So `status == 7` is what marks the character dead, and `hp_max == 0` is what
// marks it unrevivable: the revive routine refuses a corpse whose max HP is 0,
// and nothing in the game ever writes max HP back afterwards.
enum class ReviveState {
    Alive,        // status is not Dead
    Dead,         // status == 7 and hp_max > 0: dead, the temple can still raise it
    Unrevivable,  // status == 7 and hp_max == 0: the temple will always refuse
};

const char* revive_state_name(ReviveState s);

// One field a repair would change.
struct FieldChange {
    size_t offset = 0;       // byte offset inside the 410-byte struct
    std::string field;       // human-readable field name, e.g. "max HP"
    uint32_t old_value = 0;  // value in the file
    uint32_t new_value = 0;  // value a repair would write
};

// The revive state of one character plus what a repair would change.
struct ReviveFinding {
    ReviveState state = ReviveState::Alive;
    std::string detail;                // human-readable explanation
    std::vector<FieldChange> changes;  // what a repair would change
};

// Classify one character.  Empty slots are reported as Alive with no detail.
ReviveState revive_state(const Character& c);
ReviveFinding diagnose_revive(const Character& c, uint16_t hp_max = 0);

// Turn an Unrevivable corpse back into a Dead one the temple can raise.
//
// Only max HP is restored, because max HP is the only field the failed revive
// destroyed -- status is still the ordinary Dead value and current HP is still
// 0, exactly as a normal death leaves them.  The character therefore ends up in
// the same state as any other corpse: still dead, and now revivable again.
//
// `hp_max` is the maximum HP to restore.  It is not recoverable from the file:
// the failed revive overwrote it and the 410-byte struct keeps no copy, so the
// value has to come from the player (or a backup).  Pass 0 to use the engine's
// own floor of 1 -- the revive path clamps max HP to >= 1 itself (asm 0xAA06).
//
// Returns the finding that was applied, or a default (Alive) finding when there
// was nothing to do.  Callers write the file afterwards; the document save path
// refreshes the checksums, so the change never trips the cheat protection.
ReviveFinding repair_revive(Character& c, uint16_t hp_max = 0);

}  // namespace loftools

#endif  // LOFTOOLS_CORE_CHARACTER_REVIVE_H
