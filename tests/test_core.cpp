// Unit tests for the reusable core.  Run from the repository root (or pass the
// testdata directory as argv[1]).
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "core/checksum.h"
#include "core/character.h"
#include "core/character_revive.h"
#include "core/document.h"
#include "core/file_io.h"
#include "core/rost.h"
#include "core/sanitizer.h"
#include "core/savegame.h"
#include "core/tables.h"

using namespace loftools;

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond)                                                      \
    do {                                                                 \
        ++g_checks;                                                      \
        if (!(cond)) {                                                   \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);  \
            ++g_failures;                                                \
        }                                                                \
    } while (0)

static std::string data_dir(int argc, char** argv) {
    if (argc > 1) return argv[1];
    return "testdata";
}

// Known-good trailers from ref/rost/ROST1.cheat (six records).
static void test_rost_fixture(const std::string& dir) {
    std::vector<uint8_t> file;
    CHECK(read_file(dir + "/ROST1.cheat", file));
    if (file.empty()) return;

    RostFile rost = parse_rost(file);
    CHECK(rost.ok);
    CHECK(rost.records.size() == 6);
    CHECK(rost.all_intact());
    CHECK(rost.damaged_count() == 0);

    // Trailers the engine stores for this file.
    const uint32_t expect[6] = {0x0539, 0x0578, 0x053c, 0x07d7, 0x069d, 0x07cb};
    const char* names[6] = {"Siegurd", "Conan", "Tuck", "Lara", "Laladil", "Mirakulix"};
    for (size_t i = 0; i < rost.records.size(); ++i) {
        CHECK(rost.records[i].stored_checksum == expect[i]);
        CHECK(rost.records[i].actual_checksum == expect[i]);
        CHECK(rost.records[i].name() == names[i]);
        CHECK(rost.records[i].character.tamper_flag() == 0);
    }

    // Round-trip must be byte-identical for an intact file.
    std::vector<uint8_t> again = serialize_rost(rost);
    CHECK(again == file);
}

static void test_rost_repair(const std::string& dir) {
    std::vector<uint8_t> file;
    CHECK(read_file(dir + "/ROST1.cheat", file));
    if (file.empty()) return;

    // Corrupt record 1: flip a stat byte without updating the trailer.
    std::vector<uint8_t> bad = file;
    bad[1 + 1 * kRostRecordSize + 0x45] ^= 0x01;  // Strength

    RostFile rost = parse_rost(bad);
    CHECK(rost.ok);
    CHECK(!rost.all_intact());
    CHECK(rost.damaged_count() == 1);
    CHECK(rost.records[1].status == RecordStatus::BadChecksum);

    // Repair: the trailer must be recomputed to match the new bytes.
    rost.records[1].stored_checksum = rost.records[1].actual_checksum;
    std::vector<uint8_t> fixed = serialize_rost(rost);
    RostFile re = parse_rost(fixed);
    CHECK(re.all_intact());
    CHECK(re.records[1].name() == "Conan");

    // A stale tamper flag alone must also be flagged and clearable.
    std::vector<uint8_t> flagged = file;
    flagged[1 + 2 * kRostRecordSize + char_off::kTamperFlag] = kTamperFlagValue;
    // Recompute the trailer so only the flag is wrong.
    uint32_t c = lof_checksum(flagged.data() + 1 + 2 * kRostRecordSize, kCharSize);
    write_u32le(flagged.data() + 1 + 2 * kRostRecordSize + kCharSize, c);
    RostFile r2 = parse_rost(flagged);
    CHECK(r2.records[2].status == RecordStatus::Tampered);
    CHECK(!r2.all_intact());
}

static void test_save_fixture(const std::string& dir) {
    std::vector<uint8_t> file;
    CHECK(read_file(dir + "/GAME1", file));
    if (file.empty()) return;
    CHECK(file.size() == kSaveSize);

    SaveGame sv = parse_save(file);
    CHECK(sv.ok);
    CHECK(sv.checksum_ok);
    CHECK(sv.intact());
    CHECK(sv.party[0].name() == "Decker");
    CHECK(sv.party[5].name() == "Assanla");
    CHECK(sv.location() == 5);  // loc_ones=5, loc_tens=0

    std::vector<uint8_t> again = serialize_save(sv);
    CHECK(again == file);
}

