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
- Battle Frontier, part 1 (waiting for T.'s test, test29): Rogue and Factory on the Stadium's
  cup screen, in the places of two cups the US version left out (their pictures for now:
  Factory Electrode and Jolteon, Rogue the other cup's background made small), the cups in
  two columns; "Choose your rules" (one tab: cup rules, random opponents, auto pick, both
  movesets, tradeback, DVs and Stat EXP set by the mode, legendaries, final evos, monotype),
  saved for each mode; a run that goes past the eighth trainer (round 1 again, the next
  ball in the Poke and Prime Cups, the lap mixed into the opponents' seed), ends on a loss
  and quits without a continue; the options' "Playstyle" row removed. The "Little Cup" in
  the plan was a mistake (it's Stadium 2's). Parity: 92160/92160.
- Battle Frontier, round 2 (test30): every cup screen panel the same size, with equal gaps;
  Rogue's picture brighter; picking Rogue or Factory zooms in as the cups do, then Battle,
  Rules and Registration; Rules lists the modes' own rules; Battle opens "Choose your
  rules" (no cup row any more: runs are the Poke Cup's, level 50, Dragonite 55), then the
  team screens with the game's portraits: Factory six in a row, one selected at a time,
  then "Rent these 3?" with the three's moves and stats; Rogue three teams of three, Left and
  Right between them, "Choose this team?". Teams of three, no registered Pokemon.
- Battle Frontier, round 3 (test31): level 100 (the Prime Cup's rule set and rentals);
  trainers not the cup's, battle after battle from the Prime Cup's balls, Round 1's then
  Round 2's, always with random teams (the "Random opponents" row is gone); Rogue's
  "Battles" row (15 to 50 in steps of 5), a boss every 5th (Gym Leaders, the Elite Four,
  the rival last) with a harder AI, "Rogue cleared!" at the end; the trainers up to the
  next boss under "Keep battling?" (Factory: the streak and the next one); the team
  screens' portraits on the victory screen's globes; windows that open as the game's do;
  Rules over the mode's own picture.
- Last test ROM sent: test31.

## Next: a 3D hub in place of the area select (T.'s prototype, first)

A prototype, not for release: the area select (`STATE_AREA_SELECT`, 0x10, `func_80029828`,
fragment37) as a view of the Stadium complex in 3D, seen from where one would stand in its
middle, at eye level. T.'s choices:

- **The view:** a fixed point in the centre, no walking. Left and Right turn the view to
  the next building around, as a ring, smoothly (about half a second); the building's name
  and a line about it show at the bottom. A goes in, handing over to the mode as the area
  select does now; coming back out faces the building left.
- **Every building:** Stadium, Gym Leader Castle, Pokemon Lab, Kids Club, Victory Palace,
  GB Tower, Free Battle and VS Mewtwo (with Mewtwo flying in the air).
- **The ROM grows to 64 MB** for the room (emulators and flashcarts read it; only about 9 KB
  are left now, CLAUDE.md).

Steps:
1. Research (a session of its own): whether the area select's complex is a 3D scene the
   camera can stand in (it was made to be seen from above, so at eye level the buildings
   may look flat or unfinished from behind), what else could stand in for the buildings
   (the 18 arenas in `stadium_models`), and how the area select hands over to each mode.
   Stills from the centre to T. before building anything.
2. The ROM at 64 MB: the linker scripts, the header's size if any, and the room checks
   (`randomizer.ld`'s `ASSERT`s).
3. The hub: a fragment of its own in place of the area select's screen: the scene, the
   camera turning between fixed headings, the caption, the doors.
4. A test ROM and a clip.

Step 1's findings (game-screens.md, the area select; game-engine.md, 3D):
- **The complex isn't 3D.** The area select is one 640x480 pre-rendered picture seen from
  above, with a 2D cursor; no model of the complex or of any building is in the ROM. A
  camera can't stand in it: the hub's buildings have to come from somewhere else.
- **The handover is simple.** fragment37 returns the next state (`D_82202FA8`), and every
  mode returns to `STATE_AREA_SELECT`; a hub that returns the same states needs nothing
  changed in the modes. Facing the building left needs one value kept outside the
  fragment (main code's memory, as the randomizer keeps others), since the area select
  starts on the Stadium every time. Free Battle and VS Mewtwo are places too (8 in all);
  VS Mewtwo opens late.
- **What could stand in**, since the camera only turns and never moves: a picture all
  round it (a panorama) looks exactly like 3D from a fixed point.
  - A: the arenas (`stadium_models`, 18): real 3D, but battlefields seen from their
    middle, not buildings; one could be the ground and sky.
  - B: an arena or a sky for the ground and sky, and each building a flat picture standing
    in a ring round the camera, cut from the area select's picture or drawn new.
  - C: a panorama made outside the game (art for the six buildings at eye level), as
    textures on a cylinder round the camera. The ROM at 64 MB has room for it.
  - D: no 3D: the area select's own picture, the view panning and zooming smoothly to each
    building in a ring, with the caption.
- **Stills** (test builds of the title's arena scene, the camera at the centre, 60 units
  up, turning 45 degrees at a time): arenas 0, 3, 7, 15, 16 and 17 all hold up at eye
  level with no gaps. Arena 0, the open field, is the likeliest ground and sky for a hub
  (B). The `backgrounds` archive has eye-level pictures of the Stadium's dome (3) and the
  Kids Club's gate (4), but none of the other buildings.
- **T.'s answers:** Free Battle (the open field) and VS Mewtwo are in the ring too, VS
  Mewtwo open for now, with Mewtwo flying in the air (a Pokemon model, as the title loads
  them, playing a flying or floating animation: to find which of Mewtwo's has one).
- **T. chose B:** arena 0 (the open field) for the ground and sky, and each building a
  flat picture standing in a ring round the camera, cut from the area select's picture
  (or 3 and 4) to start with, T.'s own art later.

Steps 2 to 4 done: `make RANDOMIZER=1 HUB=1` builds a 64 MB ROM with the hub in place of the
area select (mod-architecture.md, "The 3D hub"); every place goes in and comes back facing
itself, checked in mupen64plus (the Stadium, Kids Club, VS Mewtwo, B to the menus). T. had
the test ROM and a clip. Open:
- **The look:** the pictures are rectangles cut from the overhead map; T.'s own art (each
  building cut out, with a see-through background, eye level) would go in as textures of
  their own in the 64 MB ROM's room. The eye-level pictures in the archive (3, the Stadium's
  dome; 4, the Kids Club's gate) could stand in for those two meanwhile.
- Not tried yet: the GB Tower, the Pokemon Lab, Victory Palace, Free Battle and the Castle
  going in (the same handover); Glide64 (the default plugin) and real hardware or other
  emulators with a 64 MB ROM.
- Mewtwo plays animation 0, its idle; a flying animation, if it has one, is to find.

## Next: the Battle Frontier, parts 2 and 3 (T.'s plan)

T. on part 1's plan: the modes are on the cup screen (not the area select, which stays as
it is), Rogue top right and Factory bottom right; their rules in "Choose your rules", as
the README has them. Custom cup art for Rogue and Factory waits for a 2.0: T. liked a
mock-up made from the game's own 3D models (Charizard's Flamethrower against Gyarados
charging a Hyper Beam, in arena 15: the title scene filmed in test builds with the arena or
the Pokemon left out, cut out on magenta and composited in the cups' style), but it takes a
lot of hand work.

2. Factory, endless: six random rentals (made with the rules) offered, three kept; battles
   until a loss; after each win the swap with one of the trainer's (Factory's panel as
   now). Levels from the rule set; DVs and stat exp at their highest (done in part 1);
   the opponents' moves better every 7 wins. Final evos On: final evolutions; Off: first,
   second and single-stage Pokemon, no legendaries (done in part 1). Legendaries (when On)
   rare at first and more common in later rounds. The streak shown between battles, the
   best streak saved, and Save & quit keeping the run (`func_8002C538` saves only mode,
   ball, round and continues: the counter needs room of its own in the randomizer's save
   data, bank 2 from 0x3E48).
3. Rogue, 15 to 50 battles in steps of 5 (done in round 3, with the bosses: the 5th a Gym
   Leader, the 10th an Elite Four member, the last the rival for the shortest run; the
   Elite Four and the rival with the highest AI, the Gym Leaders the next): the team screen
   presents 3 preset teams of 3 (done in round 2). Rewards build the team up to 6, then a new one replaces one. With Final evos Off,
   first stages are common, second stages and single-stage Pokemon uncommon, final
   evolutions rare, and no legendaries at the start; DVs and stat exp build up over the
   run (part 1 has the rentals' and trainers' own, "Build up" in the window). Opponents a
   little stronger every battle; every 5th a boss (a Gym Leader or the Elite Four with
   teams of their type), the last the Champion; a full heal after each boss. HP, PP,
   status and fainted ones carried over between battles, and the battle goes on with 1 or
   2 when fewer can fight (the run ends when all have fainted). Four rewards after every
   win: New Pokemon, New move, Steal Pokemon (one of the beaten trainer's) and Heal Pokemon
   (full HP and PP, fainted ones back). A loss ends the run.

Found in the code (for the parts above):
- A run's state: `gRandomizerState.frontier` (kind, lap, restore bit); the state is full
  otherwise. The cup screen is `randomizer_stadium.c`, the rules `randomizer_menu.c` and
  `randomizer_panel.c`, the loop `randomizer_cup.c` (mod-architecture.md, "The Battle
  Frontier").
- The team screens are `randomizer_frontier.c`, run by the pick screen in a run's place
  (`RandomizerPickHooks.pick`); the team goes in through `Randomizer_FrontierEnter`.
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

### Part 1: open points

- Runs are level 100 with the Prime Cup's rules and rentals; the trainers are picked by
  `Randomizer_Foe` (randomizer_cup.c), not by the cup's ball and round.
- The lap counter is 5 bits (held at 31, after 248 trainers): the battle number is the lap
  times 8 plus the round. Part 2's streak needs a counter of its own, saved.
- The screen between battles still runs the cup's board of eight rounds after a win, before
  the line-up shows under "Keep battling?". The line-up doesn't show before the first
  battle (the run goes straight to the first trainer).
- The bosses' AI is the game's own profiles, not decoded (game-data.md, "AI profiles"): the
  Elite Four and the rival get "saikyou 6" (the Castle's last Rival's), the Gym Leaders their
  Round 2 Castle ones. Factory has no bosses. The bosses' teams are random like any
  trainer's: the plan's teams of their type (Brock's Rock and Ground...) are still to do.
- "Keep battling?" during a run still reads "Save and Quit", though quitting a run doesn't
  save it (part 2's Save & quit will).
- Legendaries "On" is as on the website for now (any of the five, as likely as the rest).
- Rules, from Battle/Rules/Registration during a run, lists the mode's own rules, but the
  bar at the top still reads "Check Pokemon Stadium Rules" (the left-out cup's name).
- Registration, from Battle/Rules/Registration during a run, is left as it is (the Prime
  Cup's). Later, maybe: registering the team a run ended with.
- The team screens' portraits: one model buffer only (two don't fit with the pick screen's
  own memory), so each portrait waits two frames after the last before its model loads
  (else it showed the next Pokemon). They come in one after another over the first second.
  The Rogue screen has 9 (80x120: 96x144 didn't fit), Factory's 6 (88x132). Memory is
  tight there (about 50 KB left with Rogue's): anything more on those screens needs room
  made first (mod-architecture.md, "The Battle Frontier", Memory).
- Rogue's picture is made from its background as the cup screen starts: the background
  (150 KB decoded) stays in memory with the screen. No problem seen in the emulator.
- In test builds, opponents put at level 2 (in `Randomizer_RandomizeTrainer`) froze the
  game as the battle started; `BATTLE_CAMERA_TEST_FAINT` is the way to win quickly (with a
  script that presses A and the C buttons in turn: A doesn't pick a move).
- In test runs with `RSP=mupen64plus-rsp-z64-hlevideo` (below), a battle stopped for good
  on a trainer's Ditto coming out after a faint (in the spotlight, from frame 4380), with the
  Custom camera and with the battle's own: the plugin, not the game, it seems (other
  send-outs went through).
- Emulator runs of a whole cup: battles hang with the default plugins and the screen after
  a win crashes mupen64plus-video-z64 (CLAUDE.md); `RSP=mupen64plus-rsp-z64-hlevideo` with the
  default video plugin does both, and runs faster.
- Parity needs the website's `claude/stadium-movesets` branch (its `main` has no
  `json/s1_rentals.json` or `json/gen1_move_overrides.json`).

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
