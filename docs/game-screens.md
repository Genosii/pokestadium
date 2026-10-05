# Screen by screen

The screens the randomizer has touched or studied, with the functions that matter. Each
is a fragment (game-engine.md); "entry" is the function its header jumps to.

## Title screen: fragment36

Entry `func_82100C98`.

- **Background**: a full 320x240 picture from the `backgrounds` archive, index 0, or
  0x11 once section 20's round 1 record is 0x1F8 (game-data.md). It's drawn as 20x15
  tiles of 16x16 RGBA16 (`func_8210046C`). The "Pokemon Stadium" logo is part of the
  picture.
- **"PRESS START"** (`func_821000C4`): a 100x15 IA8 texture from `title_ui`, faded in and
  out, shown while a controller is plugged in; without one, a scrolling line of text
  (`func_821002F8`, text archive 0x14).
- **`func_82100028(0x64, 0x50)`** is empty and called every frame from the drawing
  (`func_821005EC`) with screen coordinates: a leftover hook, and the place to draw
  anything extra over the title.
- Textures loaded with `gDPLoadTextureBlock` (`func_8001C6AC`, `func_8001C8C4`) need rows
  a power of two long in 8-byte words, or the last pixels of every other row come out
  scrambled: the randomizer's subtitle is 160 pixels wide in rows of 256. RGBA32
  textures load 1024 pixels at a time at most, so wide ones go in strips of rows.
