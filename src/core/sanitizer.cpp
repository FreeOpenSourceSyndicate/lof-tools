#include "core/sanitizer.h"

#include <cstdio>
#include <cstring>

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
                if (!c.present() || !c.tampered()) continue;
                Finding f;
                f.slot = (int)i;
                f.character = c.name();
                f.detail = "tamper flag set (byte 0x0C = " +
                           hex8(c.tamper_flag()) + ")";
                rep.findings.push_back(std::move(f));
            }
            break;
        }
        case FileKind::Unknown:
            rep.error = "unrecognised file format";
            rep.summary = "Unrecognised file: not a ROST or a save game.";
            return rep;
    }

    if (rep.flagged) {
        rep.summary = "Cheat protection WOULD flag this file (" +
                      std::to_string(rep.findings.size()) + " problem(s)).";
    } else {
        rep.summary = "File is clean; the cheat protection would not flag it.";
    }
    return rep;
}

SanitizeReport sanitize(const std::string& path, const std::string& out_path) {
    SanitizeReport rep;
    rep.path = path;

    std::vector<uint8_t> file;
    if (!read_file(path, file)) {
        rep.error = "cannot open file";
        rep.summary = "Cannot open file.";
        return rep;
    }

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
            rep.flagged = !rep.findings.empty();
            for (size_t i = 0; i < rost.records.size(); ++i) {
                if (rost.records[i].status == RecordStatus::Ok) continue;
                for (auto& f : rep.findings) {
                    if (f.slot == (int)i) repair_record(rost.records[i], f);
                }
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
            bool any = false;
            for (size_t i = 0; i < sv.party.size(); ++i) {
                Character& c = sv.party[i];
                if (!c.present() || !c.tampered()) continue;
                any = true;
                Finding f;
                f.slot = (int)i;
                f.character = c.name();
                f.detail = "tamper flag set (byte 0x0C = " + hex8(c.tamper_flag()) + ")";
                c.set_byte(char_off::kTamperFlag, 0);
                f.fixed = true;
                rep.findings.push_back(std::move(f));
            }
            if (!sv.checksum_ok) {
                any = true;
                Finding f;
                f.slot = -1;
                f.character = "";
                f.detail = "save-game checksum mismatch (stored " +
                           hex32(sv.stored_checksum) + ", actual " +
                           hex32(sv.actual_checksum) + ")";
                f.fixed = true;
                rep.findings.push_back(std::move(f));
            }
            if (any) {
                repaired = serialize_save(sv);
            } else {
                repaired = file;
            }
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

    if (rep.flagged) {
        rep.summary = rep.fixed
            ? "Repaired " + std::to_string(rep.findings.size()) +
                  " problem(s); file now passes the cheat protection."
            : "Problems found but nothing needed writing.";
    } else {
        rep.summary = "File was already clean; nothing to fix.";
    }
    return rep;
}

}  // namespace loftools
