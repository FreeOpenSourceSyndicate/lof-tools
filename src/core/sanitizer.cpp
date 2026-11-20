#include "core/sanitizer.h"

#include <cstdio>
#include <cstring>

#include "core/character_revive.h"
#include "core/checksum.h"
#include "core/file_io.h"

namespace loftools {

namespace {

std::string hex32(uint32_t v) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "0x%08X", v);
    return buf;
}

std::string hex8(uint8_t v) {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "0x%02X", v);
    return buf;
}

// Repair a single ROST record in place: clear the tamper flag, then refresh the
// trailer so it matches the (possibly edited) 410 struct bytes.
void repair_record(RostRecord& rec, Finding& f) {
    bool changed = false;
    if (rec.character.tampered()) {
        rec.character.set_byte(char_off::kTamperFlag, 0);
        changed = true;
    }
    if (rec.actual_checksum != rec.stored_checksum) {
        rec.stored_checksum = rec.actual_checksum;
        changed = true;
    }
    rec.recompute();
    rec.status = RecordStatus::Ok;
    f.fixed = changed;
}

// Report a corpse the temple will no longer raise.  This is a valid game state,
// not tampering, so it never sets `flagged`; it is listed so the player can see
// which marker value the game is acting on and repair it if they choose.
void report_revive(SanitizeReport& rep, size_t slot, const Character& c,
                   uint16_t hp_max) {
    if (!c.present()) return;
    const ReviveFinding d = diagnose_revive(c, hp_max);
    if (d.state == ReviveState::Alive) return;

    Finding f;
    f.slot = (int)slot;
    f.character = c.name();
    f.repair_hp_max = hp_max;

    if (d.state == ReviveState::Dead) {
        f.informational = true;
        f.detail = "dead: status " + std::to_string(c.status()) + " (" +
                   status_short_name(c.status()) + "), max HP " +
                   std::to_string(c.hp_max()) + ", current HP " +
                   std::to_string(c.hp_cur()) +
                   " - the temple can still revive this character.";
        rep.findings.push_back(std::move(f));
        return;
    }

    f.revive_blocked = true;
    f.detail = std::string(revive_state_name(d.state)) + ": " + d.detail;
    for (const FieldChange& ch : d.changes) {
        f.detail += " [" + ch.field + " " + std::to_string(ch.old_value) +
                    " -> " + std::to_string(ch.new_value) + "]";
    }
    rep.has_unrevivable = true;
    rep.findings.push_back(std::move(f));
}

void report_rost(SanitizeReport& rep, const RostFile& rost) {
    rep.flagged = !rost.all_intact();
    for (size_t i = 0; i < rost.records.size(); ++i) {
        const RostRecord& r = rost.records[i];
        if (r.status == RecordStatus::Ok) continue;
        Finding f;
        f.slot = (int)i;
        f.character = r.name();
        if (r.status == RecordStatus::BadChecksum) {
            f.detail = "checksum mismatch (stored " + hex32(r.stored_checksum) +
                       ", actual " + hex32(r.actual_checksum) + ")";
        } else {
            f.detail = "tamper flag set (byte 0x0C = " +
                       hex8(r.character.tamper_flag()) + ")";
        }
        rep.findings.push_back(std::move(f));
    }
}

}  // namespace

FileKind detect_kind(const std::vector<uint8_t>& file) {
    if (file.size() >= kSaveHeaderSize &&
        std::memcmp(file.data(), kSaveMagic, 4) == 0) {
        return FileKind::SaveGame;
    }
    // ROST: one count byte, then that many 414-byte records.
    if (!file.empty()) {
        const size_t n = file[0];
        if (1 + n * kRostRecordSize == file.size()) return FileKind::Roster;
    }
    return FileKind::Unknown;
}

