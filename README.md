# lof-tools

Modding tools for the 1990 DOS RPG **Legend of Faerghail** (reLINE Software).

The first tool is a **sanitizer** for save games and `ROST` rosters: the game
implements a cheat protection, and this tool detects (and repairs) files that
the game would flag as modified. A save-game editor and a character editor are
planned; the reusable data model they need is already in `src/core`.

This is an independent, unofficial project. It is not affiliated with the
original authors or publishers. You need your own copy of the game data to use
it on real files.

## What the cheat protection does

`Legend of Faerghail` stores an integrity checksum with its character data and
refuses (or punishes) files whose bytes do not match it.

- **`ROST/ROST` rosters** store, after each 410-byte character struct, a 4-byte
  little-endian checksum of those 410 bytes. On a mismatch the engine sets the
  character's tamper byte (`+0x0C`) to `0xFC`, which later triggers the
  "caught ... trying to fraud" message and a penalty.
- **Save games (`GAMES/GAMEn`)** store one 4-byte checksum covering the first
  11834 bytes of the file. On a mismatch the engine rejects the file with
  " is not a valid game:". Party characters carry the same `0xFC` tamper byte.

The checksum is not a plain sum and not a CRC: it is an alternating ADD/XOR fold
with a carry into the high word, recovered from the engine's routine at image
offset `0x1BF18`:

```
acc = 1
for i in 0 .. n-1:
    c = data[i]
    if c == 0: c = 1        # NUL bytes are folded as 1
    if i is even: acc += c  # ADD, carry propagates into the high word
    else:         acc ^= c  # XOR
checksum = acc & 0xFFFFFFFF
```

The sanitizer recomputes this value, clears any stale tamper flag, and writes
the file back only when something actually changed.

## Building

Requirements:

- A C++17 compiler (GCC 9+ or Clang 10+)
- CMake 3.16+
- Qt6 Widgets **development** packages (optional - without them only the CLI is
  built)

Debian / Ubuntu:

```sh
sudo apt install build-essential cmake qt6-base-dev
```

Configure and build:

```sh
cmake -S . -B build
cmake --build build -j
```

This produces:

| Binary                | What it is |
|-----------------------|------------|
| `build/lof-tools`     | The Qt6 GUI (built when Qt6 is found) |
| `build/lof-cli`       | Headless CLI (sanitizer + character dump) |
| `build/lof-tests`     | Unit tests |

Run the GUI:

```sh
./build/lof-tools
# or open a file straight away:
./build/lof-tools /path/to/GAME1
# or open the character editor on a file:
./build/lof-tools --editor /path/to/ROST
```

## Using the GUI

The window opens on a welcome page. **File > Open save / ROST...** loads a file
and switches to the tools, which are shown as **tabs** (one per tool, so new
tools appear as new tabs):

- **Sanitizer** - lists every problem the cheat protection would find (per
  character for a roster, whole-file for a save) and offers **Fix & Save** to
  write the repaired file back.
- **Character editor** - edits a `ROST` roster or a save game one character at a
  time: identity, attributes, hit/magic points, experience, gold (purse and
  deposit), rations, the nine skills, the eight languages, the inventory (48
  slots, with item, equipped flag, condition and charges) and the spell list.
  Saving recomputes the integrity checksum, so the game's cheat protection
  accepts the file.

  Its field groups reflow: they pack into as many rows as the window width
  allows, so narrowing the window adds rows instead of a horizontal scrollbar.
  A horizontal scrollbar only appears if a single group no longer fits on a row
  of its own.
- **Save game editor** - edits the non-character state of a save game: the party
  roster (which slots are in use) and the party's position - the dungeon id the
  engine loads (`DNGS\DNGnn`) and the compass facing. Double-clicking a party
  member jumps to that slot in the character editor. Saving recomputes the
  checksum. (A `ROST` roster has no such state, so the tab explains that when a
  roster is open.)

The file is always opened from the menu bar (**File > Open**); the tabs have no
separate Open button, since a single open feeds every tab.

Opening a file loads it into *every* tool at once, so you can switch tabs without
re-opening. **File > Reload** re-reads it and **File > Close file** returns to the
welcome page. When a tool writes the file, the other tabs refresh. The **Tools**
menu switches between tabs (and opens a file first if none is loaded). The window
sizes itself to fit the screen and the tool tabs scroll, so it works on small
displays.

## Using the CLI

```sh
# Report only (exit code 1 if the game would flag the file):
./build/lof-cli check GAMES/GAME1

# Repair in place:
./build/lof-cli fix GAMES/GAME1

# Repair into a new file (leaves the original alone):
./build/lof-cli fix ROST/ROST ROST/ROST.fixed

# Print every decoded character field (read-only):
./build/lof-cli dump ROST/ROST

# Print a save game's state: party, location, facing (read-only):
./build/lof-cli state GAMES/GAME1
```

## Tests

```sh
ctest --test-dir build --output-on-failure
# or directly:
./build/lof-tests testdata
```

The tests run against the committed fixtures in `testdata/` (`ROST1.cheat`,
`ROST2.banked`, `ROST.user`, `GAME1`, `GAME4`) and cover the checksum, parsing,
detection, end-to-end repair, the decoded character fields (asserted value by
value against the maintainer's character sheet), and the edit round-trip.

## Character format

The full, verified field map of a character record lives in
[`docs/character-format.md`](docs/character-format.md). Fields are only offered
for editing once they are confirmed from the disassembly and real data; the
document lists what is still uncertain.

## Layout

```
src/core/         Reusable, GUI-free data model and cheat-protection logic.
src/core/tables/  Item/spell/name tables transcribed from LOF.EXE.
src/cli/          Headless front-end (sanitizer + character dump).
src/gui/          Qt6 Widgets front-end (tabbed shell + tool panels).
tests/            Unit tests.
testdata/         Small fixtures used by the tests.
docs/             Format documentation.
```

`src/core` is deliberately independent of the GUI so the sanitizer, the
character editor and the future save-game editor share one model. The
`CharacterDocument` class loads either a `ROST` roster or a save game and hands
out mutable `Character` records; it is the piece a save-game editor will reuse.

## License

GPL-3.0-or-later. See `LICENSE`.
