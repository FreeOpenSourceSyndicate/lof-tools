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
| `lof-tools`         | `src/gui`   | Qt6 GUI (menu shell + sanitizer dialog) |
| `lof-sanitize`      | `src/cli`   | headless sanitizer |
| `lof-tests`         | `tests`     | unit tests over `testdata/` |
| `lofcore` (static)  | `src/core`  | reusable data model + cheat-protection logic |

## Build

```sh
sudo apt install build-essential cmake qt6-base-dev   # once
cmake -S . -B build && cmake --build build -j
ctest --test-dir build --output-on-failure
```

Qt6 is optional; without it only `lof-sanitize` and `lof-tests` are built.

## Task 1 (done): cheat-protection sanitizer

Offers a dialog to open a save game or a `ROST` roster, checks whether the
game's cheat protection would flag the file (or any character), and repairs it.
Reusable parts live in `src/core` for the planned editors.

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
off 4      : 6 party characters, 410 bytes each
...        : further game state (location, facing, map cache, ...)
off 11834  : 4-byte little-endian checksum of bytes [0, 11834)
off 11838  : 1296-byte (36x36) map region, NOT covered by the checksum
```

The engine validates the whole-file checksum at asm `0x1C823`: it checks the
magic (`0x1C814` `cmp es:[bx],0x4600`; `0x1C81B` `cmp es:[bx+2],0x4C4F`), then
`mov ax,0x2E3A` (11834), `push es`/`push bx`, `lcall 0x1BF1,8` (the *same*
routine as the ROST checksum) and compares the returned `DX:AX` with the stored
`[bp-0x51a]/[bp-0x518]`. A mismatch prints " is not a valid game:" and refuses
the file. Party characters carry the same `0xFC` tamper byte at `+0x0C`, acted
on at asm `0xB0CB`.

`testdata/` holds the verified fixtures: `ROST1.cheat`, `ROST2.banked`,
`GAME1` (Temple, loc 5), `GAME4` (Cyldane, loc 0). The known-good ROST trailers
are asserted in `tests/test_core.cpp`.

## Character struct offsets (410 bytes)

Grounded in lof2's `docs/notes-b.md` and cross-checked against the fixtures.
See `src/core/character.h` (`char_off`) for the full list. Key ones:

| off | size | meaning |
|----:|-----:|---------|
| 0x00 | byte | present (non-zero = slot in use) |
| 0x01 | 14 | NUL-terminated name |
| 0x0C | byte | tamper flag (`0xFC` = engine thinks this char is edited) |
| 0x17 | byte | level / rank |
| 0x1b | byte | profession index (0..11; 0xff/0x0c = none) |
| 0x1e | byte | total defence / armour class |
| 0x20/0x22 | word | max / current HP |
| 0x44..0x48 | bytes | Con, Str, Dex, Int, Wis |
| 0x64/0x66 | word | max / current carried weight |
| 0x68/0x69 | byte | max / current MP |
| 0x6b | byte | used inventory-slot count |
| 0x6c | dword | experience |
| 0x70 | byte | rations |
| 0x72 | dword | gold |

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
- The GUI shell (`MainWindow`) owns the menu bar; add new tools as entries under
  **Tools** rather than restructuring the window.

## Next tasks

- **Save-game editor** (planned): reuse `src/core/savegame.*` and
  `src/core/character.*`; add a `Tools` menu entry and a dialog.
- **Character editor** (planned): reuse `src/core/character.*`; works on both a
  save's party and a ROST record. After any edit, refresh the checksum so the
  sanitizer is a no-op.
