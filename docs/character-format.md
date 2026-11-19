# Character record format (verified)

This is the authoritative field map for a `Legend of Faerghail` character
record, as used by the tools in this repository. Every field listed as
**verified** was reproduced byte-for-byte from the roster the maintainer
supplied (seven characters: Siegurd, Conan, Tuck, Lara, Laladil, Mirakulix,
Morgana) and cross-checked against the shipped save games (`GAME1`, `GAME4`).

Offsets are sizes in bytes from the start of a record; the "asm" note cites the
image offset of the routine that reads or writes the field.

## Where records live

| Container | Layout |
|-----------|--------|
| `ROST/ROST` | 1 count byte, then N records of **414** bytes: a 410-byte character struct followed by a 4-byte little-endian checksum of those 410 bytes. |
| `GAMES/GAMEn` | 13134 bytes: a header, then contiguous **410**-byte character structs (party slots), then one 4-byte checksum covering the first 11834 bytes. |

The character struct itself is always **410 (0x19a)** bytes.

## Field map

| Off | Size | Field | Confidence | Notes |
|----:|-----:|-------|-----------|-------|
| 0x00 | 1 | present | verified | non-zero = slot holds a character |
| 0x01 | 14 | name | verified | NUL-terminated ASCII |
| 0x0c | 1 | tamper flag | verified | `0xFC` = the engine's "fraud" marker; cleared on save |
| 0x17 | 1 | level / rank | verified | |
| 0x18 | 1 | sex | verified | 0 = female, 1 = male (the lof2 project labels this "class flag"; it is sex) |
| 0x19 | 1 | alignment | verified | 0 = lawful, 1 = chaotic |
| 0x1a | 1 | race | verified | 0 Human, 1 Half-Elf, 2 Elf, 3 Halfling, 4 Dwarf, 5 Half-Orc |
| 0x1b | 1 | profession | verified | 0..11 (0x0c/0xff = none, e.g. the Siegurd template) |
| 0x1e | 1 | defence (AC) | likely | recomputed by the engine's armour routine; shown read-only |
| 0x20 | 2 | HP max | verified | word |
| 0x22 | 2 | HP current | verified | word |
| 0x25 | 1 | skill: negotiating | verified | read by the sheet builder in this order |
| 0x27 | 1 | skill: attack | verified | |
| 0x28 | 1 | skill: defence | verified | |
| 0x2b | 1 | skill: concentration | verified | |
| 0x2d | 1 | skill: pick-pocketing | verified | |
| 0x30 | 1 | skill: stalking | verified | shown as "Sneak" on the sheet |
| 0x32 | 1 | skill: trap detecting | verified | |
| 0x34 | 1 | skill: trap disarming | verified | |
| 0x36 | 1 | skill: lock picking | verified | |
| 0x44 | 1 | constitution | verified | |
| 0x45 | 1 | strength | verified | |
| 0x46 | 1 | dexterity | verified | |
| 0x47 | 1 | intelligence | verified | |
| 0x48 | 1 | wisdom | verified | |
| 0x64 | 2 | weight max | verified | word; the game shows it / 10 |
| 0x66 | 2 | weight current | verified | word; accumulated from the inventory |
| 0x68 | 1 | MP max | verified | |
| 0x69 | 1 | MP current | verified | |
| 0x6a | 1 | spell count | verified | number of learned spells |
| 0x6b | 1 | inventory used | verified | count of used inventory slots (0..48) |
| 0x6c | 4 | experience | verified | signed 32-bit; may be negative |
| 0x70 | 1 | rations | verified | |
| 0x72 | 4 | gold (purse) | verified | 32-bit |
| 0x76 | 4 | gold (deposit) | verified | 32-bit; the bank balance |
| 0x7a | 8 | languages | verified | one flag byte per language; non-zero = known |
| 0x82 | 192 | inventory | verified | 48 slots x 4 bytes |
| 0x142 | 68 | spells | verified | 34 slots x (spell index, remaining casts) |

