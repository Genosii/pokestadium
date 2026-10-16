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
- Last test ROM sent: test27.

## Next: the camera editor, part 3

T. makes clips in the editor and saves them into test20 to play in Simple64; their
feedback first. Then part 3, the live link to Project64 4.x (docs/camera-editor.md), and
the clips T. sends (Export) made the ROM's defaults (Copy C code).

## Camera editor: open points

- The preview's Pokemon don't move: in the game the camera follows where they are (an
  attacker lunging, a defender reeling), so a keyframe round a Pokemon that moves a lot in
  its attack frames it a little differently in the game. Fixed keyframes aren't affected.
- The editor's preview: the battle's own camera, which clips blend in from and out to, is a
  view of both from the side there (the game's comes from wherever the battle has it); the
  models are as the title drew them at frame 640 (one moment of their idle stance), so
  points that move with an animation (a head bobbing) are where they were then. Checked:
  the same clip in the editor and in the game frame for frame (Ekans' wind-up) matched.

- A faint's clip stops when the turn's end (script 4) starts, about 80 frames in (the
  closing circle after a faint covers it then anyway). T.: leave it as it is.
- Seen in the emulator: the wind-up, the hit, the faint and the send-out (after a faint)
  playing their clips; the opening, the status move, the menus and the turn's end
  detected (from the log). Not seen yet: the recall (needs the player to switch: the
  battle was won before the script's switch), the replay of a big hit, a clip in the
  menus, the filters.
- The sample faint clip ends looking at an empty spot: the battle has called the fainted
  one back by then. A sample only; T. will make their own.

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