static void test_save_repair(const std::string& dir) {
    std::vector<uint8_t> file;
    CHECK(read_file(dir + "/GAME4", file));
    if (file.empty()) return;

    // Corrupt a character byte -> the whole-file checksum no longer matches.
    std::vector<uint8_t> bad = file;
    bad[kSaveHeaderSize + 0x45] ^= 0x04;
    SaveGame sv = parse_save(bad);
    CHECK(sv.ok);
    CHECK(!sv.checksum_ok);
    CHECK(!sv.intact());

    // Repair and re-check.
    uint8_t tail[4];
    write_u32le(tail, lof_checksum(sv.raw.data(), kSaveStateSize));
    std::memcpy(sv.raw.data() + kSaveStateSize, tail, 4);
    SaveGame fixed = parse_save(sv.raw);
    CHECK(fixed.checksum_ok);
    CHECK(fixed.intact());
    // The character edit must survive the repair.
    CHECK(fixed.party[0].str() == (uint8_t)(sv.party[0].str()));
}

static void test_detect(const std::string& dir) {
    std::vector<uint8_t> rost, save;
    if (read_file(dir + "/ROST1.cheat", rost)) CHECK(detect_kind(rost) == FileKind::Roster);
    if (read_file(dir + "/GAME1", save)) CHECK(detect_kind(save) == FileKind::SaveGame);
    std::vector<uint8_t> junk{1, 2, 3};
    CHECK(detect_kind(junk) == FileKind::Unknown);
}

// End-to-end: sanitize() a damaged file on disk and confirm the fix persists.
static void test_end_to_end(const std::string& dir) {
    std::vector<uint8_t> file;
    CHECK(read_file(dir + "/ROST2.banked", file));
    if (file.empty()) return;

    const std::string tmp = "/tmp/lof_tools_test_rost.bin";
    std::vector<uint8_t> bad = file;
    bad[1 + 3 * kRostRecordSize + 0x44] ^= 0x08;  // damage record 3
    CHECK(write_file(tmp, bad));

    SanitizeReport rep = sanitize(tmp);
    CHECK(rep.file_ok);
    CHECK(rep.kind == FileKind::Roster);
    CHECK(rep.flagged);
    CHECK(rep.fixed);

    std::vector<uint8_t> after;
    CHECK(read_file(tmp, after));
    RostFile rost = parse_rost(after);
    CHECK(rost.all_intact());

    // Sanitizing again is a no-op.
    SanitizeReport rep2 = sanitize(tmp);
    CHECK(!rep2.flagged);
    std::remove(tmp.c_str());
}

