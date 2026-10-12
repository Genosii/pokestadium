#!/usr/bin/env python3
"""The Pokemon's models for the camera editor's preview, from what the game drew.

    rip_models.py STATE SPECIES_LEFT SPECIES_RIGHT OUT_DIR

reads a mupen64plus savestate of the title screen's 3D scene (both Pokemon drawn; see
rip_all.sh), walks the last complete frame's display list (F3DEX2) as the RSP would, and writes
each side's Pokemon as OUT_DIR/NNN.json: its triangles as the game drew them that frame (in
its idle stance, posed by the game's own matrices), where it stands taken away and turned to
face +x as the player's side does, with the textures decoded from an emulated TMEM.

The models are the game's: they're made from the player's own ROM, never committed.
"""
import base64
import io
import json
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "headless_test"))
from savestate import load  # noqa: E402

from PIL import Image  # noqa: E402

SIDE_X = 150.0  # where the two stand (docs/game-engine.md)
CARD_HEIGHT = 536.0


def mtx(m, a):
    ints = struct.unpack_from(">16h", m, a)
    frac = struct.unpack_from(">16H", m, a + 32)
    return [[ints[i * 4 + j] + frac[i * 4 + j] / 65536.0 for j in range(4)] for i in range(4)]


def mul(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(4)) for j in range(4)] for i in range(4)]


IDENTITY = [[1.0 if i == j else 0.0 for j in range(4)] for i in range(4)]


class Tile:
    def __init__(self):
        self.fmt = self.siz = self.line = self.tmem = self.palette = 0
        self.cms = self.cmt = self.masks = self.maskt = self.shifts = self.shiftt = 0
        self.uls = self.ult = self.lrs = self.lrt = 0


def rgba16(v):
    return ((v >> 11) & 31) * 255 // 31, ((v >> 6) & 31) * 255 // 31, ((v >> 1) & 31) * 255 // 31, 255 if v & 1 else 0


def decode(tmem, t, pal_ia=False):
    """The texture a tile describes, from TMEM, as an RGBA image (None for one it can't read)"""
    w = (1 << t.masks) if t.masks else ((t.lrs - t.uls) >> 2) + 1
    h = (1 << t.maskt) if t.maskt else ((t.lrt - t.ult) >> 2) + 1
    if w <= 0 or h <= 0 or w > 256 or h > 256:
        return None
    bits = (4, 8, 16, 32)[t.siz]
    stride = t.line * 8
    base = t.tmem * 8
    img = Image.new("RGBA", (w, h))
    px = img.load()

    def byte(addr):
        return tmem[addr & 0xFFF]

    for y in range(h):
        row = base + y * stride
        swap = 4 if (y & 1) else 0  # odd rows are stored with their 32-bit words swapped
        for x in range(w):
            if bits == 4:
                a = row + (x >> 1)
                v = byte(a ^ swap)
                v = (v >> 4) if (x & 1) == 0 else (v & 0xF)
            elif bits == 8:
                v = byte((row + x) ^ swap)
            elif bits == 16:
                a = (row + x * 2) ^ swap
                v = (byte(a) << 8) | byte(a + 1)
            else:
                a = (row + x * 2) ^ swap  # 32-bit: red and green in the low half, blue and alpha in the high
                v = (byte(a) << 24) | (byte(a + 1) << 16) | (byte(a + 0x800) << 8) | byte(a + 0x801)
            if t.fmt == 2:  # CI: the palette in the high half of TMEM
                idx = (t.palette * 16 + v) if bits == 4 else v
                pa = 0x800 + idx * 2
                c = (byte(pa) << 8) | byte(pa + 1)
                if pal_ia:
                    i, al = c >> 8, c & 0xFF
                    px[x, y] = (i, i, i, al)
                else:
                    px[x, y] = rgba16(c)
            elif t.fmt == 0:
                px[x, y] = rgba16(v) if bits == 16 else (v >> 24, (v >> 16) & 255, (v >> 8) & 255, v & 255)
            elif t.fmt == 3:  # IA
                if bits == 4:
                    i = (v >> 1) * 255 // 7
                    px[x, y] = (i, i, i, 255 if v & 1 else 0)
                elif bits == 8:
                    i = (v >> 4) * 17
                    px[x, y] = (i, i, i, (v & 15) * 17)
                else:
                    px[x, y] = (v >> 8, v >> 8, v >> 8, v & 255)
            elif t.fmt == 4:  # I
                i = v * 17 if bits == 4 else v
                px[x, y] = (i, i, i, 255)
            else:
                return None
    return img


