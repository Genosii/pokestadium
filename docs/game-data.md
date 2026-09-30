# The game's data

## The save file

The cartridge's save is 128 KB of Flash (emulators store it as a 131072-byte `.fla`),
handled by src/26820.c. It holds four **banks**, each kept twice:

| Bank | Size | Flash sector (16 KB) | Backup copy's sector |
|---|---|---|---|
| 0 | 0x3700 | 0 | 4 |
| 1 | 0x3700 | 1 | 5 |
| 2 | 0x3F80 | 2 | 6 |
| 3 | 0x3880 | 3 | 7 |

A bank is only in RAM while some screen has loaded it: `func_80028AFC(bank)` loads it
into the memory pool (both copies, twice the bank's size) unless it's there already,
and returns whether it is. Its bookkeeping is in `D_800AE4E8[4]` (static in 26820.c, at
0x800AE4E8): `unk_00` bit 0 loaded, bit 1 needs writing; `unk_04` the bank, `unk_08`
its backup copy. Freeing the pool block unloads it.

### Sections and their checksums

Each bank is made of **sections**, and each section ends with 6 bytes: `'PO' 'KE'` and
a u16 sum of every byte before the sum (the `'POKE'` included; `func_80025C80`). When a
bank loads, every section is checked (`func_80025F50`): a bad main copy with a good
backup is restored from the backup, and a section bad in both is reset to zeros
(`func_8002707C`). The functions take the section's number:

| Section | Where | Size | Notes |
|---|---|---|---|
| 16 | banks 0 and 1 entirely, and bank 2 from 0x0000 | 9 groups of 10 entries of 0x160 | group in `D_800AE4E0` (0 to 8, set with `func_80028C48`), bank = group / 4. Each entry is a 0x10-byte header then Pokemon in the saved format below. The pick screen loads bank `rule set / 4` (`func_84203E6C`), so these are very likely the registered teams, 10 per rule set (to be confirmed with the teambuilder) |
| 17 | bank 3, 0x0000 | 12 entries of 0x468 | |
| 18 | bank 3, 0x34E0 | 4 entries of 0xE0 | 0x20 unused bytes follow, to 0x3880 |
| 19 | bank 2, 0x0DC0 | 0xF60 | |
| 21 | bank 2, 0x1D20 | 0x180 | |
| 20 | bank 2, 0x1EA0 | 0x28 | `unk_02`: Options' sound and voice flags (`func_80028070`, `func_80028084`). `unk_0C[2]`: 8 bytes per round (`func_80027FA0`, `func_80027FE0`); when round 1's first u16 is 0x1F8, the title screen and the intro use their second background (index 0x11), very likely once everything is cleared. 0x01 and 0x20 to 0x21 are unused |
| 22 | bank 2, 0x1EC8 | 0x13B8 | |
| 23 | bank 2, 0x3280 | 0xB84 | |
| (none) | bank 2, 0x3E04 | 0x17C | covered by no section: read and written with the bank, never looked at. The randomizer keeps its settings here |

**Changing a section** is the same everywhere in the game:

```c
func_80028AFC(2);            // bank 2 in RAM (loads it if needed)
// ... change the section's bytes
func_800264DC(20, 0, 2);     // mark section 20, entry 0, as changed
func_80026684(20, 0);        // rewrite its 'POKE' and sum, mark the bank as needing a write
func_800284B4(2);            // write bank 2: copy to the backup, erase and write both sectors
```

`func_800286D8` writes every bank that needs it. Writing bytes outside any section
(the bank 2 tail) needs the bank marked directly: `D_800AE4E8[2].unk_00 |= 2`, then
`func_800284B4(2)`. "Delete saved data" on the Options screen resets the sections
(`func_82C01054`) and leaves the tail alone.

## Pokemon

### In memory: 0x54 bytes

`unk_func_80026268_arg0` (src/29BA0.h). Used for the teams in `D_800AE540`, the rental
lists, the trainer data and in battle.

| Offset | Size | Field |
|---|---|---|
| 0x00 | 1 | species, Pokedex number |
| 0x01 | 1 | species, Gen 1 internal index |
| 0x02 | 2 | current HP |
| 0x04 | 1 | level as stored in the box structure |
| 0x05 | 1 | status |
| 0x06, 0x07 | 1 each | types |
| 0x08 | 1 | the Gen 1 catch rate byte |
| 0x09 | 4 | moves (move ids, 0 for none) |
| 0x0E | 2 | original trainer's ID |
| 0x10 | 4 | experience |
| 0x14 to 0x1C | 2 each | stat exp: HP, Attack, Defense, Speed, Special |
| 0x1E | 2 | DVs: `Attack << 12 \| Defense << 8 \| Speed << 4 \| Special` (HP follows from them) |
| 0x20 | 4 | PP, PP Ups in the top two bits |
| 0x24 | 1 | level |
| 0x26 to 0x2E | 2 each | max HP, Attack, Defense, Speed, Special |
| 0x30 | 11 | nickname |
| 0x3B | 11 | original trainer's name |
| 0x46 | 11 | the original trainer's name in Gen 1's text encoding, as saved |
| 0x52, 0x53 | 1 each | where it came from: a rental is `(player << 4) \| 0xD` and its index in the rental list |

`func_80022734` works out the level, stats, HP and PP from the species, experience,
DVs and stat exp. Species base data is `D_80070F84[]` (`PokemonStats`, src/22630.h;
the types are at 0x0B and 0x0C). `func_800224B8(species, level)` gives the experience
for a level.

### In the save: Gen 1's own format

The saved format (`unk_D_800AE4E8_004_1_000_010`, src/22630.h) is Gen 1's box
structure, 0x21 bytes big-endian, followed by the nickname and the original trainer's
name in Gen 1's text encoding (11 bytes each), 0x37 bytes in all: species (internal
index), HP, level, status, types, catch rate, moves, OT ID, experience (3 bytes), stat
exp (5 x 2), DVs, PP. `func_80021D9C` and `func_80026268` turn it into the 0x54-byte
structure, `func_80021F04` and `func_800262DC` back. It holds everything a teambuilder
would change: moves, PP, DVs, stat exp and level. The Game Boy party format
(`unk_D_800AC910_050_9AC_008`) adds the level and the five stats (0x2C bytes).

