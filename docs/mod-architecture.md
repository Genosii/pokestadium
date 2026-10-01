# How Pokemon Stadium Custom is built

`make RANDOMIZER=1` builds the ROM with the randomizer; a plain `make` still builds the
original, byte for byte (`md5: ed1378bc12115f71209a77844965ba50`). Everything the
randomizer adds is in `#ifdef RANDOMIZER` blocks or in files that are empty without it.
What it does for the player is in the main README; this is how it does it.

## The ground rules

1. **The first megabyte of the ROM doesn't change.** The ROM header's checksum covers
   ROM 0x1000 to 0x100FFF, and emulators recognise games by it (Project64 applies its
   Pokemon Stadium settings by it). The randomizer build leaves that range, and so the
   checksum (`90F5D9B3-9D0EDCF0`), exactly as it was: no code or data in the main
   segment changes.
2. **Nothing of the original game moves.** Every fragment the randomizer changes keeps
   the size it takes up in ROM, so everything after it stays at its address. The
   randomizer's own code goes where the ROM had padding at its end: 0x13FB0 bytes, about
   15 KB of them still free (see "Room in the ROM" below). Past that, the ROM would have
   to grow to 64 MB; nothing in the game ties it to 32 MB (its copy protection reads ROM
   0xE38 only, and ROM offsets are plain PI addresses).
3. **Screens reach the randomizer's code without relocations they can't have.** Either
   through a function pointer they get back when they load it, or through fixed
   addresses in `osAppNMIBuffer`.

## Where the code goes

