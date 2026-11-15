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
| `build/lof-sanitize`  | Headless CLI sanitizer |
| `build/lof-tests`     | Unit tests |

Run the GUI:

```sh
./build/lof-tools
# or open a file straight away:
./build/lof-tools /path/to/GAME1
```

## Using the GUI

- **File > Open save / ROST...** opens a file and shows what the cheat
  protection thinks of it.
- **Tools > Sanitizer (cheat protection)...** opens the same view without a
  file; use **Open file...** inside it.
- The dialog lists every problem (per character for a roster, whole-file for a
  save) and offers **Fix & Save** to write the repaired file back.
- **Tools > Save game editor** and **Tools > Character editor** are placeholders
  for the planned editors.

## Using the CLI

```sh
# Report only (exit code 1 if the game would flag the file):
./build/lof-sanitize check GAMES/GAME1

# Repair in place:
./build/lof-sanitize fix GAMES/GAME1

# Repair into a new file (leaves the original alone):
./build/lof-sanitize fix ROST/ROST ROST/ROST.fixed
```

## Tests

```sh
ctest --test-dir build --output-on-failure
# or directly:
./build/lof-tests testdata
```

The tests run against the committed fixtures in `testdata/` (`ROST1.cheat`,
`ROST2.banked`, `GAME1`, `GAME4`) and cover the checksum, parsing, detection,
and end-to-end repair.

## Layout

```
src/core/    Reusable, GUI-free data model and cheat-protection logic.
src/cli/     Headless front-end.
src/gui/     Qt6 Widgets front-end (menu shell + sanitizer dialog).
tests/       Unit tests.
testdata/    Small fixtures used by the tests.
```

`src/core` is deliberately independent of the GUI so the future save-game and
character editors can share it.

## License

GPL-3.0-or-later. See `LICENSE`.
