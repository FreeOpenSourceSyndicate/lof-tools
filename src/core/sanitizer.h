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
};

struct SanitizeReport {
    bool file_ok = false;        // the file parsed at all
    std::string error;           // parse error, when !file_ok
    FileKind kind = FileKind::Unknown;
    std::string path;

    // Whole-file state.
    bool flagged = false;        // the engine would refuse/flag this file
    bool fixed = false;          // a repair was applied and saved

    std::vector<Finding> findings;
    std::string summary;         // one-line result for the UI
};

// Detect the format of an in-memory image by magic / size heuristics.
FileKind detect_kind(const std::vector<uint8_t>& file);

// Inspect a file and report what the cheat protection would flag.  Does not
// modify anything on disk.
SanitizeReport inspect(const std::string& path);

// Inspect and, if needed, repair.  Writes the repaired file back to `path`
// (or to `out_path` when non-empty).  Returns the report; report.fixed is set
// when bytes were written.
SanitizeReport sanitize(const std::string& path, const std::string& out_path = "");

}  // namespace loftools

#endif  // LOFTOOLS_CORE_SANITIZER_H