## The trainer and rental archive

An archive at ROM 0x898000 (extracted by `make init` into `assets/us/898000/N.bin`),
loaded with `func_800044F4(0x898000, ...)` and read with `func_8000484C(archive, n)`.
Every file starts with a u32 count and is padded to 16 bytes.

**Trainer files**: entries of 0x230 bytes: the name at 0x00, the number of Pokemon at
0x37, then six 0x54-byte Pokemon from 0x38.

| Files | Trainers |
|---|---|
| 0 to 11 | lists of 8 (the cups' trainers) |
| 12 to 19 | the Gym Leader Castle's gyms, Brock to Giovanni, 4 each (the leader last) |
| 20 | the Elite Four |
| 21 | the Rival, 7 entries |
| 22 | Mewtwo |
| 29 | Blue and Red |

**Rental files**: entries of 0x54-byte Pokemon. The pick screen chooses one by rule set
(`func_84203C90`); Round 2 has its own, 0x1F further on.

| Rule set | File |
|---|---|
| 3, Poke Cup | 0x1C |
| 4, Petit Cup | 0x17 |
| 5, Pika Cup | 0x18 |
| 6, Prime Cup, and 8 | 0x19 |
| 7, Gym Leader Castle | 0x1E |
| 0 and 1 | 0x1A |
| 2 | 0x1B |

## Moves

The move table is at ROM 0x73700, RAM 0x80072B00 (`D_80072B00`), 165 entries of 6
bytes: id, effect, power, type, accuracy (out of 255), PP.

Gen 1 type ids: 0 Normal, 1 Fighting, 2 Flying, 3 Poison, 4 Ground, 5 Rock, 7 Bug,
8 Ghost, 20 Fire, 21 Water, 22 Grass, 23 Electric, 24 Psychic, 25 Ice, 26 Dragon.

The table is Gen 1's, differences included: Karate Chop, Gust, Sand-Attack and Bite
are Normal, Dig has 100 power, Wing Attack 35 and so on
(`json/gen1_move_overrides.json` on the website lists them).
