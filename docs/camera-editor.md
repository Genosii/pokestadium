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

## 1. Clips in the game

**The clip table**, in `randomizer_battleui`'s data, starting with a magic tag
(`"CAMCLIP1"`) so the editor finds it in a ROM and the live link finds it in RAM (its address
also goes in a fixed word in low RAM, after the battle camera hook's 0x80000358). Fixed size
(say 8 KB), so patching a ROM never moves anything. A clip:

- `moment`: which battle moment it's for (below), and a filter (any move; physical, special
  or status; a move type; the player's side or the other's).
- `mode`: always, or in the pool with a weight 1-100.
- up to 8 keyframes, each:
  - `frame` (from the clip's start, 60 per second);
  - the camera's position, relative to an anchor: `anchor` (attacker, defender, the one
    switching, the middle between them, the field's centre), and from it a yaw (degrees
    round it, 0 = the way it faces), a pitch, and a distance in the anchor's heights;
  - what it looks at: an anchor and a point on it (feet, middle, head) plus an offset;
  - field of view and roll;
  - the easing into the next keyframe: linear, ease in, ease out, ease in-out, hold, or a
    cubic Bezier curve (two control points, as in After Effects' graph editor).
- The camera rules stay on top: one side of the line, never on it between the two (35
  degrees), unless a clip turns that off.

**Moments** (the battle director's mode, script and step; game-engine.md, "The battle's
camera"): the menus; the turn's opening; an attack's wind-up (the camera to the attacker),
its animation, the hit; a critical hit's replay takes; a status move; the recall (the light
rising) and the send-out; a faint; the turn's end. Each has a length: the battle's own
(the attack's animation, the 105-frame recall), or, where the battle waits on the camera
(like the switch's throw now), the clip's.

**Playback**, in the battle camera hook: when a moment starts, pick a clip ("always" first,
else the pool with the automatic shots by weight), then each frame work out the keyframes'
positions from the Pokemon's tracked points and ease between them.

**Test switch** (a build option, not in releases): force a moment every turn (always switch,
always crit, always faint), so a clip can be seen straight away.

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
