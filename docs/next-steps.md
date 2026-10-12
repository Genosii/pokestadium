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
- Phase 3, part 1 (`bc25380`): a camera move in place of the black wipe between turns.
- T.'s notes on test11, done (waiting for their test of test12): the camera on one side of
  the field for the whole battle; quick moves instead of cuts into and out of filmed
  attacks and where the battle's camera cuts as a turn goes on; after picking a move, the
  camera to the first to attack; idle stances kept going through the turn's resets;
  critical hits replay only the hit (the defender's reaction and the hit's effects); the
  title's big hits one turn in six (its Hyper Beam test made a third or half of the turns
  big hits); the title's shots in the menus, the split screen among them; "Team" in the
  Pokemon menu's box. The tilt on hits stays as it is (T.: three in a row by chance).
- Phase 3, part 2: the closing circle at the end of a turn with a switch is a camera move
  too (kept after a faint); switching, the camera looks up after the recalled Pokemon's
  light and moves to the Poke Ball thrown instead of cutting to it.
- Phase 4: Rogue's rewards after each win, a new Pokemon (one of three) or a new move (one
  of three), instead of the opponent swap, which Factory keeps. The cup screen loads
  `randomizer_core` for them, and `randomizer_cup` moved into the block at 0x6CA730 (the
  end of the ROM had run out).
- After test13 (T.'s notes): switching, the camera holds on the sky 0.3 seconds after the
  light, looks back down, the Poke Ball thrown only then, and stays at the side until the
  new one is out; the camera never stands between the two on the line (the battle's
  head-on shots and the moves through the middle of the field turned 35 degrees off it).
- After test14 (T.: the switch timing and the 35 degrees are right): the camera closes in
  on the new Pokemon as it comes out.
- After test15 (T.): the camera zooms in (its field of view) as it pans back down, keeping
  its angle, so the Poke Ball comes in from off the screen. (test16 swung it round to a
  side view instead, which T. hadn't asked for: keep the camera's angle in the switch.)
- Last test ROM sent: test17.

## Next: the camera editor

T. wants to make camera moves themselves, like in After Effects: keyframes placed relative
to the Pokemon, easings, clips chosen per battle moment ("always" or in the pool with a
weight), previewed in the editor and live in Project64 4.x. Design: `docs/camera-editor.md`.
Build its part 1 (the clip table and playback in the game, and the test switch) first and
show T. before part 2 (the editor) and part 3 (the live link).

## Open

- **Freeze when re-rolling several times in a row** (reported by T.). Not reproduced:
  five Z / "Reselect all" / Z rounds, Z spammed right after "Reselect all", B paths, and
  the team generator over 9 million seeds and settings on a PC (no hang). Waiting for their
  exact buttons, emulator and settings.
- Feedback wanted: the side the camera keeps flips the battle's own shots on the other
  side as in a mirror, and a sweep of its that crosses the line turns back at it; the
  quick moves (24 frames) and the move after picking (50 frames); the split screen's
  framing beside the menus' boxes.
- The title screen's camera doesn't keep to one side (T. asked for the battle).

## Phase 3 and 4: open points

- T. sees two closing circles in a switch: the one at the turn's end is now a camera move.
  The first ("after the light has gone") wasn't found: in the emulator here the switch
  shows a white flash as the ball opens, not a circle. Ask T. for a clip from their
  emulator.
- Switching the player's own Pokemon, the battle's camera can stand between the trainer and
  the spot: the Poke Ball then comes from behind the camera and flashes past it for a
  frame. Ask T.: leave it, raise the camera a little (same angle), or hide the ball until
  it's in front.
- Feedback wanted: the close-in on the new Pokemon (45 frames, to 2.6 times its height);
  and whether the circle's move should keep the circle's timing (it's quicker now).
- Rogue's new Pokemon get levels from the cup's rules, like a new team; in a cup with a
  level-sum rule a new one can make some trios too high, which the battle-select screen
  refuses as it does any team. Ask T. whether it should take the level of the one it
  replaces instead.
- "Get a held item" as a reward: for Stadium 2.
