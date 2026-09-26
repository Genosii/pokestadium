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