SanitizeReport inspect(const std::string& path) {
    SanitizeReport rep;
    rep.path = path;

    std::vector<uint8_t> file;
    if (!read_file(path, file)) {
        rep.error = "cannot open file";
        rep.summary = "Cannot open file.";
        return rep;
    }

    rep.kind = detect_kind(file);
    switch (rep.kind) {
        case FileKind::Roster: {
            RostFile rost = parse_rost(file);
            if (!rost.ok) {
                rep.error = rost.error;
                rep.summary = "Not a valid ROST file: " + rost.error;
                return rep;
            }
            rep.file_ok = true;
            report_rost(rep, rost);
            for (size_t i = 0; i < rost.records.size(); ++i)
                report_revive(rep, i, rost.records[i].character, 0);
            break;
        }
        case FileKind::SaveGame: {
            SaveGame sv = parse_save(file);
            if (!sv.ok) {
                rep.error = sv.error;
                rep.summary = "Not a valid save game: " + sv.error;
                return rep;
            }
            rep.file_ok = true;
            rep.flagged = !sv.intact();
            if (!sv.checksum_ok) {
                Finding f;
                f.slot = -1;
                f.character = "";
                f.detail = "save-game checksum mismatch (stored " +
                           hex32(sv.stored_checksum) + ", actual " +
                           hex32(sv.actual_checksum) + ")";
                rep.findings.push_back(std::move(f));
            }
            for (size_t i = 0; i < sv.party.size(); ++i) {
                const Character& c = sv.party[i];
                if (!c.present()) continue;
                if (c.tampered()) {
                    Finding f;
                    f.slot = (int)i;
                    f.character = c.name();
                    f.detail = "tamper flag set (byte 0x0C = " +
                               hex8(c.tamper_flag()) + ")";
                    rep.findings.push_back(std::move(f));
                }
                report_revive(rep, i, c, 0);
            }
            break;
        }
        case FileKind::Unknown:
            rep.error = "unrecognised file format";
            rep.summary = "Unrecognised file: not a ROST or a save game.";
            return rep;
    }

    const bool cheat = rep.flagged;
    const bool dmg = rep.has_unrevivable;
    if (cheat && dmg) {
        rep.summary = "The cheat protection WOULD flag this file and it has " +
                      std::to_string(rep.problem_count()) +
                      " problem(s) in total.";
    } else if (cheat) {
        rep.summary = "Cheat protection WOULD flag this file (" +
                      std::to_string(rep.problem_count()) + " problem(s)).";
    } else if (dmg) {
        rep.summary = "File passes the cheat protection. It has " +
                      std::to_string(rep.problem_count()) +
                      " dead character(s) the temple will no longer revive "
                      "(max HP 0).";
    } else if (!rep.findings.empty()) {
        rep.summary = "File passes the cheat protection; " +
                      std::to_string(rep.findings.size()) +
                      " dead character(s), all still revivable.";
    } else {
        rep.summary = "File is clean; the cheat protection would not flag it.";
    }
    return rep;
}

