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
- Camera editor, part 1: the clip table (8 KB in the block at 0x6CA730, "CAMCLIP1", its RAM
  address at 0x8000035C), the battle's moments detected and clips played with their
  easings, and the test switches. `randomizer_battleui` moved into the block (the end of the
  ROM had run out). T. on test18's four sample clips: too many cuts and swings, worse than
  before; T. wants to fix them with the editor. So each clip now has its own blend in and
  blend out (format 2; 0 is a cut), and the ROM still comes with no clips.
- Camera editor, part 2 (waiting for T.'s test): `tools/camera_editor/index.html`, the
  editor, published privately for T. with the Pokemon's real models (`rip_all.sh`, never
  committed). test19 was the ROM to load in it (format 2, no clips); now test20 (format 3).
- Camera editor, part 2, after T. tried it: edits didn't show (paused on the first keyframe,
  the preview showed its blend in) and the camera was set by numbers. Now edits show as
  they're made, and the camera is moved as in Cinema 4D (Alt + drag orbit, pan, dolly;
  auto-key, F9 Record key; a top or front view to drag the camera and its target), stored
  round a Pokemon or fixed in the arena, a straight or orbiting path to the next keyframe
  (format 3, test20).
- T. set the camera editor aside (too clunky; its work since `6b129d7` is in a git stash,
  "Camera editor work set aside", uncommitted at T.'s word; test22 and test23 were built
  with it) and showed the menu shots wanted instead: through one's eyes, and close behind
  its head, at the other (test24), each one of the menu shots, either Pokemon's. More of
  T.'s shots to come.
- After test24 (T.): the camera into a turn reaches the first to attack and holds on it until
  its attack starts (it was turned away halfway and snapped to both as the wipe ended); the
  D-pad picks the menus' shot, to see each in the game (test25). A mode to view any two
  Pokemon on the field was talked about; the picker in a Free Battle with rentals does it
  for now.
- After test25 (T.): the POV draws what's next to the head (its hands), with a wider lens;
  from behind its head only on those under 60 tall; a Pokemon's middle leaves out its hands
  and wing tips (Zapdos swung the camera); Options' "Menu camera", "Attack camera" and "Hit
  camera" (test26). T. will note down the camera moves that still look odd.
- After test26 (T.): over the shoulder and behind its head still; the split screen centred,
  its right half's sky as the battle draws it (black in some arenas); "Overhead start to
  front" in place of the Game Boy view between turns; high above 2 seconds longer; from
  behind one up to the other no longer turned at its end; hidden Pokemon's animations kept
  going in the mod's views (frozen defenders and attackers); a big hit's replay holds the
  battle until done (test27).
- After test27 (T.): shots timed in real seconds; round the field, going round and the split
  screen 5 seconds; the chase still a second before and after; the split screen still, with
  a close variant; the fly-over from straight behind and higher, ending looking down; behind
  its head farther back, aimed where the other stands; no Game Boy view in attacks; Surf left
  to the battle; no black circle after a faint; the HP goes down after a big hit's first
  hit; Options' "Crit replay" (test28). A ban list per species is possible (T. will name
  any after playtesting; through its eyes clipped through Golbat).
- Last test ROM sent: test28.

## Next: the Battle Frontier (T.'s plan, after test28)

T. is happy with the camera angles for now. The camera editor is set aside for good (its
work stays in the git stash). Next, two modes of their own, built in three parts, each with
a test ROM (and a clip for parts 2 and 3):

1. A new spot on the main menu (fragment37, the area select), "Battle Frontier", opening a
   list: Battle Factory or Rogue. Its 196x64 picture made from the game's own art (T.
   reviews it in the test ROM), its caption, its neighbours, a new GameState and handler
   reusing the cup loop (`func_80029BC0`). The run's rule set chosen as it starts: Poke Cup
   (50-55), Prime Cup (100) or Little Cup (5). A run counter of the mod's own, mixed into
   the opponents' seed so teams don't repeat, with the round index looped back to 1 before
   it passes 8 (`func_8002BFC4` writes an 8-byte array by round). fragment37 isn't in
   `yamls/us/fragment_regen*.txt` yet: add it at a fixed size.
2. Battle Factory, endless: six random rentals (made with the options) offered, three
   kept; battles until a loss; after each win the swap with one of the trainer's (Factory's
   panel as now). Levels from the rule set; the opponents' moves, DVs and stat exp better
   every 7 wins. The streak shown between battles, the best streak saved, and Save & quit
   keeping the run (`func_8002C538` saves only mode, ball, round and continues: the
   counter needs room of its own in the randomizer's save data, bank 2 at 0x3E04).
3. Rogue, 10, 20, 30, 40 or 50 battles (chosen at the start): start with 3, rewards build
   the team up to 6, then a new one replaces one. Opponents a little stronger every battle;
   every 5th a boss (a Gym Leader or the Elite Four with teams of their type), the last the
   Champion; a full heal after each boss. HP, PP, status and fainted ones carried over
   between battles, and the battle goes on with 1 or 2 when fewer can fight (the run ends
   when all have fainted). Four rewards after every win: New Pokemon, New move, Steal
   Pokemon (one of the beaten trainer's) and Heal Pokemon (full HP and PP, fainted ones
   back). A loss ends the run.

Found in the code (for the parts above):
- Every battle starts at full health because the battle-select screen heals the registered
  team as it opens (`func_80022734` on each of `unk_214->unk_028`, fragment64.c's
  `func_84802F7C`); for Rogue call `func_80022338` (stats only) there instead.
- The battle works on `unk_01C` (the picked three, in pick order, then the rest) and never
  writes back to `unk_214->unk_028`. After a win it's still there in `afterWin`
  (randomizer_cup.c), so copy each one's HP, PP and status back; the pick order is gone by
  then (fragment64's `D_848037A0`), so keep it.
- The battle reads HP, status and PP from `unk_01C` at send-out (`func_84370B7C`), so it
  can start with less than full.
- `func_848025A0` (fragment64) wants three picks: change it for 1 or 2, and keep fainted
  ones out of the picks (`Randomizer_PickInput`, `Randomizer_BattlePick`) and the auto
  pick. The screen shows max HP (`unk_26`), not current.
- The beaten trainer's three are `D_800AE540.unk_1194[1].unk_08[0]->unk_01C` until
  `func_8002C128` loads the next trainer; `Randomizer_Swap` already steals one (fixing its
  original trainer and nickname), `Randomizer_Heal` heals.
- `gRandomizerState` is full (0x40 bytes).

## Open

- **Freeze when re-rolling several times in a row** (reported by T.). Not reproduced:
  five Z / "Reselect all" / Z rounds, Z spammed right after "Reselect all", B paths, and
  the team generator over 9 million seeds and settings on a PC (no hang). Waiting for their
  exact buttons, emulator and settings.
- The line rule, as T. meant it (after test20): the camera can be anywhere, but a new shot
  starts on the side the last one ended on (each shot decides once, at its start, whether
  it's mirrored). Checked in the emulator: clips made on the far side started on the near
  side after a near-side shot, a clip crossing inside itself is fine, and the 35 degree
  rule turns the camera off the line on the side it was on (it used to flip across it
  when the battle's own camera looked head on along the line). A clip that passes between
  the two now slides along the 35 degree edge; its "May stand on the line" flag lets it
  through.
- Feedback wanted: the quick moves (24 frames) and the move after picking (50 frames); the split screen's
  framing beside the menus' boxes.
- The title screen's camera doesn't follow the line rule (T. asked for the battle).

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
