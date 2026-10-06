#!/usr/bin/env bash
# Run a ROM headlessly in mupen64plus with scripted controller input.
#
#   tools/headless_test/run.sh ROM SCRIPT OUTDIR
#
# Screenshots from the script's "shot" commands land in OUTDIR as fNNNNNN.png
# (NNNNNN = frame), the emulator log in OUTDIR.log. Every run starts from an
# empty save file so results don't depend on earlier runs.
#
# Environment: GFX / RSP pick the video and RSP plugins, TIMEOUT caps the run
# in seconds (default 600), MUPEN64PLUS overrides the emulator binary, SEED_SAVE
# and KEEP_SAVE start from and keep a save (below). For the battle and title
# scenes use the LLE plugins: GFX=mupen64plus-video-z64 RSP=mupen64plus-rsp-z64.
set -u

if [ $# -ne 3 ]; then
    echo "usage: $0 ROM SCRIPT OUTDIR" >&2
    exit 2
fi

ROM=$(readlink -f "$1")
SCRIPT=$(readlink -f "$2")
OUT=$(readlink -m "$3")
HERE=$(dirname "$(readlink -f "$0")")
PLUGIN="$HERE/mupen64plus-input-script.so"
# Debian/Ubuntu install the emulator to /usr/games, which isn't always on PATH
MUPEN64PLUS=${MUPEN64PLUS:-$(command -v mupen64plus || echo /usr/games/mupen64plus)}

if [ ! -f "$PLUGIN" ]; then
    make -s -C "$HERE" || exit 1
fi

rm -rf "$OUT" "$OUT.work"
mkdir -p "$OUT" "$OUT.work/config" "$OUT.work/save"
# SEED_SAVE=dir starts from the save files in dir (one KEEP_SAVE kept) instead of an empty one
[ -n "${SEED_SAVE:-}" ] && cp -r "$SEED_SAVE"/. "$OUT.work/save/"

# Glide64mk2 draws this game correctly under software OpenGL; Rice garbles the
# menus. Its screenshots come out black there, so the plugin captures the X
# display instead (M64_SHOT_DIR), with the screen sized to the game window.
M64_INPUT_SCRIPT="$SCRIPT" M64_SHOT_DIR="$OUT" SDL_AUDIODRIVER=dummy \
timeout -k 5 "${TIMEOUT:-600}" xvfb-run -a -s "-screen 0 640x480x24" \
    "$MUPEN64PLUS" --configdir "$OUT.work/config" --datadir /usr/share/games/mupen64plus \
        --gfx "${GFX:-mupen64plus-video-glide64mk2}" --rsp "${RSP:-mupen64plus-rsp-hle}" \
        --resolution 640x480 --set "Video-General[ScreenWidth]=640" --set "Video-General[ScreenHeight]=480" \
        --audio dummy --input "$PLUGIN" --emumode 2 --nosaveoptions \
        --set "Core[SaveSRAMPath]=$OUT.work/save" --set "Core[SaveStatePath]=$OUT.work/save" \
        "$ROM" > "$OUT.log" 2>&1
status=$?

grep -E "^input-script:" "$OUT.log"
# KEEP_SAVE=1 keeps the run's save files in OUTDIR.save. mupen64plus names them by the ROM's
# MD5, so only the same build finds them again.
if [ -n "${KEEP_SAVE:-}" ]; then rm -rf "$OUT.save"; cp -r "$OUT.work/save" "$OUT.save"; fi
rm -rf "$OUT.work"
exit $status