SanitizeReport sanitize(const std::string& path, const std::string& out_path,
                        uint16_t repair_hp_max,
                        const std::vector<uint16_t>* repair_hp_max_for) {
    SanitizeReport rep;
    rep.path = path;

    std::vector<uint8_t> file;
    if (!read_file(path, file)) {
        rep.error = "cannot open file";
        rep.summary = "Cannot open file.";
        return rep;
    }

    // Per-character max-HP value to restore: the explicit per-slot table wins,
    // then the single value, then the engine floor of 1.
    const auto hp_max_for = [&](size_t slot) -> uint16_t {
        if (repair_hp_max_for && slot < repair_hp_max_for->size())
            return (*repair_hp_max_for)[slot];
        return repair_hp_max;
    };

    rep.kind = detect_kind(file);
    std::vector<uint8_t> repaired;

    switch (rep.kind) {
        case FileKind::Roster: {
            RostFile rost = parse_rost(file);
            if (!rost.ok) {
                rep.error = rost.error;
                rep.summary = "Not a valid ROST file: " + rost.error;
                return rep;
            }
            rep.file_ok = true;
            report_rost(rep, rost);
            for (size_t i = 0; i < rost.records.size(); ++i)
                report_revive(rep, i, rost.records[i].character, hp_max_for(i));

            // Cheat repairs first: clear the tamper flag and refresh the
            // trailer on every record the protection would reject.
            for (size_t i = 0; i < rost.records.size(); ++i) {
                if (rost.records[i].status == RecordStatus::Ok) continue;
                for (auto& f : rep.findings) {
                    if (f.slot == (int)i && !f.revive_blocked)
                        repair_record(rost.records[i], f);
                }
            }
            // Then the character repairs, which change the struct bytes; the
            // recompute in serialize_rost keeps the trailers consistent.
            for (size_t i = 0; i < rost.records.size(); ++i) {
                if (!rost.records[i].character.present()) continue;
                if (repair_revive(rost.records[i].character, hp_max_for(i)).changes.empty())
                    continue;
                for (auto& f : rep.findings)
                    if (f.slot == (int)i && f.revive_blocked) f.fixed = true;
            }
            repaired = serialize_rost(rost);
            break;
        }
        case FileKind::SaveGame: {
            SaveGame sv = parse_save(file);
            if (!sv.ok) {
                rep.error = sv.error;
                rep.summary = "Not a valid save game: " + sv.error;
                return rep;
            }
            rep.file_ok = true;
            rep.flagged = !sv.intact();
            for (size_t i = 0; i < sv.party.size(); ++i) {
                const Character& c = sv.party[i];
                if (!c.present()) continue;
                if (c.tampered()) {
                    Finding f;
                    f.slot = (int)i;
                    f.character = c.name();
                    f.detail = "tamper flag set (byte 0x0C = " +
                               hex8(c.tamper_flag()) + ")";
                    rep.findings.push_back(std::move(f));
                }
                report_revive(rep, i, c, hp_max_for(i));
            }
            if (!sv.checksum_ok) {
                Finding f;
                f.slot = -1;
                f.character = "";
                f.detail = "save-game checksum mismatch (stored " +
                           hex32(sv.stored_checksum) + ", actual " +
                           hex32(sv.actual_checksum) + ")";
                rep.findings.push_back(std::move(f));
            }

            bool any = false;
            for (size_t i = 0; i < sv.party.size(); ++i) {
                Character& c = sv.party[i];
                if (!c.present()) continue;
                if (c.tampered()) {
                    c.set_byte(char_off::kTamperFlag, 0);
                    any = true;
                    for (auto& f : rep.findings)
                        if (f.slot == (int)i && !f.revive_blocked) f.fixed = true;
                }
                if (!repair_revive(c, hp_max_for(i)).changes.empty()) {
                    any = true;
                    for (auto& f : rep.findings)
                        if (f.slot == (int)i && f.revive_blocked) f.fixed = true;
                }
            }
            if (!sv.checksum_ok) {
                any = true;
                for (auto& f : rep.findings)
                    if (f.slot < 0) f.fixed = true;
            }
            repaired = any ? serialize_save(sv) : file;
            break;
        }
        case FileKind::Unknown:
            rep.error = "unrecognised file format";
            rep.summary = "Unrecognised file: not a ROST or a save game.";
            return rep;
    }

    // Only write when we actually changed something.
    const bool changed = repaired != file;
    if (changed) {
        const std::string target = out_path.empty() ? path : out_path;
        if (!write_file(target, repaired)) {
            rep.error = "cannot write " + target;
            rep.summary = "Repaired in memory, but could not write " + target + ".";
            return rep;
        }
        rep.fixed = true;
    }

    if (rep.fixed) {
        rep.summary = "Wrote the repair; the file still passes the cheat "
                      "protection and the character can be revived again.";
    } else if (!rep.findings.empty()) {
        rep.summary = "Problems found but nothing needed writing.";
    } else {
        rep.summary = "File was already clean; nothing to fix.";
    }
    return rep;
}

}  // namespace loftools
