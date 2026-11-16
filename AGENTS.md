# AGENTS.md - project knowledge for future agents

Read this first. It records everything a new agent needs to continue this
project after a workspace reset or a handover.

## What this project is

`lof-tools` is a collection of tools to modify the old DOS game **Legend of
Faerghail** (reLINE Software, 1990). It is C++ (C++17), built with CMake, with a
Qt6 Widgets GUI.

The sibling repo <https://github.com/FreeOpenSourceSyndicate/lof2> recreates the
game and contains a lot of decoded format knowledge. **We do not contribute to
lof2**, but its `docs/` and `port/char.*` are the authoritative reference for
the data formats. Do not re-derive what it already documents; cite it instead.

Deliverable so far:

| Binary              | Source | Purpose |
|---------------------|--------|---------|
| `lof-tools`         | `src/gui`   | Qt6 GUI (menu shell + sanitizer + character editor + save-game editor) |
| `lof-cli`           | `src/cli`   | headless sanitizer + character/save dump |
| `lof-tests`         | `tests`     | unit tests over `testdata/` |
| `lofcore` (static)  | `src/core`  | reusable data model + cheat-protection logic |

## Build

```sh
sudo apt install build-essential cmake qt6-base-dev   # once
cmake -S . -B build && cmake --build build -j
ctest --test-dir build --output-on-failure
```

Qt6 is optional; without it only `lof-cli` and `lof-tests` are built.

### GUI shell

`MainWindow` is a tabbed shell: **File > Open** loads one file into every
`ToolPanel` tab (`SanitizerPanel`, `CharacterEditorPanel`, `SaveEditorPanel`);
tools never open files themselves. A tool that writes the file emits
`ToolPanel::file_changed`, and `MainWindow` reloads every *other* tab from it
(the writer refreshes itself). The character editor's field groups live in
`src/gui/flow_layout.h` (`FlowLayout`), which packs them into as many rows as the
width allows so narrowing the window adds rows instead of a horizontal
scrollbar; the scrollbar only appears when one group cannot fit a row alone.

The save-game editor double-clicks a party member to ask `MainWindow` to switch
to the character editor on that slot (`open_character_editor_slot` ->
`CharacterEditorPanel::select_slot`). Keep cross-tool jumps routed through the
window, not through direct panel-to-panel calls.

## Task 1 (done): cheat-protection sanitizer

Checks whether the game's cheat protection would flag the open file (or any
character) and repairs it. Shown as the **Sanitizer** tab; the file is opened
from the main window. Reusable parts live in `src/core` for the editors.

### The cheat protection (fully decoded, verified)

**Checksum algorithm** (the engine's routine at image offset `0x1BF18`, far-
called `0x1BF1:0x0008`; verified on 23/23 ROST records and every save):

```
acc = 1
for i in 0 .. n-1:
    c = data[i]; if c == 0: c = 1
    if i even: acc += c   else: acc ^= c
return acc & 0xFFFFFFFF    # stored little-endian
```

**ROST/ROST**: one count byte, then N records of 414 bytes. Each record is a
410-byte character struct plus a 4-byte little-endian checksum of those 410
bytes. Mismatch -> the engine sets char byte `+0x0C = 0xFC` (asm `0x2A1F6`).

**Save games (GAMES/GAMEn)**, 13134 bytes total:

```
off 0      : 4-byte header {00 'F' 'O' 'L'}
off 4      : 6 party characters, 410 bytes each (ends at off 2464)
...        : further game state (map cache, ...)
off 0x9a2  : byte  ones digit of the party's dungeon id
off 0x9a4  : byte  tens digit of the party's dungeon id
off 0x9a6  : byte  party facing (0=N .. 3=W)
off 11834  : 4-byte little-endian checksum of bytes [0, 11834)
off 11838  : 1296-byte (36x36) map region, NOT covered by the checksum
```

The party's dungeon id is split into two decimal digits; the engine composes the
map name `DNGS\DNG%01d%01d` from tens and ones (asm `0x4bb35`). `SaveGame`
exposes it as a single value `location() == tens*10 + ones` and `set_location()`
re-splits it; `facing` is 0..3. Both live inside the checksummed region, so the
editor changes them through `CharacterDocument`, whose `save()` recomputes the
whole-file checksum (see `serialize_save()`).

The engine validates the whole-file checksum at asm `0x1C823`: it checks the
magic (`0x1C814` `cmp es:[bx],0x4600`; `0x1C81B` `cmp es:[bx+2],0x4C4F`), then
`mov ax,0x2E3A` (11834), `push es`/`push bx`, `lcall 0x1BF1,8` (the *same*
routine as the ROST checksum) and compares the returned `DX:AX` with the stored
`[bp-0x51a]/[bp-0x518]`. A mismatch prints " is not a valid game:" and refuses
the file. Party characters carry the same `0xFC` tamper byte at `+0x0C`, acted
on at asm `0xB0CB`. The engine's own save loader is at asm `0x1C952`.

`testdata/` holds the verified fixtures: `ROST1.cheat`, `ROST2.banked`,
`GAME1` (Temple, loc 5), `GAME4` (Cyldane, loc 0). The known-good ROST trailers
are asserted in `tests/test_core.cpp`.

## Character struct offsets (410 bytes)