// Every value the user supplied for the uploaded roster, asserted field by
// field.  This is the ground truth for the character editor: if any of these
// fails, a decoded offset is wrong.
static void test_character_fields(const std::string& dir) {
    CharacterDocument doc = CharacterDocument::load(dir + "/ROST.user");
    CHECK(doc.ok());
    CHECK(doc.kind() == DocKind::Roster);
    CHECK(doc.character_count() == 7);

    const char* names[7] = {"Siegurd", "Conan", "Tuck", "Lara", "Laladil",
                            "Mirakulix", "Morgana"};
    for (size_t i = 0; i < 7; ++i) {
        const Character* c = doc.character(i);
        CHECK(c != nullptr);
        CHECK(c->name() == names[i]);
    }

    // Index 0 is the un-rolled template: rank/attributes unset, HP seeded.
    const Character* s = doc.character(0);
    CHECK(s->hp_max() == 88 && s->hp_cur() == 88);
    CHECK(s->rations() == 4 && s->gold() == 4 && s->deposit() == 0);
    CHECK(s->str() == 0);

    // Conan (index 1): chaotic human barbarian.
    const Character* conan = doc.character(1);
    CHECK(conan->level() == 1);
    CHECK(conan->sex() == 1);          // male
    CHECK(conan->alignment() == 1);    // chaotic
    CHECK(conan->race() == 0);         // Human
    CHECK(conan->profession() == 1);   // Barbarian
    CHECK(conan->str() == 19 && conan->con() == 18 && conan->dex() == 17);
    CHECK(conan->intel() == 16 && conan->wis() == 10);
    CHECK(conan->hp_cur() == 16 && conan->hp_max() == 17);
    CHECK(conan->mp_cur() == 0 && conan->mp_max() == 0);
    CHECK(conan->experience() == 129);
    CHECK(conan->weight_max() == 5100 && conan->weight_cur() == 1059);
    CHECK(conan->rations() == 14);
    CHECK(conan->gold() == 3);
    CHECK(conan->deposit() == 8388318);

    // Tuck (index 2): lawful Human monk.
    const Character* tuck = doc.character(2);
    CHECK(tuck->level() == 1);
    CHECK(tuck->sex() == 1);
    CHECK(tuck->alignment() == 0);     // lawful
    CHECK(tuck->race() == 0);          // Human
    CHECK(tuck->profession() == 11);   // Monk
    CHECK(tuck->str() == 19 && tuck->con() == 19 && tuck->dex() == 19);
    CHECK(tuck->hp_cur() == 16 && tuck->hp_max() == 16);
    CHECK(tuck->experience() == 28);
    CHECK(tuck->deposit() == 8388629);

    // Lara (index 3): lawful dwarven priestess, level 6, negative experience.
    const Character* lara = doc.character(3);
    CHECK(lara->level() == 6);
    CHECK(lara->sex() == 0);           // female
    CHECK(lara->alignment() == 0);     // lawful
    CHECK(lara->race() == 4);          // Dwarf
    CHECK(lara->profession() == 5);    // Priest
    CHECK(lara->hp_cur() == 39 && lara->hp_max() == 39);
    CHECK(lara->mp_cur() == 10 && lara->mp_max() == 12);
    CHECK(lara->experience() == -4746);
    CHECK(lara->deposit() == 8382582);

    // Laladil (index 4): chaotic half-elven female paladin.
    const Character* laladil = doc.character(4);
    CHECK(laladil->level() == 1);
    CHECK(laladil->sex() == 0);
    CHECK(laladil->alignment() == 1);
    CHECK(laladil->race() == 1);       // Half-Elf
    CHECK(laladil->profession() == 9); // Paladin
    CHECK(laladil->str() == 17 && laladil->con() == 17 && laladil->dex() == 14);
    CHECK(laladil->intel() == 13 && laladil->wis() == 16);
    CHECK(laladil->experience() == 0);
    CHECK(laladil->gold() == 26);
    CHECK(laladil->deposit() == 0);
    CHECK(laladil->rations() == 3);

    // Mirakulix (index 5): lawful Human druid.
    const Character* mira = doc.character(5);
    CHECK(mira->level() == 1);
    CHECK(mira->sex() == 1);
    CHECK(mira->alignment() == 0);
    CHECK(mira->race() == 0);
    CHECK(mira->profession() == 6);    // Druid
    CHECK(mira->hp_cur() == 16 && mira->hp_max() == 16);
    CHECK(mira->mp_cur() == 1 && mira->mp_max() == 1);
    CHECK(mira->experience() == 87);
    CHECK(mira->deposit() == 8388807);

    // Morgana (index 6): lawful elven female magician.
    const Character* morgana = doc.character(6);
    CHECK(morgana->level() == 1);
    CHECK(morgana->sex() == 0);
    CHECK(morgana->alignment() == 0);
    CHECK(morgana->race() == 2);       // Elf
    CHECK(morgana->profession() == 7); // Magician
    CHECK(morgana->mp_cur() == 3 && morgana->mp_max() == 3);
    CHECK(morgana->experience() == 164);
    CHECK(morgana->deposit() == 8388649);

    // Skills: Conan / Lara, in the engine's display order.
    CHECK(conan->skill(0) == 10 && conan->skill(1) == 8 && conan->skill(2) == 7);
    CHECK(conan->skill(3) == 34 && conan->skill(4) == 0 && conan->skill(5) == 0);
    CHECK(conan->skill(6) == 7 && conan->skill(7) == 5 && conan->skill(8) == 0);
    CHECK(lara->skill(0) == 20 && lara->skill(1) == 5 && lara->skill(2) == 6);
    CHECK(lara->skill(3) == 72 && lara->skill(4) == 10 && lara->skill(5) == 10);
    CHECK(lara->skill(6) == 15 && lara->skill(7) == 15 && lara->skill(8) == 10);

    // Languages: Lara knows all eight, Conan Common+Animal, Tuck Common only.
    for (size_t l = 0; l < 8; ++l) CHECK(lara->knows_language(l));
    CHECK(conan->knows_language(0) && conan->knows_language(1));
    CHECK(!conan->knows_language(2));
    CHECK(tuck->knows_language(0) && !tuck->knows_language(1));
    CHECK(laladil->knows_language(0) && laladil->knows_language(5));

    // Inventory: Conan's three equipped items (Ring mail, Large shield,
    // Barbarian axe) and their condition / equipped flags.
    CHECK(conan->inv_used() == 3);
    InvSlot s0 = conan->inventory(0);
    CHECK(s0.item == 31 && s0.equipped == 1 && s0.condition == 100);  // Ring mail
    InvSlot s1 = conan->inventory(1);
    CHECK(s1.item == 35 && s1.equipped == 1 && s1.condition == 100);  // Large shield
    InvSlot s2 = conan->inventory(2);
    CHECK(s2.item == 45 && s2.equipped == 1 && s2.condition == 96);   // Barbarian axe
    CHECK(tuck->inv_used() == 3);
    CHECK(tuck->inventory(2).item == 42 && tuck->inventory(2).equipped == 0);  // Thieves' picks

    // Spells: Lara has eight, in the file's order.
    CHECK(lara->spell_count() == 8);
    CHECK(lara->spell(0).id == 80 && lara->spell(0).casts == 9);   // Healing I
    CHECK(lara->spell(1).id == 81 && lara->spell(1).casts == 8);   // Light
    CHECK(lara->spell(2).id == 82 && lara->spell(2).casts == 8);   // Fear evil
    CHECK(lara->spell(3).id == 83 && lara->spell(3).casts == 7);   // Blessing
    CHECK(lara->spell(4).id == 84 && lara->spell(4).casts == 7);   // Fear undead
    CHECK(lara->spell(5).id == 85 && lara->spell(5).casts == 4);   // Mental hammer
    CHECK(lara->spell(6).id == 86 && lara->spell(6).casts == 6);   // Singing
    CHECK(lara->spell(7).id == 87 && lara->spell(7).casts == 6);   // Bind persons
    CHECK(mira->spell_count() == 1 && mira->spell(0).id == 59);    // Glow
    CHECK(morgana->spell_count() == 2);
    CHECK(morgana->spell(0).id == 1 && morgana->spell(1).id == 2); // Burning hands, Light

    // The spell array is 34 slots (0x142..0x185), so slot 33 is addressable and
    // a spell written there must survive a save/reload untouched.
    CHECK(char_off::kSpellSlots == 34);
    CHECK(lara->spell(33).id == 0 && lara->spell(33).casts == 0);
}

