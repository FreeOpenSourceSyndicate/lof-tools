// Headless front-end for the sanitizer.  Useful for scripting and for verifying
// the GUI's results.
//
//   lof-sanitize check  <file>            report what the cheat protection sees
//   lof-sanitize fix    <file> [out]      repair in place (or write to `out`)
#include <cstdio>
#include <string>

#include "core/sanitizer.h"

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

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr,
                     "usage: lof-sanitize check <file>\n"
                     "       lof-sanitize fix   <file> [out]\n");
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

    std::fprintf(stderr, "unknown command: %s\n", cmd.c_str());
    return 2;
}
