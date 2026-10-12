# The camera editor (design)

T.'s ask: "I need to see what I'm doing and select during which phases of the battle they
appear. Eventually save point A and point B, select easings, like I'm editing in After
Effects." Camera moves only (not the Pokemon's own animations). Agreed with T.:

- A clip is a camera move for one battle moment, made of keyframes.
- Keyframes are placed relative to the Pokemon, so a clip works for every species and both
  sides.
- Each clip is either "always" (it replaces the automatic shots for that moment) or "in the
  pool" with a weight (it joins the random shots), chosen per clip.
- Preview in two places: a 3D preview in the editor, and the real game in Project64 4.x,
  driven live by the editor. T. plays in Simple64, which has no scripting API; Project64
  4.x is for editing sessions.

Built in three parts, each shown to T. before the next.

## 1. Clips in the game (built)

The format is `src/fragments/62/randomizer_camera_clips.h` (the editor and the live link
read and write exactly that layout, big-endian, no pointers); the defaults are
`randomizer_camera_clips.c`; playback is in `randomizer_battle_camera.c`
(`Randomizer_BattleCameraClips` and the `Randomizer_Clip*` functions before it).

**The clip table** is a block of its own at the end of the block at 0x6CA730
(`randomizer_block.ld`, `randomizer_clips_ROM_START`; 0x6DD510 in the build that added it,
but find it by its tag), 0x2000 bytes whatever's in it, so writing clips into a ROM never
moves anything. It isn't part of a fragment: as a battle starts, the camera copies it into
its RAM (`sClipBuffer`, from `func_80003B30`) and writes that address at 0x8000035C (unused
low RAM, after `osAppNMIBuffer` and before the code at 0x80000400). A table without the tag,
the version or the clip size it expects has no clips.

- Header (0x10): `"CAMCLIP1"`, version (1), the number of clips, the size of a clip
  (0x108), and a byte for the live link to force a moment (`forceMoment`, unused yet).
- Up to 30 clips (0x108 each): the moment; the mode (off, always, pool); the weight
  (1-100); flags (1: the camera may cross the line and stand on it); the filters, for
  attack moments only (category: any, physical, special, status; a Gen 1 type id or 0xFF
  for any) and for any moment with a Pokemon of its own (side: any, the player's, the
  other's); the number of keyframes.
- Up to 8 keyframes (0x20 each): the frame (60ths of a second from the clip's start); the
  camera's anchor and, round its middle, a yaw from its front (tenths of a degree, turning
  toward the camera's side of the field for either side's Pokemon, so a clip films the
  other side as in a mirror; can go past 360), a pitch above it (tenths of a degree) and a
  distance (hundredths of its height, never under 90 units); what it looks at: an anchor,
  a point on it (feet, middle, head) and an offset along its facing, up and toward the
  camera's side (hundredths of its height); the roll (tenths of a degree, mirrored like the
  yaw) and the field of view (hundredths of a degree; the battle's own is 30); the easing on
  to the next keyframe (linear, in, out, in-out, hold, or its own Bezier curve: x1, y1, x2,
  y2 in thousandths, as CSS's `cubic-bezier()`).
- Anchors: the attacker, the defender, the moment's own Pokemon (the one switching or
  fainting; the attacker otherwise), halfway between the two, the middle of the field (on
  the ground). Between two keyframes round the same anchor the camera goes round it (the
  yaw, pitch and distance ease); between different anchors it goes straight across.

**Moments**, from the director's mode (`unk_1C`), script (`unk_38`) and step (`unk_20`):

| Moment | When it starts | Its Pokemon |
|---|---|---|
| menus | the menus up (mode 1, no wipe), where the battle's camera cuts to another shot | none |
| opening | script 1 | the first to attack (`unk_2A`) |
| wind-up | script 2 or 3, step 0, a move with power | the attacker |
| animation | step 1 | the attacker |
| hit | as the battle's camera turns from the attacker to the defender, or step 2 | the attacker |
| replay | a critical hit's or Hyper Beam's hit landing a second and third time (the replay) | the attacker |
| status | script 2 or 3 with a move without power, the whole of it | the attacker |
| recall | script 11 or 33, steps 0 to 2 | the one switching (`unk_2C`) |
| send-out | script 11 or 33 from step 3, script 18 (after a faint) | the one coming out |
| faint | a Pokemon's `unk_654.unk_2D` becoming 0x10 or 0x13 | the one fainting |
| turn end | script 4 | none |

**Playback.** As a moment starts, the clips for it are looked at: one of those always played,
or else one of the pool by weight against 100 for the automatic shots (none: the automatic
shots, as before). A clip plays from its first keyframe (a cut) to its last, then the camera
moves into whatever comes next in 24 frames (as after a filmed attack). It stops sooner when
the moment it's for is over; a faint's and the menus' clips play on until another moment
starts. The recall's clip holds the Poke Ball's throw until it has played (the battle's
105-frame wait otherwise); everything else keeps the battle's time. The camera rules stay on
top (one side of the line, 35 degrees off it between the two) unless the clip's flag lets it
cross. The Pokemon are both shown while a clip plays (unless fainted). The clips play with
Options' "Battle camera" on Custom only.

**Test switches** (`#define` near the top of `randomizer_battle_camera.c` or
`randomizer_camera_clips.c` in a copy, never committed):

- `BATTLE_CAMERA_SAMPLE_CLIPS`: four clips, always played: the hit (round the defender to
  square on), the send-out (low and far, pushing in to the head), the wind-up (over the
  attacker's shoulder at the defender) and a faint (pulling up and away on a Bezier curve).
- `BATTLE_CAMERA_TEST_FAINT`: the other side's Pokemon left with 1 HP in the menus, so
  the turn's first hit on it faints it and the next comes out (a faint and a send-out
  every turn).
- `BATTLE_CAMERA_TEST_BIG` (already there): every attack a big hit, so the replay plays.
- `BATTLE_CAMERA_TEST_LOG`: the hook's frame count, the clip playing (moment << 16 |
  frame) and the moment's key at 0x80000360, 0x80000364 and 0x80000368, for savestates.

## 2. The editor

A web page (`tools/camera_editor/`, also published so T. can open it without a build):

- Moments down the side; the clips for the selected one; a timeline with keyframes, and a
  graph of each value with its easing curves to drag (as in After Effects).
- A 3D preview: the arena and two Pokemon, the real models taken from the ROM if the model
  format can be read in time, otherwise stand-ins with each species' size. Pick which two
  Pokemon and which side attacks, to check a clip on a small one and a big one; scrub and
  play.
- Load the ROM; "Save ROM" writes the clip table into a copy (and fixes the ROM's
  checksum); "Export" saves the clips as a file for T. to send, to become the defaults in
  the code.

## 3. The live link (Project64 4.x)

A Project64 script (`tools/camera_editor/pj64_link.js`, put in Project64's Scripts folder):

- runs a small WebSocket server on localhost, which the editor connects to;
- writes the clip table into the running game's RAM as T. edits, so the next time the moment
  comes the game plays the new clip;
- sends back where the Pokemon and the camera are, so the editor's preview shows the real
  positions;
- can force a moment (the test switch's flag, set from the editor).

## Open questions for later

- The Pokemon models in the editor: worth reading the game's model format, or are sized
  stand-ins enough?
- Whether the existing automatic shots (the title's shots, the switch, the moves between
  turns) should become clips in the table, so all of them can be edited the same way.
