#include "core/document.h"

#include "core/checksum.h"
#include "core/file_io.h"

namespace loftools {

CharacterDocument CharacterDocument::load(const std::string& path) {
    CharacterDocument doc;
    doc.path_ = path;

    std::vector<uint8_t> file;
    if (!read_file(path, file)) {
        doc.error_ = "cannot open file";
        return doc;
    }
    if (file.empty()) {
        doc.error_ = "file is empty";
        return doc;
    }

    // A save game starts with the 00 'F' 'O' 'L' magic; a ROST starts with a
    // count byte followed by that many 414-byte records.
    if (file.size() >= kSaveHeaderSize &&
        file[0] == kSaveMagic[0] && file[1] == kSaveMagic[1] &&
        file[2] == kSaveMagic[2] && file[3] == kSaveMagic[3]) {
        SaveGame sv = parse_save(file);
        if (!sv.ok) {
            doc.error_ = sv.error;
            return doc;
        }
        doc.kind_ = DocKind::SaveGame;
        doc.save_ = std::move(sv);
        doc.ok_ = true;
        return doc;
    }

    if ((size_t)file[0] * kRostRecordSize + 1 == file.size()) {
        RostFile rost = parse_rost(file);
        if (!rost.ok) {
            doc.error_ = rost.error;
            return doc;
        }
        doc.kind_ = DocKind::Roster;
        doc.rost_ = std::move(rost);
        doc.ok_ = true;
        return doc;
    }

    doc.error_ = "not a ROST roster or a save game";
    return doc;
}

size_t CharacterDocument::character_count() const {
    switch (kind_) {
        case DocKind::Roster:   return rost_.records.size();
        case DocKind::SaveGame: return save_.party.size();
        default:                return 0;
    }
}

Character* CharacterDocument::character(size_t index) {
    switch (kind_) {
        case DocKind::Roster:
            return index < rost_.records.size() ? &rost_.records[index].character : nullptr;
        case DocKind::SaveGame:
            return index < save_.party.size() ? &save_.party[index] : nullptr;
        default:
            return nullptr;
    }
}

const Character* CharacterDocument::character(size_t index) const {
    return const_cast<CharacterDocument*>(this)->character(index);
}

std::string CharacterDocument::slot_label(size_t index) const {
    if (kind_ == DocKind::SaveGame) return "Slot " + std::to_string(index + 1);
    return std::to_string(index + 1);
}

uint16_t CharacterDocument::save_location() const {
    return kind_ == DocKind::SaveGame ? save_.location() : 0;
}

void CharacterDocument::set_save_location(uint16_t location) {
    if (kind_ == DocKind::SaveGame) save_.set_location(location);
}

uint8_t CharacterDocument::save_facing() const {
    return kind_ == DocKind::SaveGame ? save_.facing : kSaveFacingInvalid;
}

void CharacterDocument::set_save_facing(uint8_t facing) {
    if (kind_ == DocKind::SaveGame) save_.set_facing(facing);
}

std::vector<uint8_t> CharacterDocument::serialize() const {
    switch (kind_) {
        case DocKind::Roster: {
            RostFile copy = rost_;
            for (auto& r : copy.records) {
                r.character.clear_tamper_flag();
                r.recompute();
                r.stored_checksum = r.actual_checksum;
                r.status = RecordStatus::Ok;
            }
            return serialize_rost(copy);
        }
        case DocKind::SaveGame: {
            SaveGame copy = save_;
            for (auto& c : copy.party) c.clear_tamper_flag();
            // serialize_save() copies the party back into raw and refreshes the
            // whole-file checksum.
            return serialize_save(copy);
        }
        default:
            return {};
    }
}

bool CharacterDocument::save(const std::string& out_path) {
    if (!ok_) {
        error_ = "no document loaded";
        return false;
    }
    const std::vector<uint8_t> out = serialize();
    const std::string target = out_path.empty() ? path_ : out_path;
    if (!write_file(target, out)) {
        error_ = "cannot write " + target;
        return false;
    }
    // Reflect the refreshed checksums back into the live document.
    if (kind_ == DocKind::Roster) {
        for (auto& r : rost_.records) {
            r.character.clear_tamper_flag();
            r.recompute();
            r.stored_checksum = r.actual_checksum;
            r.status = RecordStatus::Ok;
        }
    } else if (kind_ == DocKind::SaveGame) {
        for (auto& c : save_.party) c.clear_tamper_flag();
        save_.recompute();
    }
    return true;
}

}  // namespace loftools
