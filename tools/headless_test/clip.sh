#!/usr/bin/env bash
# A video of a run's screenshots: clip.sh OUTDIR FIRST LAST out.mp4 [fps]
# Shots taken every 2 frames play at 30 fps in real time. Needs ffmpeg (FFMPEG overrides it;
# the imageio-ffmpeg Python package ships one).
set -eu
FFMPEG=${FFMPEG:-$(command -v ffmpeg || python3 -c 'import imageio_ffmpeg; print(imageio_ffmpeg.get_ffmpeg_exe())')}
d=$(mktemp -d); i=0
for f in $(ls "$1" | grep '\.png$' | sort); do
    n=$((10#${f:1:6}))
    if [ "$n" -ge "$2" ] && [ "$n" -le "$3" ]; then ln -s "$(readlink -f "$1/$f")" "$d/$(printf '%05d' $i).png"; i=$((i + 1)); fi
done
"$FFMPEG" -y -loglevel error -framerate "${5:-30}" -i "$d/%05d.png" -c:v libx264 -pix_fmt yuv420p -crf 20 -movflags +faststart "$4"
rm -rf "$d"; echo "$4: $i frames"
