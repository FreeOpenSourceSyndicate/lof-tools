#ifndef LOFTOOLS_CORE_CHARACTER_REVIVE_H
#define LOFTOOLS_CORE_CHARACTER_REVIVE_H

#include <cstdint>
#include <string>

#include "core/character.h"

namespace loftools {

// The values the game uses to mark a character Dead, and to mark a corpse it
// will no longer raise.  Both are ordinary game states written by the engine
// itself, not corruption and not the result of editing a file: a normal death
// writes one set of values, a failed temple revive writes another.
//
//   field      offset   death writes   failed revive writes
//   status     +0x1f    7 (Dead)       7 (unchanged)
//   hp_max     +0x20    unchanged      0
//   hp_cur     +0x22    0              0
//
// So `status == 7` is what marks the character dead, and `hp_max == 0` is what
// marks it unrevivable: the revive routine refuses a corpse whose max HP is 0
// (asm 0xA7AA), and nothing in the game ever writes max HP back afterwards.
//
// Because this is a legitimate game state and not tampering, it belongs in the
// character editor, not in the cheat-protection sanitizer.
enum class ReviveState {
    Alive,        // status is not Dead
    Dead,         // status == 7 and hp_max > 0: dead, the temple can still raise it
    Unrevivable,  // status == 7 and hp_max == 0: the temple will always refuse
};

const char* revive_state_name(ReviveState s);

// A short human-readable description of the character's condition.
std::string revive_state_detail(const Character& c);

ReviveState revive_state(const Character& c);

// Estimate a plausible maximum HP for a character whose real value was
// destroyed by a failed revive.
//
// The engine builds max HP from two things only (asm):
//   * at creation  hp_max = rand(1 .. n/2) + n/2, where n is the class record's
//     HP seed (`rec[0x2c]`, see lof2 docs/notes-b.md);
//   * on every level-up it adds rand(1..10) to max and current HP
//     (`0x9164`..`0x91A6`, `add word ptr es:[bx+si+0x20], ax`).
//
// So max HP is a random walk from a random start, and the exact value cannot be
// recovered from the file -- the failed revive overwrote it and the 410-byte
// struct keeps no copy.  The level is the only useful input: the shipped
// characters roll 5..18 max HP at level 1 (mean 11.5), and each level after that
// adds 1..10 (mean 5.5).  This returns the mean of that distribution, which is
// the most defensible "plausible" value; it is an estimate, not the original.
//
// The result is always >= 1: the engine never creates a character with less
// (creation clamps at `0x1DA84`).
uint16_t estimate_hp_max(const Character& c);

// Restore revivability: give an unrevivable corpse a plausible max HP so the
// temple will accept it again.
//
// Only max HP is written.  Status stays 7 and current HP stays 0, so the
// character remains properly dead -- this does not resurrect anyone, it just
// undoes the damage the failed revive did.  Returns true when it changed
// anything.
bool restore_revivability(Character& c);

}  // namespace loftools

#endif  // LOFTOOLS_CORE_CHARACTER_REVIVE_H