`linker_scripts/us/randomizer.ld` is included in the linker script in place of the
padding file at the end of the ROM (`_1FEC050`, see the Makefile's `$(LDSCRIPT)` rule);
the ROM is then padded back to 32 MB. It holds the randomizer's own fragments, each in a
window no fragment of the game uses:

| Fragment | VRAM | Id | Loaded by | Holds |
|---|---|---|---|---|
| `randomizer_pick` | 0x8C000000 | 0xB0 | fragment61 | the pick screen: Z's random team, the team's moves panel, the control hints, the rental list's input, the teambuilder |
| `randomizer_battle` | 0x8C100000 | 0xB1 | fragment64 | the battle-select screen: Z and auto pick, the footer, random opponents |
| `randomizer_battleui` | 0x8C200000 | 0xB2 | fragment62 | the battle menus' moves and party windows |
| `randomizer_core` | 0x8C300000 | 0xB3 | fragment61, fragment64 | the team generator and building the game's Pokemon from it |
| `randomizer_cup` | 0x8C400000 | 0xB4 | fragment63 | Factory's swap panel, the "Swap a Pokemon" line, Rogue's end of run |
| `randomizer_menu` | 0x8C500000 | 0xB5 | fragment55, fragment56, fragment61 | the options window, the settings, saving them, holding the D-pad to repeat (`Randomizer_Repeat`) |
| `randomizer_options` | 0x8C600000 | 0xB6 | fragment56 | Options' "Randomizer" line |
| `randomizer_rules` | 0x8C700000 | 0xB7 | fragment55 | Rules' Z button |
| `randomizer_title` | 0x8C800000 | 0xB8 | fragment36 | the title screen's subtitle |
| `randomizer_intro` | 0x8C900000 | 0xB9 | fragment17 | the intro's random Pokemon (run once and freed) |

Shared fragments (`randomizer_core`, `randomizer_menu`) are loaded before the ones that
use them, so their relocations resolve (game-engine.md), and they refer to no screen's
own code, since they're loaded by several. Each is loaded after the screen's
`main_pool_push_state` and freed with the rest of the screen.

Their headers and relocation tables are generated after linking by
`tools/fragment_relocs.py` from the relocations the linker keeps (`--emit-relocs`),
driven by `yamls/us/fragment_regen_randomizer.txt`, and the ELF relinked with them.

## Changing the game's fragments

A changed fragment has to keep its size in ROM (code, data and relocation table
together). The regen list gives each one its size, and the tool pads the table to it
or fails the build if the fragment grew past it:

```
fragment36 func_82100C98 0x1050
fragment55 func_83002120 0x4500
fragment56 func_82C014FC 0x17B0
fragment63 func_84B03194 0x1AD50
fragment64 func_84803368 0x3C90
```

(fragment61 is in `fragment_regen.txt` with its size, 0x13030, for every build.)

Room for the code that loads and calls the randomizer is made by **moving function
bodies out**: the function stays, its body becomes a call through a hook, and the body,
with the randomizer's changes, goes into the randomizer's fragment. For example,
fragment56's `func_82C00658` (the Options window) and fragment55's `func_8300059C` and
`func_830015EC` (the Rules list's input and text). Dead code can go too: fragment36's
`func_82100054` has debug destinations behind a flag nothing sets, and randomizer builds
drop them to make room for loading `randomizer_title`. And fragment61's level-sum check
(`func_84206A68`) moved into `randomizer_pick` to make room for the teambuilder's hooks. A fragment's statics that the moved
code needs lose their `static` in randomizer builds only.

**fragment62, the battle, is the exception.** Its relocation table can't be rebuilt
byte for byte from the decomp yet (some of the addresses in it aren't known symbols),
so its layout can't change at all. `fragment_regen.txt` has it **spliced**:

```
fragment62 splice func_84301430 func_84340ACC:0x1E4 func_843172A0 func_84317558
```

keeps the extracted table and only regenerates the entries of the listed functions,
which keep their exact size. `func_84340ACC`, never called, is replaced by
`randomizer_battle_ui_stub.s`, exactly 0x1E4 bytes: a loader for `randomizer_battleui`
and small stubs that jump to a hook if one is set, else to the game's own function.
Calls to the game's functions are redirected to the stubs with `#define`s in
fragment62_2FA4D0.c (the same size of call, a different target).

**fragment17, the intro, is spliced the same way**
(`fragment17 splice func_86B01190 func_86B044B0:0x50` in the randomizer's list). Its
table lists every relocation, even those into main code, and has no room to spare, so a
change must not add entries: `func_86B044B0`, never called and with ten relocations, is
replaced by `randomizer_intro_stub.s`, which needs six. The intro's setup calls the
stub in place of `func_8002D510`; the stub runs `randomizer_intro` with
`func_80029008` (load, call, free) and then calls `func_8002D510`.

## How screens reach the randomizer

**Hooks returned by the entry point.** The screen loads the fragment and calls its
entry point, which returns a structure of function pointers; the screen keeps the
pointer in a variable of its own and calls through it:

```c
// fragment56's entry, func_82C014FC
FRAGMENT_LOAD(randomizer_menu);
sRandomizerHooks = ((RandomizerOptionsEntry)FRAGMENT_LOAD(randomizer_options))();
// ...
void func_82C00658(s16 arg0, s32 arg1) {
    sRandomizerHooks->draw(arg0, arg1);
}
```

Used by fragment36 (`RandomizerTitleHooks`), fragment55 (`RandomizerRulesHooks`),
fragment56 (`RandomizerOptionsHooks`),
fragment61 (`RandomizerPickHooks`, in `gRandomizerPickHooks`) and fragment64
(`RandomizerBattleHooks`).

**Hooks at fixed addresses.** Where the screen can't take the relocation that a
pointer into another fragment needs (fragment62's spliced table), or has no room for a
variable, the randomizer fragment's entry point writes its functions into
`gRandomizerState` in `osAppNMIBuffer`, and the screen calls through that fixed
address, which needs no relocation. fragment62's stubs read 0x80000328, 0x8000032C,
0x80000350 and 0x80000354; fragment63 calls through `gRandomizerState.cupHooks`
(`RANDOMIZER_CUP_HOOKS`). The loader zeroes the battle's hooks before loading, so a
missing fragment means the game's own behaviour.

## The state: `gRandomizerState`

`src/randomizer_state.h`: a structure over `osAppNMIBuffer` (0x8000031C, 64 bytes),
which only power-on clears, so it lasts across screens and the Reset button. It counts
as set up once `magic` is `"RND7"`; `Randomizer_State()` (randomizer_menu.c) sets it up
the first time, from the save file or the defaults. A GCC-only typedef checks the
offsets the assembly relies on.

| Offset | Field |
|---|---|
| 0x00 | `magic`, "RND7", changed whenever the layout does |
| 0x04 | `lastSeed`: the last team's seed |
| 0x08 | `enteredSeed`: a website seed typed in for the next team |
| 0x0C | `battlePartyHook3` (0x80000328) |
| 0x10 | `battlePartyHook6` (0x8000032C) |
| 0x14 | `settings`: the team's options (8 bytes, `RandomizerSettings`) |
| 0x1C | `autoBattlePick`, 0x1D `useEnteredSeed`, 0x1E `randomOpponents` |
| 0x20 | `opponentSeed`: the run's seed for the opponents' teams |
| 0x24 | `opponentSettings` (8 bytes) |
| 0x2C | `mode`: Normal, Factory or Rogue |
| 0x30 | `cupHooks` |
| 0x34 | `battleHintHook` (0x80000350) |
| 0x38 | `battleHintForcedHook` (0x80000354) |
| 0x3C | free, 4 bytes |

## The teambuilder

`src/fragments/61/randomizer_editor.c`, in `randomizer_pick`. It edits six Pokemon in
place, the team being entered or a registered team, and recalculates each with
`func_80022734` after every change.

- "Edit Pokemon" is a line added at run time to the team's menus (`D_84211704[0]` and
  `[10]`: one more line, and for menu 0 a taller window), drawn by the randomizer at the
  end of each frame since the menus draw their own lines only.
- Picking it calls the `editTeam` hook from the menus' handlers (`func_8420720C`,
  `func_842073A4`), which puts the team panel in a state of its own, 17, whose input
  `func_8420776C` hands to the `editInput` hook. Closing it runs the cup's level-sum rule
  and goes back to the menu, as the game does when the last Pokemon is picked.
- In "Check registered Pokemon", `func_8420F86C` asks the `checkInput` hook first: Z
  starts editing the highlighted team, and closing writes it back over its own entry
  (game-data.md) and reads it again for the screen.
- The rental card's prompt (game-screens.md) has "Edit" between Yes and No:
  `func_8420B40C` (drawing) and `func_8420C368` (input) call the `cardPrompt` and
  `cardInput` hooks. Edit answers Yes and remembers the team slot it goes into; once the
  list has added it and the team has settled (back to waiting for a pick, or its menu if
  that was the sixth), the teambuilder opens on that Pokemon alone, and closing it goes
  back to where the team was. The rental list ignores the frame the teambuilder closes in,
  or the B that closed it would take the Pokemon back out.
- Holding the D-pad repeats a quarter of a second in, eight times a second
  (`Randomizer_Repeat`, timed by the CPU's counter since screens run at different frame
  rates); a held button stops at the end of a list, a press goes round.
- Moves come from the generator's learnsets (`gRandomizerLearnsets`, from the website),
  sorted by name; the move list shows the game's own power, accuracy and PP
  (`D_80072B00`).

## Saving

Everything the randomizer saves is in the 0x17C bytes at the end of save bank 2 that no
section covers (game-data.md), laid out in `src/randomizer_save.h`:

| Offset in bank 2 | What | Written |
|---|---|---|
| 0x3E04 | `RandomizerSave`, the options, 0x1C bytes | when a panel closes with them changed |
| 0x3E20 | `RandomizerBootCount`, 8 bytes | every time the intro starts |

`randomizer_menu.c` keeps the options at 0x3E04: a `RandomizerSave` of 0x1C bytes,
the magic `"RNDS"`, both settings structures, the mode, random opponents and auto
battle pick, and a checksum of its own. They're read the first time the state is set
up after power-on, and written when a panel closes with them changed, the way the game
saves bank 2 (`func_80028AFC(2)`, then `D_800AE4E8[2].unk_00 |= 2` and
`func_800284B4(2)`). A save file without them, or with anything else there, gives the
defaults, and the game's own sections aren't touched. `D_800AE4E8` is static in
26820.c, so it's reached at its fixed address (0x800AE4E8), which can't move since the
main segment doesn't.

The boot count is the intro's: its seed is the CPU's cycle count, which differs between
boots on a console but not in an emulator, which starts the same way every time, so it's
mixed with a count of boots written back each time.

## The team generator

`randomizer_logic.c` is a C port of the random team generator website's
`randomizeFullTeam()` and its helpers (`js/randomize.js`), for the Stadium 1 modes.
Random numbers come from the same generator (a 32-bit LCG, `x * 0x19660D +
0x3C6EF35F`, used as a fraction of 2^32) in the same order, so a seed and the options
give the same team in the game and on the website.

- `randomizer_data.c` is generated from the website's data by
  `tools/randomizer/gen_data.py`: species (types, flags such as legendary and final
  evolution), learnsets (each move marked Gen 1 or tradeback only) and moves, with Gen
  1's own move data applied. They're one structure, `gRandomizerData`
  (`gRandomizerSpecies` and the others are its fields): as C for the PC build, and
  compressed for the ROM (below).
- `tools/randomizer/gen_rentals.py` goes the other way, exporting the game's rental
  Pokemon to the website (`json/s1_rentals.json`) for the "Stadium" moveset, DV and stat
  exp options.
- `tools/randomizer/parity/check.py` builds the generator natively (`host.c`), runs it
  and the website (under Node.js) over every cup, moveset and option combination for a
  number of seeds, and compares the teams.
- `randomizer_build.c` turns a generated team into the game's 0x54-byte Pokemon
  (`Randomizer_BuildPokemon`) and holds each rule set's levels and rental file.

Opponents (`src/fragments/64/randomizer_opponents.c`) are generated as the
battle-select screen starts, over the trainers the game loaded, from a seed mixed from
the run's seed and the trainer's place (mode, ball or gym, round, side, slot), so the
same trainer gets the same team after a retry.

## Text in mixed case

`tools/randomizer/gen_text_case.py` rewrites the text archive and the trainer and rental
archive (game-data.md) with the Game Boy games' capitals turned into mixed case, and the
Makefile builds a `RANDOMIZER=1` ROM from its output (`build/randomizer/text`) instead of
the extracted files. Only the case of letters changes, so every string, file and archive
keeps its size and nothing moves; both archives are outside the checksummed range.

- Names (from the Pokemon, move, item, type, gym and trainer class files, and a list in
  the tool) are capitalised word by word: "Karate Chop", "Mr.Mime", "Hall of Fame".
  Abbreviations (HP, PP, OT, ID, TM, COM, GB, KO, ...) and START stay as they are.
- Text that's all capitals otherwise goes to sentence case ("Delete saved data"); other
  capitalised words in sentences are capitalised ("choose Continue"), apart from a few
  plain words (on, off, now, ball, stone). The credits don't change.
- Trainers' names and their Pokemon's nicknames and original trainers are converted too,
  with the original trainer's name in Game Boy characters (0x46 in each Pokemon) to match:
  saving a team converts names to the Game Boy's characters (`func_80021B7C`, with a table
  in main code), which have lowercase.
- `--review FILE` lists every string that changes.

What doesn't change: Pokemon from a Game Boy cartridge have the names stored there, and
some text is part of a picture (the title's "PRESS START").

## Room in the ROM

The build fails if the randomizer outgrows the end of the ROM (the `ASSERT` at the end of
`randomizer.ld`); what's left is 0x2000000 minus the end of the last fragment there
(`nm build/pokestadium-us.elf | grep randomizer_intro_relocs_ROM_END`). The biggest
things are kept small this way:

- **Compressed with the game's own Yay0.** `Yay0_Decompress(src, dst)` (main code,
  0x8000B7F0; `src` 4-byte aligned) unpacks into the fragment's bss when it loads:
  - the generator's tables, 11474 bytes stored in 5008: `gen_data.py` packs
    `RandomizerData` byte for byte, a typedef in `randomizer_data.c` fails the build if
    the structure's size changes, and `Randomizer_UnpackData` is called by the pick
    screen's and the battle-select screen's randomizer fragments as they load, before
    anything uses the core;
  - the title's subtitle, 15360 bytes stored in about 10 KB: unpacked row after row,
    then each row moved out to 256 pixels, from the last one back.
  Both tools need crunch64, which is in the repo's `.venv`. The price is RAM while the
  screen is up, of which there's plenty: the main pool had about 234 KB free on the pick
  screen with the teambuilder open, and 814 KB on the battle-select screen (its
  `available`, at 0x800A608C, read from emulator savestates).
- **The Z button icon is IA8** (32x24, a byte a pixel, greyscale like the game's L and R
  icons; `tools/randomizer/gen_z_icon.py`), drawn with `func_8001CADC`. The
  battle-select footer draws the game's icons in copy mode, which takes 16-bit textures
  only, so it switches to the blended mode (`D_8006F518`) for the Z.
- Turning off loop unrolling (`-Wo,-loopunroll,0`) was tried: the randomizer's code
  comes out byte for byte the same.

## Testing

- **Emulator runs**: `tools/headless_test/run.sh ROM SCRIPT OUTDIR` runs mupen64plus
  without a screen, presses buttons from a script by frame number, and saves
  screenshots (tools/headless_test/README.md). Every run starts from an empty save.
- **Parity**: `tools/randomizer/parity/check.py PATH_TO_WEBSITE --seeds N`.
- **The original still builds**: `make` must still give the original MD5, and a
  randomizer build must leave everything up to the end of the checksummed range
  identical to the base ROM:
  `cmp -n $((0x101000)) build/pokestadium-us.z64 baseroms/us/baserom.z64`.

## Adding a hook to another screen

1. Find the function to change and check the fragment's room: is it in a regen list
   with a size? If not, check that its table regenerates byte for byte
   (`tools/fragment_relocs.py check ELF LIST --all`) before listing it with its size.
2. Make room by moving a function body into a new randomizer fragment (a new window,
   0x8CA00000 is next), and give that fragment an entry that returns its hooks.
3. Load any shared randomizer fragment the new one uses first, then the new one, after
   the screen's `main_pool_push_state`.
4. Add the fragment to `randomizer.ld`, the regen list, `RANDOMIZER_FRAGMENT_OBJS` in the
   Makefile, and the fragment's directory to the Makefile's `RANDOMIZER_FLAG`
   dependency.
5. Build both ways: `make` must match, and `make RANDOMIZER=1` must leave ROM 0x0 to
   0x100FFF identical.
