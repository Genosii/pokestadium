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
- Last test ROM sent: test10.

## Open

- **Freeze when re-rolling several times in a row** (reported by T.). Not reproduced:
  five Z / "Reselect all" / Z rounds, Z spammed right after "Reselect all", B paths, and
  the team generator over 9 million seeds and settings on a PC (no hang). Waiting for their
  exact buttons, emulator and settings.
- Feedback wanted on the replay: whether the takes should be longer, and the angle of the
  second take (the defender is small over a big attacker's shoulder).

## Phase 3: camera between turns (research first; may not be possible)

T. asked for both; report back before building if one isn't feasible.

1. **Switching.** Today the Pokemon called back turns into a ball of light that rises off
   screen ("Enough! Come back!"), the camera cuts, and the new one comes out of its Poke
   Ball ("Go! ...!"), with a closing-circle transition between. Wanted: follow the light
   up, keep the angle, and pan to where the new one comes out, with no cut.
   Found so far: switching runs between turns (the director's mode 1, `unk_1C`). Its
   visibility rules (`func_8432A578`) hide the switching side in steps 16 to 19, and its
   camera presets are `func_8431AAFC`'s odd steps (`func_8431A718` to `func_8431AAAC`).
   Not found yet: which steps are the recall and the send-out, where the light is, and
   what draws the closing circle. The battle camera hook (`randomizer_battle_camera.c`)
   can film any mode, so once those are known it can take over.
2. **No cut to black between move selection and the battle.** A black jagged wipe plays
   between the menu phase and the attack phase (and at turn ends). Wanted: the camera
   moving from where it is to a view of both Pokemon instead. Not found yet: what starts
   the wipe. Ruled out: `func_8432E9D8` (clears effect particles), `D_8438E798` (the
   players' state), and the director's small helpers (`func_8431FF3C` and its
   neighbours). Next: the menu side (fragment31 and `randomizer_battle_ui`), and whether
   anything is loaded or rebuilt while the screen is black. If something is, the wipe
   probably has to stay.

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