// A character can carry a learned-spell count with a blank array: the engine
// zeroes a slot's id on the last cast but never lowers 0x6a.  The count is a
// stored field, so saving must round-trip it instead of recomputing it.
static void test_spell_count_roundtrip(const std::string& dir) {
    const std::string src = dir + "/ROST.user";
    CharacterDocument doc = CharacterDocument::load(src);
    CHECK(doc.ok());
    if (!doc.ok()) return;

    Character* c = doc.character(0);
    CHECK(c != nullptr);
    if (!c) return;

    SpellSlot last{};
    last.id = 118;
    last.casts = 0;
    c->set_spell(33, last);
    c->set_spell_count(34);

    const std::string out = dir + "/spellcount.tmp";
    CHECK(doc.save(out));

    CharacterDocument back = CharacterDocument::load(out);
    CHECK(back.ok());
    if (!back.ok()) return;
    const Character* r = back.character(0);
    CHECK(r != nullptr);
    if (!r) return;
    CHECK(r->spell_count() == 34);
    CHECK(r->spell(33).id == 118 && r->spell(33).casts == 0);
    std::remove(out.c_str());
}

// Editing a field through the document and saving must refresh the checksum so
// the game's cheat protection stays happy.
static void test_character_edit_roundtrip(const std::string& dir) {
    CharacterDocument doc = CharacterDocument::load(dir + "/ROST.user");
    CHECK(doc.ok());

    Character* conan = doc.character(1);
    CHECK(conan != nullptr);
    conan->set_str(18);
    conan->set_gold(1234);
    conan->set_experience(99999);
    conan->set_skill(0, 55);

    const std::vector<uint8_t> out = doc.serialize();
    RostFile rost = parse_rost(out);
    CHECK(rost.ok);
    CHECK(rost.all_intact());  // every recomputed trailer matches
    CHECK(rost.records[1].character.str() == 18);
    CHECK(rost.records[1].character.gold() == 1234);
    CHECK(rost.records[1].character.experience() == 99999);
    CHECK(rost.records[1].character.skill(0) == 55);
    // Untouched characters keep their original checksums.
    CHECK(rost.records[0].name() == "Siegurd");
    CHECK(rost.records[6].character.deposit() == 8388649);

    // The sanitizer must now consider the edited image clean.
    const std::string tmp = "/tmp/lof_tools_test_edit_rost.bin";
    CHECK(write_file(tmp, out));
    SanitizeReport rep = inspect(tmp);
    CHECK(rep.file_ok && !rep.flagged);
    std::remove(tmp.c_str());
}