- `func_821009B4` is the loop: A or Start goes on (`func_82100054`, which also has
  debug destinations behind a flag that's never set); otherwise a demo starts after a
  while (game-engine.md).
- **Randomizer builds** draw a live 3D scene over the picture, from that hook
  (src/fragments/36/randomizer_title_arena.c): one of the 18 battle arenas and two of the
  151 Pokemon, picked at random each time the title starts, standing where the battle
  puts them and taking turns attacking with their battle animations and cries. The camera
  follows the fight, after Pokemon Battle Revolution's: between turns a shot of the field
  or of one of them (round the field, high above it, a split screen, going round one, low
  beside one, close on one's head, or from behind one up to the other); as a turn begins, the attacker (from the
  ground looking up, from above, pushing in on its front, from behind it at its target,
  or close on its head, following it as it moves); as the hit lands, the defender close and tilted, the camera shaking, reached by
  a cut or a quick swing. Every shot moves, and a new one comes every two or three
  seconds. The logo and the subtitle, in the top third of the screen, and "PRESS START" are drawn
  over it; the hook is
  called before them in randomizer builds. The logo is the game's own painted into the
  picture, so the randomizer draws one of its own (mod-architecture.md, "Room in the
  ROM"). The demo battles pick from all 151 (game-engine.md).

## The intro: fragment17, stage 0x12

Entry `func_86B00020` picks the stage by mode; the intro is `func_86B01190`
(src/fragments/17/fragment17_15FA60.c). It's rendered live, not a video. Five scenes,
each a table of Pokemon (`D_86B0C4C8[scene]`, src/fragments/17/fragment17_161E60.c) of
`unk_D_86B0C4C8`: position (`Vec3f`), species at 0x0C, animation at 0x10, the list
ending with species 0x98:

| Scene | Table | Pokemon | Setting |
|---|---|---|---|
| 0 | `D_86B0C2F0` | Blastoise, Rhydon, Kangaskhan, Machoke | ground, the camera circling |
| 1 | `D_86B0C354` | Nidoking, Sandslash, Scyther, Kabutops | ground |
| 2 | `D_86B0C3B8` | Pidgeot, Fearow, Aerodactyl, Charizard | sky: they fly at the camera |
| 3 | `D_86B0C41C` | Seaking, Dewgong, Goldeen, Tentacruel | water |
| 4 | `D_86B0C480` | model 0xD7 | the closing scene with Pikachu, Psyduck and Clefairy, one model with its own animation |

- `func_86B003CC` / `func_86B00470` load a scene: for each entry, the model from the
  archive in `D_800ABE10.unk_A04` (file `species - 1`, so 0xD7 is one of the special
  models after the 151 Pokemon; `func_80019D18`), then animation `unk_10`
  (`func_8001BD04`). Every Pokemon plays its animation 0, its idle loop in battle,
  except Charizard, which plays 10, apparently its flying one; species 6 also gets
  `unk_0A6 = 0` (`func_86B0027C`).
- The Pokemon don't walk or fly by themselves: scenes 2 and 3 move them 20 units a frame
  towards the camera (`func_86B00848`) until they're all past it; scenes 0 and 1 move
  the camera (`D_86B0C160`, `func_86B01AAC`). `D_86B0C264` holds each scene's fade
  colour and speed.
- `func_86B00C34` is the scene state machine; A, B or Start fade out and end it.
- The randomizer rewrites the species in scenes 0 to 3 as the intro starts
  (src/fragments/17/randomizer_intro.c), from pools checked in an emulator: Pokemon of
  about the originals' size on the ground (not Muk or Snorlax, which fill the screen as
  the camera passes), Pokemon whose idle animation flies in the sky, and ones whose idle
  animation swims in the water. Gastly, Haunter and Koffing float rather than fly;
  Articuno and Moltres stand with their wings spread in all seven of their animations
  (0 to 6; an animation a model doesn't have hangs the intro), so they're on the ground
  with Vaporeon, which sits. Four Moltres in one scene don't fit in memory either.

## Options: fragment56

Entry `func_82C014FC`, loop `func_82C012FC`, 640x480.

- Four lines: Sound, Voice, Delete saved data, Quit. `D_82C01664` is the cursor,
  `D_82C01666` the flags being edited (bit 0 sound, bit 1 voice), `D_82C01660` the text
  archive (0x15).
- `func_82C00658` draws the window (its size and the four lines are constants),
  `func_82C0120C` acts on A, `func_82C0115C` saves the flags (section 20),
  `func_82C01054` deletes the saved data, `func_82C00E10` and `func_82C00F88` are the
  confirmation windows, `func_82C00D98` draws one frame.

## Rules: fragment55

Entry `func_83002120`, 640x480, reached from the cups and the Castle ("Rules") and
Event Battle.

- `func_83001CF8` sets it up: `func_83000160` returns the list of rule ids for the mode
  (-1 terminated) into `D_83003CA0`, counted into `D_83003CA4`; `D_83003CA6` is the
  cursor. Rule `id`'s bullet text is `D_83003CE0.unk_00[id]`, its details
  `D_83003DE0.unk_00[id]` (NULL for the rule that opens the rental Pokemon viewer,
  `func_830038DC`).
- `D_83003C80` is the screen's state (0 opening, 1 the list, 2 a details window, 3
  closing, 4 done), run by `func_83002030`; `func_8300059C` is the list's input, and A
  opens a rule's details with `func_830025F8(id)`.
- Drawing: `func_83001A9C` per frame; `func_830017C0` the list window and bullets,
  `func_830015EC` its text, 0x18 pixels a line from `window y + 0x26` (the window fits
  11); `func_8300243C` the details window.
- The background is only drawn while `D_83003C90` is non-zero (-1 always, else a
  count of frames); set it to 2 to redraw everything once in both framebuffers.

## The Pokemon pick screen: fragment61

Entry `func_84203E6C`, 640x480: the rental list and the team being entered.

- `func_84202718` ends every frame's drawing; `D_84210D40` is how many more frames it
  redraws everything (set 2 after covering something).
- `func_8420AA08` is the rental list's input (`unk_D_842168A0`: `unk_0013C` the rentals,
  `unk_13608` the team panel). `func_84209DB8` marks a rental as picked,
  `func_84207BD4` runs the team panel (`unk_0001 == 3` while it waits for a pick),
  `func_84206A68` checks the cup's level-sum rule and `func_8420ACA8` its levels.
