#!/usr/bin/env bash
# The camera editor's Pokemon models, all 151, from the player's own ROM (baseroms/us/):
#
#   tools/camera_editor/rip_all.sh OUT_DIR [FIRST [LAST]]   (FRAME=n saves at another frame)
#
# For each pair of species, builds the randomizer with the title screen's ARENA_TEST_SPECIES
# switch on that pair (src/fragments/36/randomizer_title_arena.c, put back afterwards), runs it in
# the headless emulator to frame 640 (the title's 3D scene just up, both in their idle stance before
# the first attack; always the first arena), saves
# a state there and takes both models out of it (rip_models.py). Two emulators at a time. Then
# packs OUT_DIR/models.json for the editor ("Load models", or next to it when published).
# The models are the game's: never commit them.
set -euo pipefail
cd "$(dirname "$0")/../.."
OUT=$(realpath -m "$1"); FIRST=${2:-1}; LAST=${3:-151}
mkdir -p "$OUT/work"
ARENA=src/fragments/36/randomizer_title_arena.c
cp "$ARENA" "$OUT/work/arena.c.orig"
trap 'cp "$OUT/work/arena.c.orig" "$ARENA"' EXIT
PY=${PYTHON:-.venv/bin/python3}

run_pair() { # a b rom
    local a=$1 b=$2 rom=$3 dir="$OUT/work/$1"
    mkdir -p "$dir"
    printf '%d save %s/state.st\n%d quit\n' "${FRAME:-640}" "$dir" "$((${FRAME:-640} + 5))" > "$dir/script.txt"
    GFX=mupen64plus-video-z64 RSP=mupen64plus-rsp-z64 TIMEOUT=600 \
        tools/headless_test/run.sh "$rom" "$dir/script.txt" "$dir/out" > "$dir/run.log" 2>&1 || true
    if [ -f "$dir/state.st" ]; then
        "$PY" tools/camera_editor/rip_models.py "$dir/state.st" "$a" "$b" "$OUT" >> "$OUT/work/rip.log" 2>&1 || echo "$a $b: rip failed" >> "$OUT/work/rip.log"
    else
        echo "$a $b: no savestate" >> "$OUT/work/rip.log"
    fi
    rm -rf "$rom" "$dir/out" "$dir/out.work"
    [ -n "${KEEP_STATES:-}" ] || rm -f "$dir/state.st"
}

for ((a = FIRST; a <= LAST; a += 2)); do
    b=$((a + 1)); [ "$b" -gt 151 ] && b=$((a - 1))
    cp "$OUT/work/arena.c.orig" "$ARENA"
    sed -i "0,/^#ifdef RANDOMIZER\$/s//#ifdef RANDOMIZER\n#define ARENA_TEST_SPECIES $a\n#define ARENA_TEST_SPECIES2 $b\n#define ARENA_TEST_ARENA 0/" "$ARENA"
    make RANDOMIZER=1 -j8 > "$OUT/work/build.log" 2>&1 || { echo "$a $b: build failed" >> "$OUT/work/rip.log"; continue; }
    cp build/pokestadium-us.z64 "$OUT/work/$a.z64"
    while [ "$(jobs -rp | wc -l)" -ge 2 ]; do wait -n; done
    run_pair "$a" "$b" "$OUT/work/$a.z64" &
done
wait
cp "$OUT/work/arena.c.orig" "$ARENA"

# One file for the editor: { "025": model, ... }
"$PY" - "$OUT" <<'EOF'
import json, sys
from pathlib import Path
out = Path(sys.argv[1])
models = {p.stem: json.loads(p.read_text()) for p in sorted(out.glob("[0-9][0-9][0-9].json"))}
(out / "models.json").write_text(json.dumps({"format": "stadium-camera-models", "models": models}, separators=(",", ":")))
print(f"{len(models)} models, {(out / 'models.json').stat().st_size // 1024} KB")
EOF