def walk(m, start, max_cmds=400000):
    """Every triangle of a frame's display list: its three vertices (position after the
    modelview matrix, texture coordinates, where in RAM they came from) and its material"""
    seg = [0] * 16
    tmem = bytearray(4096)
    tiles = [Tile() for _ in range(8)]
    timg = (0, 0, 1, 0)  # fmt, siz, width, address
    tex = {"on": False, "s": 0xFFFF, "t": 0xFFFF, "tile": 0}
    prim = (255, 255, 255, 255)
    tlut_ia = False
    mv = [IDENTITY]
    vbuf = [None] * 64
    tris = []
    textures = {}  # (tile state, TMEM bytes) -> image
    stack = []

    def phys(a):
        if (a >> 24) >= 0x80:
            return a & 0x7FFFFF
        return (seg[(a >> 24) & 0xF] + (a & 0xFFFFFF)) & 0x7FFFFF

    pc = start & 0x7FFFFF
    n = 0
    while n < max_cmds:
        n += 1
        w0, w1 = struct.unpack_from(">II", m, pc)
        op = w0 >> 24
        pc += 8
        if op == 0xDF:  # G_ENDDL
            if not stack:
                break
            pc = stack.pop()
        elif op == 0xDE:  # G_DL
            if (w0 >> 16) & 0xFF == 0:
                stack.append(pc)
            pc = phys(w1)
        elif op == 0xDB and (w0 >> 16) & 0xFF == 6:  # G_MOVEWORD, a segment
            seg[(w0 & 0xFFFF) // 4] = w1 & 0xFFFFFF
        elif op == 0xDA:  # G_MTX
            p = (w0 & 0xFF) ^ 1
            if not p & 4:
                M = mtx(m, phys(w1))
                if p & 1:
                    mv.append(mv[-1])
                mv[-1] = M if p & 2 else mul(M, mv[-1])
        elif op == 0xD8:  # G_POPMTX
            for _ in range(w1 // 64):
                if len(mv) > 1:
                    mv.pop()
        elif op == 0xFD:  # G_SETTIMG
            timg = ((w0 >> 21) & 7, (w0 >> 19) & 3, (w0 & 0xFFF) + 1, phys(w1))
        elif op == 0xF5:  # G_SETTILE
            t = tiles[(w1 >> 24) & 7]
            t.fmt, t.siz, t.line, t.tmem = (w0 >> 21) & 7, (w0 >> 19) & 3, (w0 >> 9) & 0x1FF, w0 & 0x1FF
            t.palette, t.cmt, t.maskt, t.shiftt = (w1 >> 20) & 0xF, (w1 >> 18) & 3, (w1 >> 14) & 0xF, (w1 >> 10) & 0xF
            t.cms, t.masks, t.shifts = (w1 >> 8) & 3, (w1 >> 4) & 0xF, w1 & 0xF
        elif op == 0xF2:  # G_SETTILESIZE
            t = tiles[(w1 >> 24) & 7]
            t.uls, t.ult, t.lrs, t.lrt = (w0 >> 12) & 0xFFF, w0 & 0xFFF, (w1 >> 12) & 0xFFF, w1 & 0xFFF
        elif op == 0xF3:  # G_LOADBLOCK
            t = tiles[(w1 >> 24) & 7]
            texels = ((w1 >> 12) & 0xFFF) + 1
            dxt = w1 & 0xFFF
            size = (texels << timg[1]) >> 1  # bytes
            src = timg[3]
            dst = t.tmem * 8
            for i in range((size + 7) // 8):
                word = bytearray(m[src + i * 8:src + i * 8 + 8])
                if dxt and ((i * dxt) >> 11) & 1:
                    word = word[4:] + word[:4]
                for b in range(8):
                    tmem[(dst + i * 8 + b) & 0xFFF] = word[b] if b < len(word) else 0
        elif op == 0xF4:  # G_LOADTILE
            t = tiles[(w1 >> 24) & 7]
            uls, ult, lrs, lrt = (w0 >> 12) & 0xFFF, w0 & 0xFFF, (w1 >> 12) & 0xFFF, w1 & 0xFFF
            bpp = (4, 8, 16, 32)[timg[1]]
            rowbytes = (((lrs - uls) >> 2) + 1) * bpp // 8
            for r, y in enumerate(range(ult >> 2, (lrt >> 2) + 1)):
                src = timg[3] + (y * timg[2] + (uls >> 2)) * bpp // 8
                dst = t.tmem * 8 + r * t.line * 8
                for b in range(rowbytes):
                    tmem[((dst + b) ^ (4 if r & 1 else 0)) & 0xFFF] = m[src + b]
        elif op == 0xF0:  # G_LOADTLUT
            t = tiles[(w1 >> 24) & 7]
            count = ((w1 >> 14) & 0x3FF) + 1
            for i in range(count):
                a = t.tmem * 8 + i * 2
                tmem[a & 0xFFF] = m[timg[3] + i * 2]
                tmem[(a + 1) & 0xFFF] = m[timg[3] + i * 2 + 1]
        elif op == 0xE2 or op == 0xE3:  # G_SETOTHERMODE: the palette's type (G_TT_IA16)
            if op == 0xE3 and ((w0 >> 8) & 0xFF) == (32 - 14 - ((w0 & 0xFF) + 1)):
                tlut_ia = (w1 >> 14) & 3 == 3
        elif op == 0xEF:  # G_RDPSETOTHERMODE
            tlut_ia = (w0 >> 14) & 3 == 3
        elif op == 0xD7:  # G_TEXTURE
            tex = {"on": bool((w0 >> 1) & 0x7F), "s": w1 >> 16, "t": w1 & 0xFFFF, "tile": (w0 >> 8) & 7}
        elif op == 0xFA:  # G_SETPRIMCOLOR
            prim = (w1 >> 24, (w1 >> 16) & 255, (w1 >> 8) & 255, w1 & 255)
        elif op == 0x01:  # G_VTX
            num, end = (w0 >> 12) & 0xFF, (w0 >> 1) & 0x7F
            a = phys(w1)
            M = mv[-1]
            for i in range(num):
                x, y, z, _, s, t = struct.unpack_from(">6h", m, a + i * 16)
                vbuf[end - num + i] = (
                    x * M[0][0] + y * M[1][0] + z * M[2][0] + M[3][0],
                    x * M[0][1] + y * M[1][1] + z * M[2][1] + M[3][1],
                    x * M[0][2] + y * M[1][2] + z * M[2][2] + M[3][2],
                    s, t, a + i * 16,
                )
        elif op in (0x05, 0x06, 0x07):  # G_TRI1, G_TRI2, G_QUAD
            mat = None
            if tex["on"]:
                t = tiles[tex["tile"]]
                key = (t.fmt, t.siz, t.line, t.tmem, t.palette, t.masks, t.maskt, t.uls, t.ult, t.lrs, t.lrt, tlut_ia,
                       bytes(tmem))
                if key not in textures:
                    textures[key] = (decode(tmem, t, tlut_ia), t.cms, t.cmt)
                img = textures[key][0]
                if img is not None:
                    mat = {"key": key, "tile": (t.uls, t.ult, t.shifts, t.shiftt), "size": img.size,
                           "scale": (tex["s"], tex["t"]), "wrap": (t.cms, t.cmt)}
            for idx in ([(w0 >> 16) & 0xFF, (w0 >> 8) & 0xFF, w0 & 0xFF],
                        [(w1 >> 16) & 0xFF, (w1 >> 8) & 0xFF, w1 & 0xFF]) if op != 0x05 else \
                    ([(w0 >> 16) & 0xFF, (w0 >> 8) & 0xFF, w0 & 0xFF],):
                vs = [vbuf[i // 2] for i in idx]
                if None not in vs:
                    tris.append((vs, mat, prim))
    return tris, textures


def shift(v, sh):
    if sh == 0:
        return v
    return v / (1 << sh) if sh <= 10 else v * (1 << (16 - sh))


def uv(v, mat):
    """A vertex's texture coordinates, 0 to 1 across the texture"""
    uls, ult, shs, sht = mat["tile"]
    s = shift(v[3] / 32.0 * mat["scale"][0] / 65536.0, shs) - uls / 4.0
    t = shift(v[4] / 32.0 * mat["scale"][1] / 65536.0, sht) - ult / 4.0
    return s / mat["size"][0], t / mat["size"][1]


def side_tris(tris, side, pts, height):
    """The triangles of a side's Pokemon: near the points its model marked (pts, in the arena's
    coordinates; or, without them, on its half of the field), above the ground (not its
    shadow), and from the blocks of memory most of them come from (not the arena's)"""
    sign = -1 if side == 0 else 1
    if pts:
        margin = max(height, 30.0)
        lo = [min(p[i] for p in pts) - margin for i in range(3)]
        hi = [max(p[i] for p in pts) + margin for i in range(3)]
    out = []
    for vs, mat, prim in tris:
        cx = sum(v[0] for v in vs) / 3
        cy = sum(v[1] for v in vs) / 3
        cz = sum(v[2] for v in vs) / 3
        if pts:
            if not all(lo[i] <= c <= hi[i] for i, c in enumerate((cx, cy, cz))):
                continue
        elif cx * sign <= 20 or abs(cx - sign * SIDE_X) > 400 or abs(cz) > 400:
            continue
        if max(v[1] for v in vs) < 0.6 and cy < 0.6:
            continue  # its shadow, flat on the ground
        out.append((vs, mat, prim))
    # The arena's own triangles are in a few big blocks of memory; the Pokemon's in its own buffer:
    # keep the 64 KB blocks most of the side's triangles come from
    blocks = {}
    for vs, _, _ in out:
        blocks[vs[0][5] >> 16] = blocks.get(vs[0][5] >> 16, 0) + 1
    keep = {b for b, c in blocks.items() if c >= 8}
    return [t for t in out if t[0][0][5] >> 16 in keep]


def model(tris, side, textures, stand):
    """A side's triangles as the editor's model: grouped by texture, standing at the origin and
    facing +x; textures as PNG data"""
    parts = {}
    tex_ids = {}
    tex_out = []
    sign = -1 if side == 0 else 1
    for vs, mat, prim in tris:
        if mat is not None:
            if mat["key"] not in tex_ids:
                img, cms, cmt = textures[mat["key"]]
                buf = io.BytesIO()
                img.save(buf, "PNG", optimize=True)
                tex_ids[mat["key"]] = len(tex_out)
                wrap = lambda c: "clamp" if c & 2 else ("mirror" if c & 1 else "repeat")  # noqa: E731
                tex_out.append({"png": base64.b64encode(buf.getvalue()).decode(), "ws": wrap(cms), "wt": wrap(cmt)})
            key = ("t", tex_ids[mat["key"]])
        else:
            key = ("c",) + tuple(prim[:3])
        part = parts.setdefault(key, {"pos": [], "uv": []})
        for v in vs:
            x, y, z = v[0] - sign * stand, v[1], v[2]
            if side == 1:
                x, z = -x, -z  # turned to face +x
            part["pos"] += [round(x, 2), round(y, 2), round(z, 2)]
            if mat is not None:
                u, w = uv(v, mat)
                part["uv"] += [round(u, 4), round(w, 4)]
    out = []
    for key, p in parts.items():
        entry = {"pos": p["pos"]}
        if key[0] == "t":
            entry["tex"] = key[1]
            entry["uv"] = p["uv"]
        else:
            entry["color"] = list(key[1:])
        out.append(entry)
    return {"parts": out, "textures": tex_out}


def place(species):
    """How far from the middle of the field a species stands, as the battle and the title put it
    (randomizer_title_arena.c): Onix and Gyarados farther, Venusaur and Lapras a little"""
    return 225.0 if species in (95, 130) else 175.0 if species in (3, 131) else SIDE_X


def points(m, species, side):
    """The points the side's model marked on itself as it was drawn (docs/game-engine.md, "Points
    on a model"), where the battle and the clips aim, and how high it floats: its model is found
    by its species (unk_01A) and where it stands (unk_024)"""
    x0 = -place(species) if side == 0 else place(species)
    for a in range(0x100000, 0x7F0000, 4):
        if struct.unpack_from(">h", m, a + 0x1A)[0] != species:
            continue
        x, y0, z = struct.unpack_from(">fff", m, a + 0x24)
        if abs(x - x0) > 1 or abs(z) > 1 or not 1 <= m[a + 0xA7] <= 12:
            continue
        out = {}
        for i in range(min(m[a + 0xA7], 12)):
            pid = struct.unpack_from(">H", m, a + 0xA8 + i * 16)[0]
            px, py, pz = struct.unpack_from(">fff", m, a + 0xA8 + i * 16 + 4)
            if not 1 <= pid <= 255 or abs(px - x0) > 1000 or abs(pz) > 1000 or not -100 < py < 1000:
                break  # not a model's points: something else that happens to look like one
            px -= x0
            if side == 1:
                px, pz = -px, -pz
            out[str(pid)] = [round(px, 2), round(py, 2), round(pz, 2)]
        else:
            if "100" in out:  # every model marks its root
                return out, round(y0, 2)
    return {}, 0.0


def main():
    state, left, right, out_dir = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), Path(sys.argv[4])
    out_dir.mkdir(parents=True, exist_ok=True)
    m = load(state)
    which = struct.unpack_from(">h", m, (0x800A7428 + 0x10) & 0x7FFFFF)[0]
    buffers = struct.unpack_from(">II", m, 0x800A7428 & 0x7FFFFF)
    start = buffers[which ^ 1]  # the frame finished before the one being built
    tris, textures = walk(m, start)
    for side, species in ((0, left), (1, right)):
        pts, float_y = points(m, species, side)
        stand = place(species)
        x0, sign = (-stand, 1) if side == 0 else (stand, -1)
        world = [(x0 + sign * p[0], p[1], sign * p[2]) for p in pts.values()]
        height = CARD_HEIGHT * 100.0 / struct.unpack_from(">h", m, (0x8006FF00 + (species - 1) * 0x1C + 2) & 0x7FFFFF)[0]
        mine = side_tris(tris, side, world, height)
        mdl = model(mine, side, textures, stand)
        mdl["species"] = species
        ys = [p for part in mdl["parts"] for p in part["pos"][1::3]]
        mdl["top"] = round(max(ys), 2) if ys else 0
        mdl["points"] = pts
        mdl["stand"] = [stand, float_y]  # where it stands: x from the middle of the field, and y
        (out_dir / f"{species:03d}.json").write_text(json.dumps(mdl, separators=(",", ":")))
        print(f"{species:03d}: {len(mine)} triangles, {len(mdl['textures'])} textures, top {mdl['top']}, "
              f"points {sorted(mdl['points'], key=int)}")


if __name__ == "__main__":
    main()