// The name tables the editor relies on.
static void test_tables() {
    CHECK(kItemCount == 185);
    CHECK(kSpellCount == 141);
    CHECK(std::string(item_by_index(31)->name) == "Ring mail");
    CHECK(std::string(item_by_index(35)->name) == "Large shield");
    CHECK(std::string(item_by_index(45)->name) == "Barbarian axe");
    CHECK(std::string(item_by_index(42)->name) == "Thieves' picks");
    CHECK(std::string(item_by_index(120)->name) == "The Amulet");
    CHECK(item_by_index(0) == nullptr);
    CHECK(item_by_index(186) == nullptr);
    CHECK(std::string(spell_name(80)) == "Healing I");
    CHECK(std::string(spell_name(82)) == "Fear evil");
    CHECK(std::string(spell_name(59)) == "Glow");
    CHECK(std::string(spell_name(1)) == "Burning hands");
    CHECK(std::string(spell_name(0)).empty());
    CHECK(std::string(profession_name(1)) == "Barbarian");
    CHECK(std::string(profession_name(11)) == "Monk");
    CHECK(std::string(race_name(4)) == "Dwarf");
    CHECK(std::string(alignment_name(1)) == "Chaotic");
    CHECK(std::string(sex_name(0)) == "Female");
    // Every advertised field is at least "Likely", i.e. the editor may show it.
    for (const FieldInfo& f : character_fields()) {
        CHECK(f.confidence != FieldConfidence::Uncertain);
    }
}

// End-to-end: edit a save game's party position through the document model and
// confirm the state persists with a refreshed, valid checksum.
static void test_save_editor_roundtrip(const std::string& dir) {
    CharacterDocument doc = CharacterDocument::load(dir + "/GAME4");
    CHECK(doc.ok());
    CHECK(doc.kind() == DocKind::SaveGame);
    CHECK(doc.is_save_game());
    CHECK(doc.character_count() == kSavePartySlots);
    CHECK(doc.save_location() == 0);   // GAME4: loc_ones=0, loc_tens=0
    CHECK(doc.save_facing() == 3);     // GAME4 faces West

    // Move the party and turn; also edit a party member.
    doc.set_save_location(35);         // tens 3, ones 5
    doc.set_save_facing(1);            // East
    Character* c = doc.character(0);
    CHECK(c != nullptr);
    c->set_gold(4242);

    const std::vector<uint8_t> out = doc.serialize();
    CHECK(out.size() == kSaveSize);

    SaveGame sv = parse_save(out);
    CHECK(sv.ok);
    CHECK(sv.checksum_ok);             // checksum was refreshed for the edits
    CHECK(sv.intact());
    CHECK(sv.location() == 35);
    CHECK(sv.loc_ones == 5 && sv.loc_tens == 3);
    CHECK(sv.facing == 1);
    CHECK(sv.party[0].gold() == 4242);

    // The sanitizer must consider the edited image clean.
    const std::string tmp = "/tmp/lof_tools_test_edit_save.bin";
    CHECK(write_file(tmp, out));
    SanitizeReport rep = inspect(tmp);
    CHECK(rep.file_ok && !rep.flagged);
    std::remove(tmp.c_str());

    // Out-of-range inputs are clamped so the file stays loadable.
    doc.set_save_location(200);
    doc.set_save_facing(9);
    SaveGame clamped = parse_save(doc.serialize());
    CHECK(clamped.location() == kSaveLocationMax);
    CHECK(clamped.loc_ones == 9 && clamped.loc_tens == 9);
    CHECK(clamped.facing == 3);

    // A roster ignores save-game state changes (no crash, no effect).
    CharacterDocument rost = CharacterDocument::load(dir + "/ROST.user");
    CHECK(rost.ok() && !rost.is_save_game());
    rost.set_save_location(42);
    CHECK(rost.save_location() == 0);
    CHECK(rost.serialize() == rost.serialize());
}


