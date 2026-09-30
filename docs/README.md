# Documentation

Notes on how Pokemon Stadium (US 1.0) works inside, gathered while building Pokemon Stadium
Custom (the in-game randomizer and everything since) on top of this decomp, and on how it's
put together. The code and these notes call it the randomizer, the name it started with. They
are written to be reused: for a static recompilation of the game, and as a starting point
for doing the same things in Pokemon Stadium 2.

| File | What's in it |
|---|---|
| [game-engine.md](game-engine.md) | Boot and the game's state machine, the memory pool, fragments (code overlays) and how they're relocated, the reset-proof `osAppNMIBuffer`, frames, input, text and 2D drawing helpers |
| [game-data.md](game-data.md) | The save file (banks, sections, checksums, unused space), the Pokemon structures in memory and in the save, the trainer and rental archive, the move table |
| [game-screens.md](game-screens.md) | Screen by screen: the title screen, the intro, Options, Rules, the Pokemon pick screen, the battle-select screen, battles and the screen between a cup's battles, with the functions that matter in each |
| [mod-architecture.md](mod-architecture.md) | How the randomizer is built: its own fragments at the end of the ROM, how each screen is hooked, why the ROM's checksum stays the original's, settings and saving, the teambuilder, the team generator and its parity with the website, keeping it within the room in the ROM, testing |
| [porting-notes.md](porting-notes.md) | What a recompilation has to know about this ROM and the mod, and what should carry over to Stadium 2 and what won't |

## Conventions

- Addresses are for the US 1.0 ROM (`md5: ed1378bc12115f71209a77844965ba50`). RAM
  addresses in fragments are their link-time addresses (their "VRAM", `0x81000000`
  and up); the game moves fragments when it loads them (see game-engine.md).
- Function and variable names are the decomp's placeholders (`func_8001F1E8`,
  `D_800AE540`, `unk_24`). A description here is what the code was seen to do, not an
  official name. Where something is inferred rather than confirmed, it says so.
- "Mode" means the value in `D_800AE540.unk_0000` and "rule set" the one in
  `D_800AE540.unk_0001` (see game-engine.md), not the randomizer's Normal, Factory
  and Rogue modes, which are called the randomizer's modes.
