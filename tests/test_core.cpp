// Unit tests for the reusable core.  Run from the repository root (or pass the
// testdata directory as argv[1]).
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "core/checksum.h"
#include "core/file_io.h"
#include "core/rost.h"
#include "core/sanitizer.h"
#include "core/savegame.h"

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

int main(int argc, char** argv) {
    const std::string dir = data_dir(argc, argv);
    test_rost_fixture(dir);
    test_rost_repair(dir);
    test_save_fixture(dir);
    test_save_repair(dir);
    test_detect(dir);
    test_end_to_end(dir);

    std::printf("%d checks, %d failure(s)\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