// The two marker values: status 7 marks a character Dead, max HP 0 marks a
// corpse the temple will no longer raise.  ROST3 holds Elzndra in exactly the
// state a failed temple revive leaves behind.
static void test_revive_state(const std::string& dir) {
    std::vector<uint8_t> file;
    CHECK(read_file(dir + "/ROST3.dead", file));
    RostFile rost = parse_rost(file);
    CHECK(rost.ok);
    if (!rost.ok || rost.records.size() < 18) return;

    const Character& dead = rost.records[17].character;
    CHECK(dead.name() == "Elzndra");
    CHECK(dead.status() == kStatusDead);
    CHECK(dead.hp_max() == 0);
    CHECK(dead.hp_cur() == 0);
    CHECK(revive_state(dead) == ReviveState::Unrevivable);

    // Every other record is alive and untouched.
    for (size_t i = 0; i < 17; ++i)
        CHECK(revive_state(rost.records[i].character) == ReviveState::Alive);

    // The repair restores max HP only, and must not resurrect the character.
    Character c = dead;
    ReviveFinding f = repair_revive(c, 5);
    CHECK(f.state == ReviveState::Unrevivable);
    CHECK(f.changes.size() == 1);
    CHECK(f.changes[0].offset == char_off::kHpMax);
    CHECK(f.changes[0].old_value == 0);
    CHECK(f.changes[0].new_value == 5);
    CHECK(c.hp_max() == 5);
    CHECK(c.status() == kStatusDead);
    CHECK(c.hp_cur() == 0);
    CHECK(revive_state(c) == ReviveState::Dead);

    // 0 falls back to the engine's own floor of 1 (asm 0xAA06).
    Character c2 = dead;
    repair_revive(c2, 0);
    CHECK(c2.hp_max() == 1);
    CHECK(revive_state(c2) == ReviveState::Dead);

    // Nothing to do on an already-revivable corpse or a living character.
    CHECK(repair_revive(c, 5).changes.empty());
    CHECK(repair_revive(rost.records[0].character, 5).changes.empty());

    // A dead character with max HP intact is reported as Dead, not unrevivable.
    Character d;
    d.set_name("Test");
    d.set_present(true);
    d.set_status(kStatusDead);
    d.set_hp_max(7);
    d.set_hp_cur(0);
    CHECK(revive_state(d) == ReviveState::Dead);
    CHECK(diagnose_revive(d, 5).changes.empty());

    // An empty slot is never reported.
    Character empty;
    CHECK(revive_state(empty) == ReviveState::Alive);
}

// The repair round-trips through the file and the cheat protection stays happy.
static void test_revive_repair_roundtrip(const std::string& dir) {
    const std::string tmp = "/tmp/lof_revive_roundtrip";
    SanitizeReport rep = sanitize(dir + "/ROST3.dead", tmp, 5);
    CHECK(rep.file_ok);
    CHECK(rep.fixed);
    CHECK(rep.has_unrevivable);
    // The cheat protection must not be tripped by the repair.
    CHECK(!rep.flagged);
    if (!rep.fixed) return;

    std::vector<uint8_t> out;
    CHECK(read_file(tmp, out));
    RostFile after = parse_rost(out);
    CHECK(after.ok);
    if (!after.ok || after.records.size() < 18) return;
    CHECK(after.all_intact());
    const Character& e = after.records[17].character;
    CHECK(e.name() == "Elzndra");
    CHECK(e.hp_max() == 5);
    CHECK(e.status() == kStatusDead);
    CHECK(e.hp_cur() == 0);
    CHECK(revive_state(e) == ReviveState::Dead);

    // Re-inspecting the repaired file reports no problem, only an info entry.
    SanitizeReport again = inspect(tmp);
    CHECK(again.file_ok);
    CHECK(!again.flagged);
    CHECK(!again.has_unrevivable);
    CHECK(again.problem_count() == 0);
    std::remove(tmp.c_str());
}

int main(int argc, char** argv) {
    const std::string dir = data_dir(argc, argv);
    test_rost_fixture(dir);
    test_rost_repair(dir);
    test_save_fixture(dir);
    test_save_repair(dir);
    test_detect(dir);
    test_end_to_end(dir);
    test_character_fields(dir);
    test_character_edit_roundtrip(dir);
    test_spell_count_roundtrip(dir);
    test_save_editor_roundtrip(dir);
    test_revive_state(dir);
    test_revive_repair_roundtrip(dir);
    test_tables();

    std::printf("%d checks, %d failure(s)\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
