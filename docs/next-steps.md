# Next steps

Where the work stands, for the next session. Keep it current: update it as items are done
and remove what's finished.

## Done (on the branch)

- Phase 1 (`77fa727`): the rental card's "Randomize" (re-rolls that Pokemon with the
  player's options; Strong moves and top stats with the "Stadium" options), the card's
  four lines fitted in its window, and the options saved as the pick, Rules or Options
  screen ends instead of freezing as the window closes.
- Phase 2 (`c977b29`): battle camera fixes. Critical hits found again (the director's
  `unk_1A` 2 and the defender's `unk_5B` 1), the whole attack replayed twice with its
  effects, hidden Pokemon kept animating, the over-the-shoulder hit 30 frames earlier, no
  cut when the shot already ends on the defender, low shots higher and less tilted, the
  shot from above removed, tails left out of the framing.
- Last test ROM sent: test11 (the camera move in place of the black wipe).

## Open

- **Freeze when re-rolling several times in a row** (reported by T.). Not reproduced:
  five Z / "Reselect all" / Z rounds, Z spammed right after "Reselect all", B paths, and
  the team generator over 9 million seeds and settings on a PC (no hang). Waiting for their
  exact buttons, emulator and settings.
- Feedback wanted on the replay: whether the takes should be longer, and the angle of the
  second take (the defender is small over a big attacker's shoulder).

## Phase 3: camera between turns

1. **No black wipe** (done, waiting for T.'s test): the camera moves to both Pokemon and
   into the battle's camera instead (mod-architecture.md, "The battle camera"). The wipe
   stays on turns with a substitute, a Pokemon in the air or underground, or a pose the
   battle puts back; not tested in the emulator yet with a real Substitute, Fly or Dig
   (a test switch keeping every other wipe checked that the bands come back).
2. **Switching** (next). Wanted: follow the recalled Pokemon's light up, keep the angle,
   and pan to where the new one comes out, with no cut; and the two closing circles T.
   sees (after the light has gone, and around the new Pokemon once it's out) turned into
   camera moves too, unless they hide a model swap (then keep them).
   Found (game-engine.md, "Switching" and "The wipes"): the switch is move script 11
   (`func_84327DC0`); the cut is at the end of its step 2, after 105 frames, the camera
   set straight to the send-out view; the new model loads with the ball on screen. The
   closing circle is the camera's iris (`func_80012044`, `func_80011FC8`, the camera's
   `unk_CC`), which the director's `func_84329858` runs at the end of a turn with a
   switch or a faint. Not found yet: where the light's position is (effect 2, started
   by the recall action, its graphics loaded on demand), and where the first circle T.
   sees (before the send-out) comes from; measure both with savestates (the probe
   approach below).
   Probing tip: IDO drops static symbols, so for a test build make the camera file's
   statics global (`sed 's/^static //'`), read `nm` for their addresses, and find the
   fragment in RAM from `gRandomizerState.battleCameraHook` (0x80000358), which points
   at `Randomizer_BattleCamera`. Emulator runs aren't repeatable even with the same ROM,
   so take the savestates and the screenshots in one run, and keep scripts sorted by
   frame (`sort -n`): a line out of order stops the rest from running.

## Phase 4: Rogue rewards between rounds

T.'s design: after each won round, choose one:
- **A new Pokemon:** one of three random ones (the player's options, none from the
  opponent's team), replacing one of the player's six that they pick.
- **Change a move:** pick one of the player's Pokemon and the move to replace, then one
  of three random moves it can learn.
- Later, for Stadium 2: "Get a held item".

Starting points: `src/fragments/63/randomizer_cup.c`, whose panel between rounds already
lets Factory (and today Rogue) swap in one of the opponent's Pokemon (fragment63's
`func_84B022A0` and `func_84B014DC`). The generator is in `randomizer_core`
(`src/fragments/61/randomizer_logic.c`: `Randomizer_GenerateTeam`,
`Randomizer_RerollMon`; learnsets in `gRandomizerLearnsets`). Check what the cup screen
loads before using it there. Ask T. whether Rogue keeps the opponent swap as well.
