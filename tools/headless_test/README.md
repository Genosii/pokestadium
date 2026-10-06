# Headless test runs

Runs a ROM in [mupen64plus](https://mupen64plus.org/) without a screen, pressing
buttons from a script and saving screenshots, so a build can be checked (for
example, that it still reaches the pick screen) without playing it by hand.
Linux only.

## Setup

```bash
sudo apt install mupen64plus-ui-console mupen64plus-video-glide64mk2 \
    mupen64plus-rsp-hle libmupen64plus-dev xvfb imagemagick
```

`run.sh` builds the input plugin (`input_script.c`) on first use.

## Usage

```bash
tools/headless_test/run.sh build/pokestadium-us.z64 tools/headless_test/scripts/pick_screen.txt /tmp/pick
```

Screenshots go to `/tmp/pick/fNNNNNN.png`, where `NNNNNN` is the frame they were
taken on. Every run starts from an empty save file.

## Scripts

One command per line, keyed by frame number (60 per second); `#` starts a comment.

| Command | Effect |
|---|---|
| `<frame> press <buttons> [frames]` | Hold buttons for `frames` frames (default 4) |
| `<frame> shot` | Take a screenshot |
| `<frame> save <path>` | Write a savestate |
| `<frame> quit` | Stop the emulator |

Buttons are joined with `+`: `A B Z START L R DU DD DL DR CU CD CL CR`
(`D` = D-pad, `C` = C buttons).

Inputs are replayed by frame number, so a script only works for a ROM whose
menus take the same time to appear. Take screenshots while writing a new one.

## Battles, the title screen and helpers

- The battle and the title's 3D scenes need the low-level plugins:
  `GFX=mupen64plus-video-z64 RSP=mupen64plus-rsp-z64 TIMEOUT=3000 run.sh ...` (a battle
  runs at about 10 frames a second of wall time; run at most two at once).
- `SEED_SAVE=dir` starts from the save files in `dir`, `KEEP_SAVE=1` keeps the run's in
  `OUTDIR.save`. mupen64plus names them by the ROM's MD5, so a save only carries over
  between runs of the same build (`scripts/options_camera.txt` then a battle, say).
- `scripts/battle.txt` (into the Poke Cup's first battle), `scripts/rental_card.txt`
  (the rental card's Randomize), `scripts/options_camera.txt` (Battle camera: Original).
  Teams and the battle's own random numbers change from build to build, so a run shows
  whatever battle it gets; force what you need with a test switch in the code instead.
- `sheet.py OUTDIR out.jpg [columns] [every] [first] [last] [width]`: a contact sheet of
  the screenshots, labelled with their frames. Keep it small and look closer only where
  something's off.
- `clip.sh OUTDIR FIRST LAST out.mp4`: a video of the screenshots (shots every 2 frames
  play in real time at 30 fps).
- `savestate.py battle STATE`: the battle director's state and both Pokemon's models
  from a savestate (`<frame> save <path>`); `load()` and the readers for anything else.
