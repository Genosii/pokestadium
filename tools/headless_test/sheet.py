#!/usr/bin/env python3
"""Contact sheet of a run's screenshots, each labelled with its frame.

    sheet.py OUTDIR out.jpg [columns] [every] [first] [last] [width]

every: use every n-th screenshot; first/last: frame range; width: of each thumbnail
(default 160, keep it small: a sheet of small thumbnails is enough to spot problems).
"""
import os
import sys

from PIL import Image, ImageDraw

d, out = sys.argv[1], sys.argv[2]
cols = int(sys.argv[3]) if len(sys.argv) > 3 else 6
step = int(sys.argv[4]) if len(sys.argv) > 4 else 1
first = int(sys.argv[5]) if len(sys.argv) > 5 else 0
last = int(sys.argv[6]) if len(sys.argv) > 6 else 10**9
w = int(sys.argv[7]) if len(sys.argv) > 7 else 160
h = w * 3 // 4
fs = sorted(f for f in os.listdir(d) if f.endswith('.png'))
fs = [f for f in fs if first <= int(f[1:7]) <= last][::step]
rows = (len(fs) + cols - 1) // cols
sheet = Image.new('RGB', (cols * w, max(rows, 1) * h), 'white')
draw = ImageDraw.Draw(sheet)
for i, f in enumerate(fs):
    x, y = (i % cols) * w, (i // cols) * h
    sheet.paste(Image.open(os.path.join(d, f)).convert('RGB').resize((w, h)), (x, y))
    draw.rectangle([x, y, x + 34, y + 11], fill='black')
    draw.text((x + 2, y), f[1:7].lstrip('0'), fill='yellow')
sheet.save(out, quality=80)
print(out, len(fs))
