#ifndef LOFTOOLS_CORE_DOCUMENT_H
#define LOFTOOLS_CORE_DOCUMENT_H

#include <cstdint>
#include <string>
#include <vector>

#include "core/character.h"
#include "core/rost.h"
#include "core/savegame.h"

namespace loftools {

// What a loaded file turned out to be.
enum class DocKind { None, Roster, SaveGame };

// A loaded, editable document: either a ROST roster or a save game.  It owns the
// parsed representation and hands out mutable Character pointers, so the future
// character and save-game editors can share one model.  Saving recomputes the
// integrity checksums, so an edit never trips the game's cheat protection.
//
// For a ROST, every record is exposed (including an un-rolled template slot such
// as Siegurd); for a save game, the six party slots are exposed.
class CharacterDocument {
public:
    CharacterDocument() = default;

    // Load and parse `path`.  Never throws.  Check ok() and error().
    static CharacterDocument load(const std::string& path);

    bool ok() const { return ok_; }
    const std::string& error() const { return error_; }
    DocKind kind() const { return kind_; }
    const std::string& path() const { return path_; }

    // Number of character slots available in this document.
    size_t character_count() const;

    // Character at `index` (0-based); nullptr when out of range.
    Character* character(size_t index);
    const Character* character(size_t index) const;

    // A short label for a slot (e.g. "1" for a roster, "Slot 1" for a party).
    std::string slot_label(size_t index) const;

    // Recompute every integrity checksum from the current character bytes and
    // write the document back to `path` (or to out_path when non-empty).
    // Returns false and sets error() on failure.
    bool save(const std::string& out_path = "");

    // Serialize to a byte image without writing (checksums refreshed).
    std::vector<uint8_t> serialize() const;

private:
    bool ok_ = false;
    std::string error_;
    DocKind kind_ = DocKind::None;
    std::string path_;

    // Exactly one of these is populated, according to kind_.
    RostFile rost_;
    SaveGame save_;
};

}  // namespace loftools

#endif  // LOFTOOLS_CORE_DOCUMENT_H
