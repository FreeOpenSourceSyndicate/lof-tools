#ifndef LOFTOOLS_CORE_SANITIZER_H
#define LOFTOOLS_CORE_SANITIZER_H

#include <cstdint>
#include <string>
#include <vector>

#include "core/rost.h"
#include "core/savegame.h"

namespace loftools {

// Which on-disk format a file turned out to be.
enum class FileKind { Unknown, Roster, SaveGame };

// A single problem found (or fixed) in a file.
struct Finding {
    // Which character slot the finding concerns, or -1 for a whole-file issue.
    int slot = -1;
    std::string character;   // character name, empty for whole-file issues
    std::string detail;      // what is wrong
    bool fixed = false;      // true once the repair has been applied

    // Set when this finding is a valid game state the player cannot recover
    // from in-game (a corpse the temple will no longer raise, see
    // core/character_revive.h) rather than a cheat-protection problem.  Such a
    // finding never makes the file "flagged".
    bool revive_blocked = false;
    // Informational only: a dead character that is still revivable.  Listed so
    // the player can see which values mark a character dead, but it is not a
    // problem and does not enable the fix button.
    bool informational = false;
    // For a revive finding: the max HP a repair should restore.  The value
    // cannot be recovered from the file, so it is supplied by the caller of
    // sanitize(); 0 means "use the engine's floor of 1".
    uint16_t repair_hp_max = 0;
};

struct SanitizeReport {
    bool file_ok = false;        // the file parsed at all
    std::string error;           // parse error, when !file_ok
    FileKind kind = FileKind::Unknown;
    std::string path;

    // Whole-file state.
    bool flagged = false;        // the engine would refuse/flag this file
    bool fixed = false;          // a repair was applied and saved
    // True when at least one dead character can no longer be revived in game.
    bool has_unrevivable = false;

    std::vector<Finding> findings;
    std::string summary;         // one-line result for the UI

    // Findings that are actual problems (excludes informational entries).
    size_t problem_count() const {
        size_t n = 0;
        for (const Finding& f : findings)
            if (!f.informational) ++n;
        return n;
    }
};

// Detect the format of an in-memory image by magic / size heuristics.
FileKind detect_kind(const std::vector<uint8_t>& file);

// Inspect a file and report what the cheat protection would flag.  Does not
// modify anything on disk.
SanitizeReport inspect(const std::string& path);

// Inspect and, if needed, repair.  Writes the repaired file back to `path`
// (or to `out_path` when non-empty).  Returns the report; report.fixed is set
// when bytes were written.
//
// `repair_hp_max` supplies the maximum HP to restore for a corpse the temple
// will no longer raise (see core/character_revive.h).  It cannot be recovered
// from the file; 0 uses the engine's floor of 1.  Pass a slot-specific override
// through `repair_hp_max_for` to set a different value per character index.
SanitizeReport sanitize(const std::string& path, const std::string& out_path = "",
                        uint16_t repair_hp_max = 0,
                        const std::vector<uint16_t>* repair_hp_max_for = nullptr);

}  // namespace loftools

#endif  // LOFTOOLS_CORE_SANITIZER_H
