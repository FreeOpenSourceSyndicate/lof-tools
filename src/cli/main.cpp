// Headless front-end.  Useful for scripting and for verifying the GUI's results.
//
//   lof-cli check   <file>            report what the cheat protection sees
//   lof-cli fix     <file> [out]      repair in place (or write to `out`)
//   lof-cli dump    <file>            print every character field it can decode
#include <cstdio>
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
    std::printf("state: %s\n", rep.flagged ? "FLAGGED by cheat protection" : "clean");
    if (verbose) {
        for (const auto& f : rep.findings) {
            std::printf("  - %s%s: %s%s\n",
                        f.slot >= 0 ? "slot " : "",
                        f.slot >= 0 ? std::to_string(f.slot).c_str() : "file",
                        f.character.empty() ? "" : (f.character + ": ").c_str(),
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
        std::printf("      %2u) %-18s %u cast(s)\n", (unsigned)s + 1,
                    spell_name(sp.id), sp.casts);
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

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr,
                     "usage: lof-cli check <file>\n"
                     "       lof-tools fix   <file> [out]\n"
                     "       lof-tools dump  <file>\n");
        return 2;
    }
    const std::string cmd = argv[1];
    const std::string path = argv[2];

    if (cmd == "check") {
        SanitizeReport rep = inspect(path);
        print_report(rep, true);
        return rep.flagged ? 1 : 0;
    }
    if (cmd == "fix") {
        const std::string out = argc > 3 ? argv[3] : "";
        SanitizeReport rep = sanitize(path, out);
        print_report(rep, true);
        return rep.file_ok ? 0 : 1;
    }
    if (cmd == "dump") return dump(path);

    std::fprintf(stderr, "unknown command: %s\n", cmd.c_str());
    return 2;
}
