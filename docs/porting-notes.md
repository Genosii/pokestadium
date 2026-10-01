# Porting notes: a recompilation, and Stadium 2

## For a static recompilation

A recompilation (N64Recomp and the like) turns the ROM's MIPS code into native code
ahead of time. This ROM, and the randomizer on top of it, have a few things it has to
handle.

**Almost all the code is in relocatable overlays.** Screens are fragments, loaded
anywhere in the memory pool and relocated at load time (game-engine.md). A recompiler
needs each fragment's code with its relocations, so its references can be fixed up
wherever it lands, and a way to map any address in a fragment window (0x81000000 to
0x8FFFFFFF) to the loaded copy. Two sources for them:

- the fragments' own relocation tables in the ROM (format in game-engine.md), and
- the decomp's ELF, linked with `--emit-relocs`, which has the symbols and relocations
  of every fragment, the randomizer's included, and `build/pokestadium-us.map` for the
  sections.

The loader also accepts fragments compressed as `PERS-SZP` or `PRESJPEG`
(`func_80003DC4`); check each fragment for it.

**Fragments call each other.** A relocation into another fragment's window resolves to
wherever that fragment is loaded, so cross-overlay calls are ordinary `jal`s after
relocation; the recompiled code has to follow the same loaded-or-not state. A call into
a fragment that isn't loaded is left pointing at its link-time address; the game never
makes one.

**The randomizer adds indirect calls between overlays.** Its hooks are function
pointers (`jalr`), and fragment62's stubs tail-jump into another fragment (`jr $t9`
to a pointer read from 0x80000328, 0x8000032C, 0x80000350 or 0x80000354, else `j` to the
game's function). The recompiled code has to look up the target function by its
runtime address.

**Memory outside the game's own.** The randomizer keeps its state in `osAppNMIBuffer`
(0x8000031C, 64 bytes), which a console clears at power-on only; a recompilation that
offers Reset should keep it. It also reads the save-bank bookkeeping at 0x800AE4E8
directly.

**The save.** The randomizer's settings are in bytes of save bank 2 that no section
covers (game-data.md). A recompilation's save has to keep the whole 128 KB Flash image,
not only the sections the game checks.

### The randomizer as a recompilation mod

Recompilation projects usually let mods replace or wrap functions by name. The
randomizer is already split along those lines: every change to the game is a function
whose body is replaced or wrapped, and the new code is plain C in its own files. Its
patch points:

| Fragment | Function | What the randomizer does there |
|---|---|---|
| 17 | `func_86B01190` | the intro's setup: runs `randomizer_intro` (through `func_86B044B0`), which changes the scenes' Pokemon |
| 36 | `func_82100B98` | loads `randomizer_title` |
| 36 | `func_82100028` | empty in the game, called every frame: draws the subtitle |
| 36 | `func_82100054` | the unreachable debug destinations dropped, for room |
| 55 | `func_83002120` | loads `randomizer_menu` and `randomizer_rules` |
| 55 | `func_8300059C` | the Rules list's input, plus Z and the options panels |
| 55 | `func_830015EC` | the Rules list's text, plus the Z button and the panels |
| 56 | `func_82C014FC` | loads `randomizer_menu` and `randomizer_options` |
| 56 | `func_82C00658` | the Options window, with a "Randomizer" line |
| 56 | `func_82C0120C`, `func_82C012FC` | five lines instead of four; the new line opens the panels |
| 61 | `func_84203E6C` | loads `randomizer_core`, `randomizer_menu`, `randomizer_pick` |
| 61 | `func_8420AA08` | the rental list's input, plus Z (random team) and the C buttons |
| 61 | `func_84202718` | draws the options window, the team's moves and the hints at the end of each frame |
| 61 | `func_8420720C`, `func_842073A4`, `func_8420776C` | the teambuilder: "Edit Pokemon" in the team's menus, and the team's edit state |
| 61 | `func_8420F86C` | "Check registered Pokemon": Z edits a registered team |
| 61 | `func_84206A68` | the level-sum check, moved out unchanged for room |
| 61 | `func_8420B40C`, `func_8420C368` | the rental card's prompt and its input, with "Edit" |
| 62 | `func_84301430` | loads `randomizer_battleui` along with fragment31 (through `func_84340ACC`) |
| 62 | calls to `func_8431524C`, `func_84315550`, `func_843133B4`, `func_843135B8` | the party box and hint bars, through hooks |
| 63 | `func_84B03194` | loads `randomizer_cup` |
| 63 | `func_84B014DC` | draws Factory's swap panel |
| 63 | `func_84B022A0` | Factory's swap after a win, and the "Swap a Pokemon" line |
| 63 | `func_84B02654`, `func_84B02984` | Rogue: a loss ends the run |
| 64 | `func_84803368` | loads `randomizer_core`, `randomizer_battle`; random opponents |
| 64 | `func_848027F0` | Z and auto pick on the battle-select screen |
| 64 | `func_84800020` | the footer with the Z button |

A native mod wouldn't need the size tricks (moved bodies, the spliced table, the
fixed-address hooks); those only exist to fit into the original ROM.

## For Stadium 2

Stadium 2 very likely shares much of this engine (same developers, same hardware and
tools), but nothing here has been checked against its ROM yet. What to look for first, and what should carry over:

**Likely to carry over, once confirmed:**

- **Fragments.** If Stadium 2's overlays also start with `j entry` and `"FRAGMENT"` and
  carry a relocation table in the same format, the fragment tooling
  (`tools/fragment_relocs.py`, the regen lists), the "own fragments at the end of the
  ROM" layout and both hook patterns apply as they are.
- **`osAppNMIBuffer`** is libultra's, so it exists there too; check that Stadium 2
  doesn't use it.
- **The save**: check whether its banks also leave bytes outside every section, and
  whether they carry the same `'POKE'` and sum trailer.
- **The generator** (`randomizer_logic.c`) is plain C with no game dependencies, tested
  natively against the website. Porting it means Gen 2 data and rules, not new code
  structure.
- **The testing tools**: the headless emulator runner works for any N64 ROM; the parity
  checker for any generator that has a website counterpart.

**Won't carry over:**

- Every address and structure: the equivalents of `D_800AE540`, the Pokemon structure
  (Gen 2's box structure has a held item and a different layout), the trainer and rental
  archive, the move table, the save layout, and each screen's functions.
- Gen 1 specifics in the generator: 251 species, Steel and Dark, the Special split into
  Special Attack and Special Defense (stat exp stays one Special value), held items, DVs
  also deciding gender and shininess, and Stadium 2's own cups (Little Cup, Challenge
  Cup and so on).

A practical order: confirm the fragment format and find the pick screen and
battle-select screen equivalents; port the generator with Gen 2 data and get parity with
the website; then hook the screens one at a time, with the same checksum rule (keep the
checksummed range untouched so emulators still recognise the game).