Fully decoded and verified: every field below reproduced the maintainer's
hand-made character sheet byte-for-byte for seven characters (Siegurd, Conan,
Tuck, Lara, Laladil, Mirakulix, Morgana) and cross-checks against the shipped
saves. The authoritative, complete map is
[`docs/character-format.md`](docs/character-format.md); the code of record is
`src/core/character.h` (`char_off`). Key fields:

| off | size | meaning |
|----:|-----:|---------|
| 0x00 | byte | present (non-zero = slot in use) |
| 0x01 | 14 | NUL-terminated name |
| 0x0C | byte | tamper flag (`0xFC` = engine thinks this char is edited) |
| 0x17 | byte | level / rank |
| 0x18 | byte | sex (0 female, 1 male) — lof2 wrongly calls this "class flag" |
| 0x19 | byte | alignment (0 lawful, 1 chaotic) |
| 0x1a | byte | race (0 Human .. 5 Half-Orc) |
| 0x1b | byte | profession index (0..11; 0xff/0x0c = none) |
| 0x1e | byte | total defence / armour class (engine-recomputed; read-only) |
| 0x20/0x22 | word | max / current HP |
| 0x25/0x27/0x28/0x2b/0x2d/0x30/0x32/0x34/0x36 | bytes | the nine skills, in the sheet builder's read order (Negotiating, Attack, Defence, Concentration, Pick-pocketing, Stalking, Trap detecting, Trap disarming, Lock picking) |
| 0x44..0x48 | bytes | Con, Str, Dex, Int, Wis |
| 0x64/0x66 | word | max / current carried weight (shown /10) |
| 0x68/0x69 | byte | max / current MP |
| 0x6a | byte | learned-spell count |
| 0x6b | byte | used inventory-slot count |
| 0x6c | dword | experience (signed) |
| 0x70 | byte | rations |
| 0x72 | dword | gold (purse) |
| 0x76 | dword | gold (deposit / bank) — not in lof2 |
| 0x7a | 8 bytes | language flags (one per language) |
| 0x82 | 192 | 48 inventory slots x 4 bytes (item, equipped, charges, condition) |
| 0x142 | 40 | 20 spell slots x 2 bytes (spell index, casts) |

Fields we are **not** yet sure about (deliberately left out of the editor) are
listed at the end of `docs/character-format.md`.

## Task 2 (done): save-game editor

`SaveEditorPanel` (`src/gui/save_editor_panel.*`) is a tab that edits the
non-character state of a save game and shows its party. It is built entirely on
the reusable core so a later editor can share the same pieces:

- `SaveGame` (`src/core/savegame.h`) parses the file and exposes the mutable
  state: `location()` / `set_location()` (the dungeon id, `tens*10 + ones`),
  `facing` / `set_facing()`, and the party as `Character` records.
  `serialize_save()` copies the party and the digit bytes back into the raw image
  and refreshes the whole-file checksum.
- `CharacterDocument` (`src/core/document.h`) is the single editable model for
  both rosters and saves. The panel edits through it, so `save()` always writes a
  checksum-valid file: `is_save_game()`, `save_location()`/`set_save_location()`,
  `save_facing()`/`set_save_facing()` (no-ops for a roster).
- Name tables (`src/core/tables.h`, `src/core/names.cpp`): `facing_name()`,
  `scenario_name()` (the 8 scenario names), and `dungeon_map_name()` for the
  `DNGS\DNGnn` label.

Verified end-to-end: an offscreen probe drove the panel's widgets, clicked
**Save**, and the written file parsed back with the new location/facing and a
valid checksum (`sv.intact()`). `tests/test_core.cpp::test_save_editor_roundtrip`
covers the same path plus clamping of out-of-range input.

## Conventions / hygiene

- **Commit and push regularly.** The workspace can reset; unpushed work is lost.
- Work on a branch and open a PR; CI (`.github/workflows/build.yml`) builds and
  runs `ctest` on every PR and on `main`. Merge only when CI is green and the PR
  is `MERGEABLE`/`CLEAN`. The repo has no other maintainers, so the agent owns
  reviewing and merging its own PRs.
- `testdata/` is committed on purpose (small fixtures needed by the tests).
  Real game data (`game/`, `GAME*` archives, `*.EXE`) must **not** be committed.
- Keep `src/core` free of Qt and of any GUI dependency so the planned editors
  and the CLI can both use it.
- The GUI shell (`MainWindow`) owns the menu bar and a **tab per tool**
  (`ToolPanel` subclasses: `SanitizerPanel`, `CharacterEditorPanel`). Opening a
  file loads it into every tool at once; a tool that writes the file emits
  `ToolPanel::file_changed` so the others refresh. **Add a new tool as a tab**,
  not a new window. Keep the default window size within the screen and put large
  forms in a `QScrollArea`.

## Next tasks

- **Save-game editor** (done for party position): `SaveEditorPanel` edits the
  party position (dungeon id + facing) and shows the roster; the character editor
  already edits a save's party members. More non-character state (bank/gold,
  game time, map cache) can be added once its layout is confirmed from the
  disassembly - only offer a field once it is verified.
- The character editor, the save editor and `lof-cli` share the same
  `CharacterDocument` model; keep them in sync when new fields are confirmed.
- When new characters (other classes/races) or shop item orders arrive from the
  maintainer, extend `testdata/` and the assertions in `tests/test_core.cpp`.
