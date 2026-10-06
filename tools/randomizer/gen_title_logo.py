#!/usr/bin/env python3
"""
Make the logo the randomizer's title screen draws over its 3D scene
(src/fragments/36/randomizer_title_logo.c), for the ROM block it goes in:

    .venv/bin/python3 tools/randomizer/gen_title_logo.py OUT.bin --size BYTES \\
        --logo ARTWORK.png --background assets/us/backgrounds/0.jpeg

The logo is the artwork in --logo if that file exists: any size, with transparency. The
artwork is kept out of the repository (assets/ is ignored), so without it the logo is cut
out of the game's own title picture (--background, extracted from the ROM): the "Pokemon
Stadium" letters with their outline. Either way it's scaled to LOGO_H pixels high (the title
screen is 320x240) and centred at the top, so that with the subtitle under it
(randomizer_title.c) it takes the top third of the screen.

The ROM gets a 256-colour texture (CI8, a palette of RGBA16 colours, transparent or not),
Yay0-compressed, after a 16-byte header:

    0x00  "LOGO"
    0x04  u16 x, u16 y       where it's drawn
    0x08  u16 width, height  the width a multiple of 8, for texture loads
    0x0C  u32 size           of the compressed data that follows

and the compressed data holds the palette (256 big-endian u16, colour 0 transparent), then
the colour of every pixel, row after row. OUT.bin is padded to 16 bytes; it has to fit in
--size, the size of the block it goes in, where the randomizer's code follows it
(linker_scripts/us/randomizer_block.ld, which ends where the block did).

Needs Pillow and crunch64 (both in requirements.txt).
"""

import argparse
import os
import struct
import sys
from collections import deque

import crunch64
from PIL import Image

SCREEN_W = 320
LOGO_H = 58
LOGO_Y = 3
COLOURS = 255  # and colour 0, transparent

# Where the logo is in the title picture
CUT_X0, CUT_Y0, CUT_X1, CUT_Y1 = 50, 0, 272, 112


def place(logo):
    """The logo trimmed to what isn't transparent, scaled to LOGO_H high, and where it goes"""
    logo = logo.crop(logo.getchannel("A").getbbox())
    width = round(LOGO_H * logo.width / logo.height)
    return logo.resize((width, LOGO_H), Image.LANCZOS), (SCREEN_W - width) // 2, LOGO_Y


def from_artwork(path):
    return place(Image.open(path).convert("RGBA"))


def neighbours(y, x, h, w):
    for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        ny, nx = y + dy, x + dx
        if 0 <= ny < h and 0 <= nx < w:
            yield ny, nx


def dilate(mask, times):
    h, w = len(mask), len(mask[0])
    for _ in range(times):
        grown = [row[:] for row in mask]
        for y in range(h):
            for x in range(w):
                if mask[y][x]:
                    for ny, nx in neighbours(y, x, h, w):
                        grown[ny][nx] = True
        mask = grown
    return mask


def invert(mask):
    return [[not v for v in row] for row in mask]


def pieces(mask):
    """The 4-connected pieces of a mask, each a list of (y, x)"""
    h, w = len(mask), len(mask[0])
    seen = [[False] * w for _ in range(h)]
    for sy in range(h):
        for sx in range(w):
            if mask[sy][sx] and not seen[sy][sx]:
                piece = []
                queue = deque([(sy, sx)])
                seen[sy][sx] = True
                while queue:
                    y, x = queue.popleft()
                    piece.append((y, x))
                    for ny, nx in neighbours(y, x, h, w):
                        if mask[ny][nx] and not seen[ny][nx]:
                            seen[ny][nx] = True
                            queue.append((ny, nx))
                yield piece


