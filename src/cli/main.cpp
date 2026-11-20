// Headless front-end.  Useful for scripting and for verifying the GUI's results.
//
//   lof-cli check   <file>            report what the cheat protection sees
//   lof-cli fix     <file> [out]      repair in place (or write to `out`)
//   lof-cli dump    <file>            print every character field it can decode
//   lof-cli state   <file>            print save-game state (party, position)
//
// `fix` accepts `--hp-max <n>` to set the max HP restored for an unrevivable
// character (see docs/character-status-and-revive.md); without it the engine's
// floor of 1 is used.
#include <cstdio>
#include <cstdlib>
#include <string>

#include "core/document.h"
#include "core/sanitizer.h"
#include "core/tables.h"

using namespace loftools;

static const char* kind_name(FileKind k) {
    switch (k) {
        case FileKind::Roster:   return "ROST roster";
        case FileKind::SaveGame: return "save game";
        default:                 return "unknown";
    }
}

static void print_report(const SanitizeReport& rep, bool verbose) {
    std::printf("file : %s\n", rep.path.c_str());
    std::printf("kind : %s\n", kind_name(rep.kind));
    if (!rep.file_ok) {
        std::printf("error: %s\n", rep.error.c_str());
        return;
    }
    if (rep.flagged)
        std::printf("state: FLAGGED by cheat protection\n");
    else if (rep.has_unrevivable)
        std::printf("state: passes cheat protection; a dead character can no "
                    "longer be revived\n");
    else
        std::printf("state: clean\n");
    if (verbose) {
        for (const auto& f : rep.findings) {
            std::printf("  - %s%s: %s%s%s\n",
                        f.slot >= 0 ? "slot " : "",
                        f.slot >= 0 ? std::to_string(f.slot).c_str() : "file",
                        f.character.empty() ? "" : (f.character + ": ").c_str(),
                        f.informational ? "[info] "
                        : f.revive_blocked ? "[dead] "
                                           : "[cheat] ",
                        f.detail.c_str());
        }
    }
    std::printf("%s\n", rep.summary.c_str());
}

static void dump_character(const Character& c) {
    std::printf("  %-12s  L%-3u %s %s %s %s\n", c.name().c_str(), c.level(),
                sex_name(c.sex()), race_name(c.race()),
                alignment_name(c.alignment()), profession_name(c.profession()));
    std::printf("    attributes : STR %u  CON %u  DEX %u  INT %u  WIS %u\n",
                c.str(), c.con(), c.dex(), c.intel(), c.wis());
    std::printf("    vitals     : HP %u/%u  MP %u/%u  defence %u\n", c.hp_cur(),
                c.hp_max(), c.mp_cur(), c.mp_max(), c.defence());
    std::printf("    money      : gold %u  deposit %u  rations %u  exp %d\n",
                c.gold(), c.deposit(), c.rations(), c.experience());
    std::printf("    weight     : %u/%u (shown /10)\n", c.weight_cur(),
                c.weight_max());
    std::printf("    skills     :");
    for (size_t i = 0; i < kSkillCount; ++i)
        std::printf(" %s %u;", skill_name(i), c.skill(i));
    std::printf("\n");
    std::printf("    languages  :");
    for (size_t i = 0; i < kLanguageCount; ++i)
        if (c.knows_language(i)) std::printf(" %s;", language_name(i));
    std::printf("\n");
    std::printf("    inventory  : %u slot(s) used\n", c.inv_used());
    for (size_t s = 0; s < char_off::kInventorySlots; ++s) {
        InvSlot slot = c.inventory(s);
        if (!slot.item) break;
        const ItemInfo* it = item_by_index(slot.item);
        std::printf("      %2u) %-18s %3u%%%s\n", (unsigned)s + 1,
                    it ? it->name : "?", slot.condition,
                    slot.equipped ? " equipped" : "");
    }
    std::printf("    spells     : %u learned\n", c.spell_count());
    for (size_t s = 0; s < c.spell_count() && s < char_off::kSpellSlots; ++s) {
        SpellSlot sp = c.spell(s);
        std::printf("      %2u) %-18s %u cast(s)%s\n", (unsigned)s + 1,
                    spell_name(sp.id), sp.casts,
                    sp.id == 0 ? "  (spent)" : "");
    }
}

static int dump(const std::string& path) {
    CharacterDocument doc = CharacterDocument::load(path);
    if (!doc.ok()) {
        std::fprintf(stderr, "cannot read %s: %s\n", path.c_str(),
                     doc.error().c_str());
        return 1;
    }
    std::printf("file : %s\n", path.c_str());
    std::printf("kind : %s\n",
                doc.kind() == DocKind::SaveGame ? "save game" : "ROST roster");
    std::printf("slots: %zu\n\n", doc.character_count());
    for (size_t i = 0; i < doc.character_count(); ++i) {
        const Character* c = doc.character(i);
        std::printf("[%s]\n", doc.slot_label(i).c_str());
        if (!c) {
            std::printf("  (no character)\n\n");
            continue;
        }
        dump_character(*c);
        std::printf("\n");
    }
    return 0;
}

static int dump_state(const std::string& path) {
    CharacterDocument doc = CharacterDocument::load(path);
    if (!doc.ok()) {
        std::fprintf(stderr, "cannot read %s: %s\n", path.c_str(),
                     doc.error().c_str());
        return 1;
    }
    if (!doc.is_save_game()) {
        std::fprintf(stderr, "%s is a ROST roster, not a save game\n",
                     path.c_str());
        return 1;
    }
    const uint16_t loc = doc.save_location();
    std::printf("file     : %s\n", path.c_str());
    std::printf("location : %u -> %s (region %s)\n", loc,
                dungeon_map_name(loc).c_str(), scenario_name(loc % 10));
    std::printf("facing   : %u (%s)\n", doc.save_facing(),
                facing_name(doc.save_facing()));
    std::printf("party    :\n");
    for (size_t i = 0; i < doc.character_count(); ++i) {
        const Character* c = doc.character(i);
        if (!c || !c->present()) {
            std::printf("  slot %zu : (empty)\n", i + 1);
            continue;
        }
        std::printf("  slot %zu : %-12s %s %s, rank %u, HP %u/%u\n", i + 1,
                    c->name().c_str(), race_name(c->race()),
                    profession_name(c->profession()), c->level(), c->hp_cur(),
                    c->hp_max());
    }
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr,
                     "usage: lof-cli check <file>\n"
                     "       lof-cli fix   <file> [out] [--hp-max <n>]\n"
                     "       lof-cli dump  <file>\n"
                     "       lof-cli state <file>\n");
        return 2;
    }
    const std::string cmd = argv[1];
    const std::string path = argv[2];

    // `fix` takes an optional output path and an optional --hp-max value; pull
    // the option out of the remaining arguments wherever it appears.
    std::string out;
    uint16_t hp_max = 0;
    for (int i = 3; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--hp-max" && i + 1 < argc) {
            hp_max = (uint16_t)std::strtoul(argv[++i], nullptr, 10);
        } else if (out.empty()) {
            out = a;
        }
    }

    if (cmd == "check") {
        SanitizeReport rep = inspect(path);
        print_report(rep, true);
        return rep.flagged ? 1 : 0;
    }
    if (cmd == "fix") {
        SanitizeReport rep = sanitize(path, out, hp_max);
        print_report(rep, true);
        return rep.file_ok ? 0 : 1;
    }
    if (cmd == "dump") return dump(path);
    if (cmd == "state") return dump_state(path);

    std::fprintf(stderr, "unknown command: %s\n", cmd.c_str());
    return 2;
}
