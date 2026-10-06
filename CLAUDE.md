# Pokémon Stadium Custom

An in-game randomizer and ROM hack built on this fork of the pret/pokestadium decomp
(US). Its teams must match the website's for the same seed and settings
(Genosii/pokemon-stadium-random-team-generator). The README says what it does; `docs/`
says how.

## Working with T. (Genosii)

- Read the relevant files before every task. Show a plan and ask questions before
  changing anything. Never delete or send anything without approval.
- T. tests the ROMs and makes videos about Pokémon: send a short clip and a test
  ROM with each finished change.
- Current work and open questions: `docs/next-steps.md`.

## Git

- Branch `claude/pokemon-stadium-randomizer-mljpf2`; push with
  `git push -u origin claude/pokemon-stadium-randomizer-mljpf2`. No pull requests unless
  asked.
- Commit messages say what changed and why, in plain prose. End them with the attribution
  lines the session gives. No model names or IDs anywhere in the repo.
- Never commit a ROM (`baseroms/`, `build/`), and never commit a test-only hack (see below).

## Build and checks (all before every commit)

```bash
make -j8                 # plain build: must stay byte for byte the original game
md5sum build/pokestadium-us.z64          # ed1378bc12115f71209a77844965ba50
make RANDOMIZER=1 -j8    # the hack (both share build/; rebuild after switching)
python3 tools/fragment_relocs.py check build/pokestadium-us.elf \
    yamls/us/fragment_regen.txt yamls/us/fragment_regen_randomizer.txt   # silent = OK
cmp -n $((0x101000)) build/pokestadium-us.z64 baseroms/us/baserom.z64     # first MB untouched
```

If the team generator changes (`src/fragments/61/randomizer_logic.c`), check parity:
`tools/randomizer/parity/check.py PATH_TO_WEBSITE_REPO` (needs node and the website repo).

## How the code is laid out

- Randomizer code is under `#ifdef RANDOMIZER`, mostly in fragments of its own
  (`linker_scripts/us/randomizer.ld`, `randomizer_block.ld`), hooked into the game's
  screens: `docs/mod-architecture.md`. Engine findings: `docs/game-engine.md`; screens:
  `docs/game-screens.md`; save and data: `docs/game-data.md`; every game function changed:
  `docs/porting-notes.md`. Update the docs with each change.
- Match the surrounding code: names, comment density, and comments that explain what
  the game does and why the code does what it does.
- Fragments the hack changes keep fixed sizes (`yamls/us/fragment_regen*.txt`).
  fragment62 (the battle) is spliced: its functions keep their sizes, and
  `src/fragments/62/randomizer_battle_ui_stub.s` stays 0x1E4 bytes (a Makefile check).
- ROM space: about 70 KB free in the block after the title logo, about 14 KB at the end.

## Pitfalls

- The Makefile runs bash with `pipefail`: use `grep ... > /dev/null`, not `grep -q`.
- asm-processor doesn't take `.if`.
- Battles aren't deterministic from build to build (the game seeds from the CPU's
  counter): teams, moves and crits change. Force what a test needs with a test switch
  (`#ifdef` near the top of the file, e.g. `BATTLE_CAMERA_TEST_ALWAYS`/`_BIG`/`_SHOT`,
  `ARENA_TEST_*`), build a copy of the ROM, and put the file back before committing.

## Testing in the emulator

`tools/headless_test/` (its README): scripted input, screenshots by frame, savestates.
Battle and title scenes need `GFX=mupen64plus-video-z64 RSP=mupen64plus-rsp-z64`. Run at
most two emulators at once; stop one by its PID, never with `pkill -f`. Keep test output
in the session's scratchpad.

## Keeping sessions cheap

Every tool call re-reads the whole conversation, so:
- keep command output short (`head`, `tail`, `grep -c`), and read only the part of a file
  you need;
- keep images few and small: contact sheets of small thumbnails (`sheet.py`, width
  120–160), full frames only where something looks wrong;
- wait for an emulator run with one blocking or background command, not repeated checks;
- one task per session; write what's left to `docs/next-steps.md`.

## Deliverables

- ROM: `zip -9` the `.z64` (about 27.7 MB, under the 30 MB upload limit), one per file.
- Clips: `tools/headless_test/clip.sh` from a run with a shot every 2 frames.
