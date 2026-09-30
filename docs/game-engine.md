# The game's engine

## Boot and the state machine

`Game_Thread` (src/29BA0.c) runs one loop for the whole game: it switches on
`gCurrentGameState` (`GameState` in include/variables.h) and calls that state's handler,
which returns once the player leaves and sets the next state.

| State | Value | Handler | What runs |
|---|---|---|---|
| `STATE_N64_LOGO_INTRO` | 0x01 | `func_80029310` | fragment35 (the N64 logo), then the intro: `func_800290E4(0x12)` |
| `STATE_TITLE_SCREEN` | 0x02 | `func_800293CC` | fragment36; if it times out, a demo (see below) |
| `STATE_MENU_SELECT` | 0x04 | `func_80029924` | Battle Now! / Stadium / Gallery / Event Battle / Options |
| `STATE_AREA_SELECT` | 0x10 | `func_80029828` | fragment37 |
| `STATE_OPTIONS` | 0x13 | `func_800298D4` | fragment56 |
| `STATE_STADIUM_MENU` | 0x20 | `func_80029BC0` | the cups: fragment59, fragment60, fragment54 (Battle/Rules/Registration), fragment61, then 63, 64, 62, 63 per battle |
| `STATE_GYM_LEADER_CASTLE` | 0x27 | `func_8002AAA8` | fragment65, then the same battle loop |

Screens are fragments (below). A handler usually runs one with
`FRAGMENT_LOAD_AND_CALL(fragmentN, arg0, arg1)`, which is `func_80029008`: load the
fragment, call its entry point with the two arguments, free it, return what the entry
point returned.

### The intro and the title screen's demos

After the N64 logo, `func_800290E4(0x12)` pushes a `'Demo'` pool state, loads
fragment34, fragment2 and fragment3, sets `D_800AE540.unk_0000 = 0x12`, and runs
fragment17, the "stage" player, whose entry (`func_86B00020`) picks what to play by
that mode: 0x12 is the intro (`func_86B01190`), and 0x11, 0x15, 0x16/0x17, 0x18 and
0x1A are other scripted stages. See game-screens.md for the intro's scene tables.

The title screen (fragment36) starts a demo once it has run about 600 frames and
`func_800484E0` returns 0 (`func_82100958`; probably the title music ending), by
returning into `func_82100844`: alternately a demo battle between two random Pokemon (mode 0x10,
`func_82100760` picks them from the per-species table `D_82100DCC` through a bit mask),
or a minigame demo (mode 0x19, the minigame in `D_800AE540.unk_0003`, 3 = fragment8,
9 = fragment14, 13 = fragment18 in `func_800293CC`).

## The global battle and mode state: `D_800AE540`

`unk_D_800AE540` in src/29BA0.h, the one structure most screens read:

| Field | Meaning |
|---|---|
| `unk_0000` | the mode. 1 to 8 load computer trainers from the trainer data (`func_8002C128`), 7 is the Gym Leader Castle, 0x10 a title-screen demo battle, 0x12 the intro, 0x19 a minigame demo |
| `unk_0001` | the rule set: 3 Poke Cup, 4 Petit Cup, 5 Pika Cup, 6 Prime Cup, 7 Gym Leader Castle, 0 to 2 the other level 50-55 modes (see `Randomizer_GetRules` in src/fragments/61/randomizer_build.c) |
| `unk_0002` | the ball (cups) or the gym (Castle, 0 to 7 Brock to Giovanni, 8 the Elite Four) |
| `unk_0003` | the round within it (the Castle's leader is round 4; the Elite Four are 1 to 4) |
| `unk_0004[4]` | the players' and trainers' teams (`unk_D_800AE540_0004`): `unk_000 & 2` marks a computer trainer, `unk_002` is the number of Pokemon, `unk_01C[6]` the Pokemon (0x54-byte structure, game-data.md), `unk_214` the copy the battle-select screen reads |
| `unk_1194[2]` | the two sides: `unk_01` trainers, `unk_08[]` pointers into `unk_0004` |
| `unk_11F2` | set in Round 2 |
| `unk_11F6` | bit 0: the screen between a cup's battles is told to quit (the run is over) |

## The memory pool

Everything a screen loads comes from one pool (src/memory.c). Screens bracket their
allocations with `main_pool_push_state('TAG')` and `main_pool_pop_state('TAG')`
(the tag is a four-character constant, checked at the pop), and everything allocated
in between, fragments included, is freed at the pop. Blocks can carry a function
called when they're freed (`main_pool_alloc_with_func`); fragments and the save
banks use it to clear their bookkeeping.

## Fragments: the game's code overlays

Almost all screen code lives in "fragments": code overlays in ROM that are loaded
anywhere in the pool and relocated. src/memmap.c and src/3FB0.c hold the loader.

**Header** (`struct Fragment`, src/memmap.h), 0x20 bytes at the start of the fragment:

| Offset | Field |
|---|---|
| 0x00 | `j entry` and a `nop`: the entry point, relocated like any other jump |
| 0x08 | `"FRAGMENT"` |
| 0x10 | header size, 0x20 |
| 0x14 | `relocOffset`: size of code + data + rodata; the relocation table follows |
| 0x18 | `sizeInRom`: `relocOffset` + the table's size |
| 0x1C | `sizeInRam`: `relocOffset` + bss |

**Relocation table**: a u32 count, then one u32 per relocation,
`(type << 24) | offset from the start of the fragment`, with MIPS types 2 (`R_MIPS_32`),
4 (`R_MIPS_26`), 5 (`R_MIPS_HI16`) and 6 (`R_MIPS_LO16`). The table is zeroed after
use, and the block shrunk or grown to `sizeInRam`.

**Address windows.** Each fragment is linked at its own 1 MB window from 0x81000000
up to 0x8FFFFFFF. Its id is `((vram & 0x0FF00000) >> 20) - 0x10` (the `FRAGMENT_ID`
macro): 0x81000000 is id 0, 0x8C000000 is id 0xB0. `gFragments[240]` records where
each loaded id is in RAM, and `Memmap_GetFragmentVaddr` turns any address in a window
into the loaded one.

**Loading** is `func_80004454(id, romStart, romEnd)` (`FRAGMENT_LOAD`): read the
fragment from ROM (decompressing it if it's `PERS-SZP` or `PRESJPEG`), record it in
`gFragments`, and relocate it (`Memmap_RelocateFragment`). It returns the entry point.

Things to know when working with fragments:

- **Relocations reach other fragments.** A relocation whose target is in another
  fragment's window is resolved to where *that* fragment is loaded, so fragment A can
  call fragment B directly, but only if B was loaded before A.
- **A missing fragment is not an error.** If the target window has nothing loaded,
  `Memmap_GetFragmentVaddr` leaves the address as it was (a link-time address like
  0x8C5xxxxx, which isn't mapped). It only crashes when that code runs.
- **Only fragment windows get relocations.** Code and data in the main segment
  (0x80000000 to 0x80FFFFFF) never move and need none; a fixed address like
  0x80000350 can be called or read from any fragment without a relocation.
- **bss isn't cleared.** A loaded fragment's statics hold whatever was in that memory;
  entry points have to reset their own.
- **Freeing** the fragment's pool block clears its `gFragments` entry
  (`func_80004364`).

Assets (models, textures, text) are loaded into the 16 display-list segments instead
(`func_80004258`, `ASSET_LOAD`; `gSegments[16]`), or as archives: `func_800044F4`
loads an archive and `func_8000484C(archive, index)` returns one of its files.

## osAppNMIBuffer: memory that survives Reset

libultra reserves 64 bytes at 0x8000031C (`osAppNMIBuffer`) for the game. They're only
cleared at power-on, not by the Reset button, and Pokemon Stadium never uses them.
The randomizer keeps its state there (mod-architecture.md).

## Frames, input and drawing

A screen's loop usually looks like this:

```c
while (!done) {
    func_800290B4();        // read the controllers for the new frame
    // ... input: BTN_IS_PRESSED(gPlayer1Controller, BTN_A) etc.
    func_800079C4();        // start the frame's display list
    // ... draw
    func_80007778();        // finish it
}
```

- **Input** (src/controller.h): `gControllers[4]`, `gPlayer1Controller`;
  `BTN_IS_PRESSED` is a new press this frame, `BTN_IS_DOWN` held.
- **Resolution**: the menus (Options, Rules, the pick screen) draw at 640x480, the title
  screen and battles at 320x240.
- **Sound effects**: `func_80048B90(id)`; the menus use 1 for moving the cursor, 2 to
  confirm, 3 to cancel or close, 4 to open a window.

### Text (src/1CF30.h, src/2E110.h)

| Function | Does |
|---|---|
| `func_8001E94C(n, 0)` | called once as a screen starts, with 0x1C in the hi-res menus and 6 on the title screen and in the intro; sets up text for the screen (likely the font or text mode) |
| `func_8001F3F4()` / `func_8001F444()` | start / end a block of text drawing; 2D drawing goes outside these |
| `func_8001EBE0(size, 0)` | text size (0x10 in the hi-res menus, 8 for smaller text) |
| `func_8001F324(r, g, b, a)` | text colour (the menus use yellow, 0xFF 0xFF 0, for the cursor's line) |
| `func_8001F1E8(x, y, fmt, ...)` | draws text, printf-style; `\n` starts a new line |
| `func_8001F5B0(n, 0, fmt, ...)` | the width text would take (the first argument is 0 or the text size in the code seen so far) |
| `func_8002D5AC(id)` | a text archive (0x14 title, 0x15 Options, 0x20 and 0x24 Rules, 0x24 and 0x25 the pick screen) |
| `func_8002D7C0(buf, size, archive, index)` | one of its strings (NULL buffer: an internal one) |

Text is Latin-1: `\xE9` is the é of "Pokémon".

### 2D (src/20470.h, src/1CF30.h)

| Function | Draws |
|---|---|
| `func_80020460(x, y, w, h, color)` | a window (colour as RGBA16; 0x2121 is the pick screen's blue) |
| `func_80020754(x, y, w, h)` | the Options screen's window |
| `func_80020928(x, y)` | the pointing-hand cursor (Rules uses `func_800207FC`, with its x moved by a sine to bob) |
| `func_8001C6AC(x, y, w, h, texture, texW, 0)` | an RGBA16 texture, between `gSPDisplayList(D_8006F518)` and `gSPDisplayList(D_8006F630)` |
| `func_8001C8C4(...)`, `func_8001CADC(...)` | the same for RGBA32 and IA8 (greyscale with alpha, a byte a pixel) textures |
| `gSPDisplayList(D_8006F4E0)` | copy mode instead, as the game draws its button icons: 16-bit textures only, no blending |