def from_background(path):
    """
    The logo cut out of the title picture: the yellow and red of the letters, the dark
    outline and white highlights next to them, in the pieces that touch the letters, with
    pinholes filled
    """
    picture = Image.open(path).convert("RGB").crop((CUT_X0, CUT_Y0, CUT_X1, CUT_Y1))
    w, h = picture.size
    pixels = picture.load()
    fill = [[False] * w for _ in range(h)]
    dark = [[False] * w for _ in range(h)]
    bright = [[False] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            r, g, b = pixels[x, y]
            lum = (r * 299 + g * 587 + b * 114) // 1000
            yellow = r > 160 and g > 130 and b < 130 and r - b > 60
            red = r > 140 and g < 100 and b < 100 and r - g > 60
            fill[y][x] = yellow or red
            dark[y][x] = lum < 75 or (b < 180 and g - r < 35 and b - r > 25 and lum < 140)
            bright[y][x] = lum > 200
    near = dilate(fill, 5)
    beside = dilate(fill, 1)
    mask = [
        [fill[y][x] or (bright[y][x] and beside[y][x]) or (dark[y][x] and near[y][x]) for x in range(w)]
        for y in range(h)
    ]
    mask = invert(dilate(invert(dilate(mask, 1)), 1))  # closed: gaps of a pixel filled

    kept = [[False] * w for _ in range(h)]
    for piece in pieces(mask):
        if len(piece) > 20 and any(fill[y][x] for y, x in piece):
            for y, x in piece:
                kept[y][x] = True
    for hole in pieces(invert(kept)):
        if len(hole) < 12 and not any(y in (0, h - 1) or x in (0, w - 1) for y, x in hole):
            for y, x in hole:
                kept[y][x] = True

    logo = Image.new("RGBA", (w, h))
    out = logo.load()
    for y in range(h):
        for x in range(w):
            if kept[y][x]:
                out[x, y] = pixels[x, y] + (255,)
    return place(logo)


def rgba16(r, g, b, a):
    return ((r >> 3) << 11) | ((g >> 3) << 6) | ((b >> 3) << 1) | a


def to_ci8(logo):
    """The palette and every pixel's colour, the width padded to a multiple of 8"""
    width = (logo.width + 7) & ~7
    padded = Image.new("RGBA", (width, logo.height))
    padded.paste(logo, (0, 0))
    opaque = [a >= 128 for a in padded.getchannel("A").tobytes()]
    # The colours are picked from the pixels that are drawn: the others take one of theirs
    rgb = padded.convert("RGB").tobytes()
    rgb = [rgb[i : i + 3] for i in range(0, len(rgb), 3)]
    some = next(colour for colour, keep in zip(rgb, opaque) if keep)
    solid = Image.frombytes("RGB", padded.size, b"".join(c if keep else some for c, keep in zip(rgb, opaque)))
    quantized = solid.quantize(colors=COLOURS, method=Image.MEDIANCUT, dither=Image.Dither.NONE)
    flat = quantized.getpalette()
    palette = [0] * 256
    for i in range(COLOURS):
        r, g, b = flat[i * 3 : i * 3 + 3]
        palette[i + 1] = rgba16(r, g, b, 1)
    indices = bytes((index + 1) if keep else 0 for index, keep in zip(quantized.tobytes(), opaque))
    return width, struct.pack(">256H", *palette) + indices


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("out")
    parser.add_argument("--size", type=lambda s: int(s, 0), required=True, help="the most it can take, the block's size")
    parser.add_argument("--logo", help="the artwork, used if the file exists")
    parser.add_argument("--background", required=True, help="the title picture, for the cut-out")
    args = parser.parse_args()

    if args.logo and os.path.isfile(args.logo):
        logo, x, y = from_artwork(args.logo)
        source = args.logo
    else:
        logo, x, y = from_background(args.background)
        source = "the title picture"
    width, raw = to_ci8(logo)
    packed = crunch64.yay0.compress(raw)
    blob = b"LOGO" + struct.pack(">HHHHI", x, y, width, logo.height, len(packed)) + packed
    if len(blob) > args.size:
        sys.exit(f"the logo takes {len(blob)} bytes, more than the block's {args.size}")
    with open(args.out, "wb") as f:
        f.write(blob + bytes(-len(blob) % 16))
    print(f"{args.out}: logo from {source}, {width}x{logo.height} at ({x}, {y}), {len(packed)} bytes compressed")


if __name__ == "__main__":
    main()