### Inventory slot (4 bytes at `0x82 + 4*n`)

| Byte | Field | Notes |
|-----:|-------|-------|
| 0 | item index | 1-based into the 185-item table (0 = empty) |
| 1 | equipped | 1 = worn / wielded |
| 2 | charges | remaining count / charges |
| 3 | condition | durability 0..100 (100 = pristine, 0 = broken) |

### Spell slot (2 bytes at `0x142 + 2*n`, n = 0..33)

| Byte | Field | Notes |
|-----:|-------|-------|
| 0 | spell index | 1-based into the 141-spell table (0 = empty) |
| 1 | casts | remaining casts |

The array is **34 slots**, not 20. Proof: the engine's spell scan loop runs
`cmp word ptr [bp-0x34], 0x22` (34) over `es:[bx+si+0x142]` with `bx = slot*2`
(asm `0x1387D` / `0x1389E`), and `0x142 + 34*2 = 0x186`, exactly where the next
character field starts (read at asm `0x29195`).

`0x6a` (the learned-spell count) is **not** a derived value. The engine raises it
when a spell is learned and never lowers it when a spell is spent: on the last
cast it only zeroes the slot's spell index (asm `0x12A5E`) and leaves `0x6a`
alone. A character can therefore have `0x6a = 1` with an all-zero spell array,
which is exactly what the game displays: the spell list screen iterates
`0x6a` entries, so the slot is still listed (with no casts left). Editors must
round-trip `0x6a` verbatim instead of recomputing it from the array.

## Enumerations and tables

All tables live inside `LOF.EXE` (no data file carries them). The name tables in
`src/core/tables/` are transcriptions from the lof2 project (`port/char_items.inc`,
`port/char_spells.inc`, `port/char_names.inc`), cross-checked against the data.

| Table | Count | Values |
|-------|------:|--------|
| sex | 2 | Female, Male |
| race | 6 | Human, Half-Elf, Elf, Halfling, Dwarf, Half-Orc |
| profession | 12 | Warrior, Barbarian, Rogue, Smith, Scout, Priest, Druid, Magician, Illusionist, Paladin, Healer, Monk |
| alignment | 2 | Lawful, Chaotic |
| skills | 9 | Negotiating, Attack, Defence, Concentration, Pick-pocketing, Stalking, Trap detecting, Trap disarming, Lock picking |
| languages | 8 | Common, Animal, Orc, Lizard, Dwarven, Elven, Dark, Magic |
| items | 185 | index 0 = Club ... index 119 = The Amulet |
| spells | 141 | index 0 = Burning hands ... |

## Integrity checksum

The engine's routine at image offset `0x1BF18` (records: `0x2A1E1`):

```
acc = 1
for i in 0 .. n-1:
    c = data[i]; if c == 0: c = 1
    if i even: acc += c   else: acc ^= c
return acc & 0xFFFFFFFF    # stored little-endian
```

For a ROST record `n = 410`; for a save game `n = 11834`. A mismatch sets the
character's tamper byte (`+0x0c`) to `0xFC`, which later triggers the
"... trying to fraud" message and a penalty. The tools always recompute the
checksum when they save, so an edited file passes the cheat protection.

## Fields we are *not* yet sure about

These are deliberately left out of the editor (see `docs/RULES.md`): no field is
offered for editing unless it is verified or at least firmly grounded. They can
be added once confirmed.

- The exact meaning of the attribute-modifier bytes `0x5b`, `0x5c`, `0x5e`,
  `0x5f` and the flags `0x6a`, `0x0f`, `0x24` (they feed the party-status view).
- The two-byte seed pair at `0x14` and the SNSC seed vector at `0x7a` for
  monsters (PCs use the language bytes there; monsters carry combat stats).
- The item-table fields beyond name/weight (class/effect bytes) are not decoded.
