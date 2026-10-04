# The game's data

## The save file

The cartridge's save is 128 KB of Flash (emulators store it as a 131072-byte `.fla`;
mupen64plus's has each 32-bit word byte-swapped), handled by src/26820.c. It holds four **banks**, each kept twice:

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
| 16 | banks 0 and 1 entirely, and bank 2 from 0x0000 | 9 groups of 10 entries of 0x160 | group in `D_800AE4E0` (0 to 8, set with `func_80028C48`), bank = group / 4. Each entry is a 0x10-byte header then Pokemon in the saved format below. The registered teams, 10 per rule set: the pick screen picks the group with `func_80028C48(rule set)` and loads bank `rule set / 4` (`func_84203E6C`). Each entry's header holds the trainer ID (`unk_0C`) and name |
| 17 | bank 3, 0x0000 | 12 entries of 0x468 | |
| 18 | bank 3, 0x34E0 | 4 entries of 0xE0 | 0x20 unused bytes follow, to 0x3880 |
| 19 | bank 2, 0x0DC0 | 0xF60 | |
| 21 | bank 2, 0x1D20 | 0x180 | |
| 20 | bank 2, 0x1EA0 | 0x28 | `unk_02`: Options' sound and voice flags (`func_80028070`, `func_80028084`). `unk_0C[2]`: 8 bytes per round (`func_80027FA0`, `func_80027FE0`); when round 1's first u16 is 0x1F8, the title screen and the intro use their second background (index 0x11), very likely once everything is cleared. 0x01 and 0x20 to 0x21 are unused |
| 22 | bank 2, 0x1EC8 | 0x13B8 | |
| 23 | bank 2, 0x3280 | 0xB84 | |
| (none) | bank 2, 0x3E04 | 0x17C | covered by no section: read and written with the bank, never looked at. The randomizer keeps its data here (src/randomizer_save.h) |

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

### Reading and writing registered teams

Through a handle (`unk_func_80022C28_ret`, src/232C0.c):

```c
// Reading entry i of the current group (func_8420F204)
save = func_80022C28(0x10, 0, i, 0);
count = 0;
for (j = 0; j < 6; j++) {
    count += func_80022E18((u8*)&mons[j], 1, save); // into the 0x54-byte structure
}
func_80022D8C(save);

// Writing it (func_84206990): opening it for writing starts its Pokemon over from the
// first (func_800276F0, called with 0) and writes the ID and name (func_80027430)
save = func_80022CC0(0x10, 0, i, 0, name, id);
for (j = 0; j < count; j++) {
    func_80022F24((u8*)&mons[j], 1, save);
}
func_80022D8C(save);                                // the section's 'POKE' and sum
func_800286D8();                                    // to the cartridge
```

`func_80028E68` counts the group's registered teams.

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

## The text archive

All the game's text is one archive at ROM 0x783760 (`textdata`, 0x15570 bytes,
uncompressed), loaded whole at boot (`func_8002D510`). `func_8002D5AC(n)` returns file
`n`, `func_8002D7C0(buf, size, file, i)` its string `i`, with `#NN` in a string standing
for a number or a string set with `func_8002D600` / `func_8002D5D4` (a Pokemon's name in a
battle message, for example).

Archives in this game (this one and the trainer and rental archive, at least) start with
16 bytes ending in their number of files, then 16 bytes per file: its offset from the
archive's start, its size and two zeros. A text file is a u32 count, the offsets of its
strings from the file's start, then the strings, Latin-1 (`\xE9` é, `\xBE` ♀ and `\xA9` ♂
in the game's font).

| File | Holds |
|---|---|
| 0 to 9 | the PC, boxes, items, the Pokedex and its areas |
| 10, 11, 12 | Pokedex entries, move descriptions, item and TM descriptions |
| 13, 20 | Game Boy connection and controller messages |
| 14 to 19, 23, 24, 26, 27, 31, 32 | modes, cups, rounds, rules, rentals, the Hall of Fame |
| 21 | Options |
| 25 | the gyms |
| 28 | Surfing Pikachu |
| 29 | the Kids Club |
| 30 | battle messages |
| 33 | the trade service |
| 34, 35 | trainer classes |
| 36, 37 | Pokemon names (151) and move names (165) |
| 38, 39, 40 | types, items, Pokemon categories |
| 41 | statuses |
| 44 | the album and the gallery |
| 45 | the staff credits |

The names in main code (`D_8006FEE8`, `func_80021CA4`, used for the nicknames of new
Pokemon) hold placeholders, `pmname_001` and on, until the game fills them in at boot.

## Moves

The move table is at ROM 0x73700, RAM 0x80072B00 (`D_80072B00`), 165 entries of 6
bytes: id, effect, power, type, accuracy (out of 255), PP.

Gen 1 type ids: 0 Normal, 1 Fighting, 2 Flying, 3 Poison, 4 Ground, 5 Rock, 7 Bug,
8 Ghost, 20 Fire, 21 Water, 22 Grass, 23 Electric, 24 Psychic, 25 Ice, 26 Dragon.

The table is Gen 1's, differences included: Karate Chop, Gust, Sand-Attack and Bite
are Normal, Dig has 100 power, Wing Attack 35 and so on
(`json/gen1_move_overrides.json` on the website lists them).

## Per-species battle data

Tables in the ROM file at `_70D3A0_ROM_START`, each read with `func_80003B30` at
`_70D3A0_ROM_START + (offset & 0xFFFFFF)`, species - 1 as the index:

| Offset | Entry | Holds |
|---|---|---|
| `D_80075BD0[species - 1]` | 0xBC0 bytes, 188 entries of 0x10 | the animations (first byte of an entry, an index into the model's animations): one per move at the move's id - 1, the idle stance at 165, the reaction to a hit at 168 (fragment62's `func_84302658`) |
| `D_70110` | 0x10 | the battle's `D_84390028`: two f32 sizes (0x00 across, 0x04 up: Pikachu 30 and 25, Onix 110 and 90) that the battle scales its effects and camera by, and the f32 height it floats at (0x08: Mew 30, Aerodactyl 80, Gastly 70) |
| `D_70B10` | 0x20 | two points (`Vec3f`) the battle's camera aims at for close-ups of the Pokemon, for the left side (x is negated for the right) |
| `D_6E910` | 0x20 | the battle's `D_84384580` |

The battle stands the Pokemon 150 units either side of the middle, 175 for Venusaur and
Lapras, 225 for Onix and Gyarados (`func_84307C5C`), the left one facing right
(`unk_01E.y` 0x4000) and the right one left (-0x4000), at scale 1. The rental card's
`D_8006FF00[species - 1]` gives the scale it shows each model at (`unk_02` / 100, to about
536 units tall) and the model's middle (`unk_14`), so a model is about 536 / that scale
units tall at scale 1.

