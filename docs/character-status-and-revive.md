# Character status byte and the temple revive

Everything below is read off the decompilation in
`lof2/analysis/lof.asm` (addresses are image offsets) and cross-checked
against the shipped saves and the rosters in this repo. Nothing here is a
guess.

## The status byte is `+0x1f`

`+0x1f` is the character's **condition / status**. It indexes the engine's
own status table: the short forms are 8 fixed 6-byte records at image
`0x3cca0`, the long forms a NUL-separated list at `0x3ccd0`:

| value | short | long |
|------:|-------|------|
| 0 | `Good`  | Good |
| 1 | `Mad`   | Mad |
| 2 | `Ill`   | Ill |
| 3 | `Poisn` | Poisoned |
| 4 | `Blnd.` | Blind |
| 5 | `Stun.` | Stunned |
| 6 | `Stone` | Stoned |
| 7 | `Dead`  | Dead |

(Dumped straight from the image: `0x3cca0` holds `Good  Mad   Ill
Poisn Blnd. Stun. Stone Dead ` and `0x3ccd0` holds
`Good\0Mad\0Ill\0Poisoned\0Blind\0Stunned\0Stoned\0Dead\0`. The same names
are emitted into `lof2/port/char_names.inc` as `kStatusShortNames` /
`kStatusLongNames`. Note that an earlier revision of this file cited
`0x42190`, which is a bank message, not the status table.)

Proof that `+0x1f` is this byte and not something else:

* **Death** sets it to 7. The death site zeroes current HP and then writes 7:
  ```
  258D8: mov word ptr es:[bx + si + 0x22], 0     ; hp_cur = 0
  258EF: mov byte ptr es:[bx + si + 0x1f], 7     ; status  = Dead
  ```
* **Revive** clears it to 0 (`25EA3`, `0A964`, `0AB41`).
* **On load** a character with `hp_cur < 1` that is not already `Dead` is
  forced to `Dead` (`1517`..`1536`):
  ```
  1517: cmp word ptr es:[bx + 0x22], 1   ; hp_cur >= 1 ?
  151C: jge 0x1540                       ;   yes -> leave alone
  151E: cmp byte ptr es:[bx + 0x1f], 7   ; already Dead ?
  1523: je  0x1540                       ;   yes -> leave alone
  1536: mov byte ptr es:[bx + si + 0x1f], 7
  ```
* Other conditions use the same byte: poison handling tests `== 3` (`161C`),
  stun handling tests `== 5` and counts down `+0x49` (`155AA`..`15F1`).
* The party-status row copies `+0x1f` into the display block and formats it
  as a small integer (`18177`, `188BB`).

## Maximum HP is never zeroed by death

The death site zeroes `hp_cur` only; `hp_max` (`+0x20`) is left intact.
Every reader of `hp_max` guards the divide-by-zero explicitly, e.g. the HP
percentage in the status builder (`2EF75`):

```
2EF75: cmp word ptr es:[bx + 0x20], 0
2EF7A: je  0x2ef8d          ; skip the percentage when hp_max == 0
```

So a *normally* dead character always has `hp_max > 0`.

## The temple revive

The temple's `(R)evive` handler is the routine entered around `0xA4DA`. It
rolls a Constitution check and then either revives or destroys the corpse.

Success chance (`0xA87B`..`0xA8A2`):

```
0A87B: inc ax              ; ax = rand(0..99) + 1  -> 1..100
0A88F: mov al, es:[bx+si+0x44]   ; Constitution
0A89B: add ax, dx ; ax = Con*5
0A89D: add ax, 5   ; ax = Con*5 + 5
0A8A0: cmp ax, cx  ; Con*5+5  vs  roll
0A8A2: jb  0xa8bc  ; roll wins -> FAILURE
0A8B5: cmp byte es:[bx+si+0x44], 0
0A8BA: jne 0xa92a  ; Con != 0   -> SUCCESS
```

Success (`0xA92A` onward):

* `dec` Constitution if it is above 1 (`0A94A`..`0A950`),
* `status = 0` (`0A964`),
* add experience — profession base, scaled by `2^(level-1)` and Con
  (`0A986`..`0A9E2`),
* force `hp_cur >= 1` (`0AA06`..`0AA0C`).

Failure (`0xA8BC`): prints three messages and then **zeroes both HP fields**:

```
0A8FE: mov word ptr es:[bx + si + 0x20], 0   ; hp_max = 0
0A915: mov word ptr es:[bx + si + 0x22], 0   ; hp_cur = 0
```

That is the engine's way of saying the body is too badly damaged to raise.
It is the only site in the image that writes `hp_max = 0`.

## Why a failed revive is permanent

The revive routine refuses any character whose `hp_max` is already 0
(`0xA7AA`):

```
0A7AA: cmp word ptr es:[bx + si + 0x20], 0
0A7AF: je  0xa7b4      ; hp_max == 0 -> refuse ("cannot be revived")
0A7B1: jmp 0xa65c      ; otherwise continue
```

So after one failed attempt the character is stuck: `Dead` with `hp_max = 0`
can never pass the guard, and no in-game action writes `hp_max` back.

The direction of that branch is not a guess. The success path above never
writes `+0x20`: it only sets status, decrements Con, adds experience and
clamps `hp_cur`. So if the guard let an `hp_max == 0` corpse through, the
"revived" character would keep `hp_max == 0` for good -- alive with a
maximum of zero hit points, which the rest of the engine treats as a
degenerate case (every `hp_max` reader divides by it behind a zero check).
`0xA8FE` is the only write of `hp_max = 0` in the image, and it sits on the
failure path, so the guard exists to stop exactly that state from being
revived. Hence `je 0xa7b4` takes the refusal branch.

## Consequence for the tool

`status == Dead && hp_max == 0` is a dead end that no normal death produces
(death keeps `hp_max`). It is **not corruption**: the engine writes it itself
when a temple revive fails, so both `ROST` and `ROST3` are valid game states.
The tool reports which marker values are present and can put the character
back into the ordinary dead state so the temple will accept it again.

The max HP value itself is **not recoverable from the file**: the failed
revive overwrote it and the 410-byte struct holds no shadow copy. In the
`ROST` / `ROST3` pair every byte except the status/HP/Con/exp fields is
identical, which confirms there is nothing to restore from. The repair
therefore needs the value supplied from outside the file (the number the
character had before the failed attempt, or a value from a backup), with a
provable floor of 1 — the engine never creates or revives a character with
less (`1DA84` clamps creation to >= 1, `0AA06` clamps revive to >= 1).

## Observed data

`ROST` and `ROST3` are the same 7453-byte roster with 18 records. Record 17
is `Elzndra` (record base `0x1B7F`, so name at `0x1B80` and `+0x20` at
`0x1B9F`). Only six bytes inside her record differ:

| char offset | `ROST` (revived) | `ROST3` (broken) | field |
|---|---|---|---|
| `+0x1f` | 0 (`Good`)  | 7 (`Dead`) | status |
| `+0x20` | 5           | 0          | hp_max |
| `+0x22` | 1           | 0          | hp_cur |
| `+0x44` | 13          | 14         | Constitution |
| `+0x6c` | 2195        | 106        | experience |

`ROST` is the result of a *successful* revive: Constitution went 14 -> 13,
experience went up, status is `Good`, and `hp_cur` was forced to 1. `ROST3`
is the state *after a failed* attempt: status `Dead`, both HP fields zeroed,
Constitution and experience untouched. Her real max HP was 5.