- `func_84203C90` loads the rule set's rental file (game-data.md).
- **The rental card** (`D_8423D3A8`, `unk_D_8423D3A8`): A on a rental opens it
  (`func_8420A288` calls `func_8420C60C` with a mode: 1 "Enter" in the cups, 3 "Use this
  Pokemon?" elsewhere; the team panel opens it with 2, "Exchange"), and the list waits in
  state 11. `func_8420C580` runs it (`unk_00`: 2 opening, 1 open, 3 closing, 0 closed),
  `func_8420C368` is its input and `func_8420B40C` draws its prompt, `unk_02` the answer
  (0 Yes, 1 No). `func_8420C788` returns the answer plus one once it's closed, and
  `func_8420A0E4` then adds the Pokemon with `func_84207BD4`, into the slot under the team
  panel's cursor (`unk_0010 + unk_0012 * 3`).
- The team panel's state is `unk_D_84211B50.unk_0001`, run by `func_8420776C`: 7 is the
  menu once six are picked (OK, OK to Register, Reselect some, Reselect all;
  `func_8420720C`), 14 Registration's (OK, Reselect some, Reselect all; `func_842073A4`),
  15 the level-sum message (`func_84207530`). `func_84206990` registers the team.
- **Menus** (`func_8420DA28(id, controller)` opens one, `func_8420DB48(id)` returns the
  line picked plus one, 0 for B, once it has closed): `D_84211704[id]` holds each one's
  x, y, width, height, number of lines (`unk_0A`), default line, colour and the sound
  each line makes (`unk_0E`, 3 bits a line); its lines are drawn by a function per menu
  (`func_8420D4F8`, e.g. `func_8420C844` for menu 0), 0x1C pixels apart
  (`func_8420C7B0`). `D_84211700` is the open menu's state (1 opening, 2 open, 3 closing,
  4 closed with a result), `D_8423E580` which, `D_8423E58A` the cursor.
- **Registration** is this screen too: fragment54's Registration runs fragment61 with
  argument 1 (`func_84203BBC`), with its own menu (Register, Check registered, Delete
  registered, Quit; `func_842023E4`). Checking and deleting go through the registered
  teams' viewer, `D_84229EB0` (`func_8421089C` sets it up in mode 1 or 2,
  `func_842106FC` runs it); `func_8420F86C` is its input on the teams, with
  `func_8420F1E0` giving the highlighted one, and `func_8420F204` reads a team from the
  save. (fragment57, which rom.yaml calls the registration code, is the "Please Select"
  menu after the title screen.)

## The battle-select screen: fragment64

Entry `func_84803368`: each player picks the three that battle.

- `func_848027F0` is a player's picking, `func_84800020` draws the footer
  (`* [L] Button to cancel`).
- The computer trainers' teams were loaded by `func_8002C128` (main code) for modes 1
  to 8; battles start from the copy in `unk_214` (`func_8002B888`).

## Battles: fragment62

The battle system; menus at 320x240. Its relocation table can't be rebuilt byte for
byte from the decomp yet, so it's changed only at fixed sizes (mod-architecture.md).

- `func_84301430` sets the battle up (it loads fragment31); `func_84340ACC` is never
  called.
- The menus: `unk_654.unk_10` is 1 in the fight menu, 2 in the Pokemon menu;
  `unk_654.unk_1C` is set while R is held, and only the drawing reads it
  (`func_843172A0`, `func_84317558`).
- `func_843133B4` draws the "L Cancel / R Check" bar, `func_843135B8` the "R Check" bar
  when a switch is forced, `func_84313A74` the moves list, `func_8431524C` the party box
  shown with R, `func_84314F60` a line of it.
- The player at the top of the screen has the menus at (0x60, 0x0F); at the bottom, the
  bars at (0x62, 0xCE) and the moves at (0x19, 0xA6).

## Between a cup's battles: fragment63

Entry `func_84B03194`.

- `func_84B022A0` shows the "Keep battling?" menu after a win (menu id 1 in the cups,
  2 in the Castle); `func_84B02654` and `func_84B02984` are the cups' and the Castle's
  win or loss branches.
- `func_84B014DC` ends every frame's drawing, `func_84B01AA0` draws the menu.
- Menus are described by `D_84B17550` (x, y, width, height, number of lines at
  `unk_08`, colour, ...); the open menu's state is `D_84B26640.unk_1C` (`unk_00` open,
  `unk_01` which, `unk_02` its opening animation, 4 or more when open, `unk_04` the
  cursor).
- `D_800AE540.unk_11F6 |= 1` makes the screen quit, ending the run.
- `func_84B01994`: after the Prime Cup's Master Ball in Round 2, a Pikachu on the team
  that knows Surf and came from a Game Boy game unlocks Surfing Pikachu (the randomizer
  moved it, unchanged, into its own fragment to make room).
