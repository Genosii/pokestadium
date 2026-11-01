#!/usr/bin/env python3
"""
Makes the 3D hub's city (src/fragments/37/randomizer_hub.c, HUB=1 builds) as C data: the
ground, the plaza and its streets, the buildings around it and the hills on the horizon,
as F3DEX2 vertex and display lists with their textures, included by randomizer_hub.c.

The game has no model of the complex (docs/game-screens.md, the area select), so these are
made here after the area select's picture of it: simple shapes (boxes, drums, domes,
cones), coloured after it, and the shading of a sun baked into each corner's colour, as
N64 games often light their scenery. The textures (paving, windows, signs, the GB
Tower's screen) are drawn here too.

The camera stands at the middle of the plaza (0, 0) and the places are round it at the
headings the hub turns to: place k at -k * 45 degrees, its direction (sin, cos) of that,
the Stadium straight ahead (+z). A building is made in its own frame, x to the right as
one faces it from the plaza, y up and z away from the plaza, its front at its smallest z.

Usage: gen_hub_city.py OUT.inc.c
"""
import argparse
import math
import random

from PIL import Image, ImageDraw, ImageFont

SUN = None  # set below, normalised
AMBIENT = 0.58
DIFFUSE = 0.48
VERTS_PER_LOAD = 32  # F3DEX2's vertex buffer

FONT_PATHS = [
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
]


def norm(v):
    length = math.sqrt(sum(c * c for c in v)) or 1.0
    return tuple(c / length for c in v)


def sub(a, b):
    return tuple(x - y for x, y in zip(a, b))


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def dot(a, b):
    return sum(x * y for x, y in zip(a, b))


SUN = norm((0.45, 0.8, -0.35))


def hexrgb(s):
    s = s.lstrip("#")
    return tuple(int(s[i : i + 2], 16) for i in (0, 2, 4))


def mix(a, b, t):
    return tuple(round(x + (y - x) * t) for x, y in zip(a, b))


# Colours, after the area select's picture
C = {k: hexrgb(v) for k, v in {
    "grass": "#86c04e", "grass2": "#7ab346", "grass_far": "#93b98a",
    "paving": "#e6dcc6", "street": "#c9c1ae", "curb": "#f2efe6",
    "stone": "#c8c6bf", "stone_dark": "#9c9a92", "white": "#f3f4f1", "offwhite": "#e9ebdf",
    "dome": "#ebcd4a", "dome2": "#d9b636", "glass": "#5d7fa6", "glass_dark": "#3e5677",
    "sign_blue": "#8aa6d6", "dark": "#3a3a44",
    "gb_body": "#e3cdb0", "gb_bezel": "#55556a", "gb_button": "#b23a6e", "gb_dpad": "#2d2d36",
    "purple": "#a07ad8", "pink": "#f29ac0", "yellow": "#f2d24a", "teal": "#4cbcc0",
    "red": "#d8443a", "orange": "#f0903a", "blue": "#4a7ad8",
    "roof": "#c74a3a", "roof2": "#a83c30", "cream": "#f1e6cf", "gold": "#e8c040",
    "rock": "#6e655b", "rock2": "#5a524a", "moss": "#6f8a4a", "path": "#d8c79e",
    "castle": "#a6a49c", "castle_roof": "#55688a",
    "trunk": "#7a5634", "leaf": "#4f9a3c", "leaf2": "#3f8432", "leaf3": "#62ad48",
    "mountain": "#7f9c8a", "mountain2": "#93ab9f", "snow": "#e8eef2",
    "field": "#9fd062", "line": "#f6f6f2", "seat_red": "#c84848", "seat_blue": "#4868c8",
    "mewtwo_stone": "#8e8a9a", "mewtwo_glow": "#b070e0", "crystal": "#7a4ab0",
}.items()}


def shade(colour, normal):
    lit = AMBIENT + DIFFUSE * max(0.0, dot(normal, SUN))
    # Faces looking up get a little of the sky
    lit += 0.08 * max(0.0, normal[1])
    return tuple(max(0, min(255, round(c * lit))) for c in colour)


class Texture:
    """An RGBA16 texture, at most 4 KB (TMEM)"""

    def __init__(self, name, image, wrap=True):
        self.name = name
        self.image = image.convert("RGBA")
        self.w, self.h = self.image.size
        assert self.w * self.h * 2 <= 4096, name
        self.wrap = wrap

    def words(self):
        out = []
        for y in range(self.h):
            for x in range(self.w):
                r, g, b, a = self.image.getpixel((x, y))
                out.append(((r >> 3) << 11) | ((g >> 3) << 6) | ((b >> 3) << 1) | (1 if a >= 128 else 0))
        return out


class Face:
    def __init__(self, verts, colours, uvs, texture):
        self.verts = verts  # 3 or 4 positions
        self.colours = colours
        self.uvs = uvs  # texel coordinates, or None
        self.texture = texture


class Mesh:
    """Faces in a building's own frame, put in the world by place()"""

    def __init__(self):
        self.faces = []

    # A face of 3 or 4 corners (in order round it), lit by its normal, which points away
    # from `inside` if given
    def poly(self, pts, colour, texture=None, uvs=None, inside=None, normal=None, colours=None, unlit=False):
        if normal is None:
            normal = norm(cross(sub(pts[1], pts[0]), sub(pts[2], pts[0])))
            if inside is not None:
                centre = tuple(sum(p[i] for p in pts) / len(pts) for i in range(3))
                if dot(normal, sub(centre, inside)) < 0:
                    normal = tuple(-c for c in normal)
        self.faces.append(Face(list(pts), colours or [colour] * len(pts), uvs, texture))
        self.faces[-1].normal = normal
        self.faces[-1].unlit = unlit

    def box(self, x, y, z, w, h, d, colour, top=None, front_tex=None, tile=None, sides_tex=False):
        """A box: (x, z) the middle of its base, y its bottom; front_tex on its front (and
        sides), tiled every `tile` units"""
        x0, x1, y0, y1, z0, z1 = x - w / 2, x + w / 2, y, y + h, z - d / 2, z + d / 2
        inside = (x, y + h / 2, z)

        def uv(a, b):
            if front_tex is None:
                return None
            tw, th = front_tex.w, front_tex.h
            return [(u * tw / tile, v * th / tile) for u, v in ((0, 0), (a, 0), (a, b), (0, b))]

        # front, back, left, right: corners top-left, top-right, bottom-right, bottom-left
        self.poly([(x0, y1, z0), (x1, y1, z0), (x1, y0, z0), (x0, y0, z0)], colour, front_tex, uv(w, h), inside)
        self.poly([(x1, y1, z1), (x0, y1, z1), (x0, y0, z1), (x1, y0, z1)], colour,
                  front_tex if sides_tex else None, uv(w, h) if sides_tex else None, inside)
        self.poly([(x0, y1, z1), (x0, y1, z0), (x0, y0, z0), (x0, y0, z1)], colour,
                  front_tex if sides_tex else None, uv(d, h) if sides_tex else None, inside)
        self.poly([(x1, y1, z0), (x1, y1, z1), (x1, y0, z1), (x1, y0, z0)], colour,
                  front_tex if sides_tex else None, uv(d, h) if sides_tex else None, inside)
        self.poly([(x0, y1, z1), (x1, y1, z1), (x1, y1, z0), (x0, y1, z0)], top or colour, inside=inside)

    def frustum(self, x, y, z, r0, r1, h, colours, seg=16, cap=None, start=0.0, texture=None, tile=None,
                bands=None):
        """A drum (r0 at the bottom, r1 at the top), its segments in turn in `colours`; a cap
        on top if given. `bands`: a list of (y fraction, colours) rings up its height"""
        rings = bands or [(1.0, colours)]
        ybot = y
        inside = (x, y + h / 2, z)
        for frac, cols in rings:
            ytop = y + h * frac
            rb = r0 + (r1 - r0) * (ybot - y) / h
            rt = r0 + (r1 - r0) * (ytop - y) / h
            for i in range(seg):
                a0 = start + 2 * math.pi * i / seg
                a1 = start + 2 * math.pi * (i + 1) / seg
                p = [(x + rt * math.sin(a0), ytop, z - rt * math.cos(a0)),
                     (x + rt * math.sin(a1), ytop, z - rt * math.cos(a1)),
                     (x + rb * math.sin(a1), ybot, z - rb * math.cos(a1)),
                     (x + rb * math.sin(a0), ybot, z - rb * math.cos(a0))]
                uvs = None
                if texture is not None:
                    arc = 2 * math.pi * rb / seg
                    uvs = [(0, 0), (arc * texture.w / tile, 0), (arc * texture.w / tile, (ytop - ybot) * texture.h / tile),
                           (0, (ytop - ybot) * texture.h / tile)]
                self.poly(p, cols[i % len(cols)], texture, uvs, inside)
            ybot = ytop
        if cap is not None:
            self.disc(x, y + h, z, r1, cap, seg, start=start)

    def disc(self, x, y, z, r, colour, seg=16, start=0.0, down=False):
        for i in range(seg):
            a0 = start + 2 * math.pi * i / seg
            a1 = start + 2 * math.pi * (i + 1) / seg
            self.poly([(x, y, z), (x + r * math.sin(a0), y, z - r * math.cos(a0)),
                       (x + r * math.sin(a1), y, z - r * math.cos(a1))],
                      colour, normal=(0, -1 if down else 1, 0))

    def cone(self, x, y, z, r, h, colours, seg=16, start=0.0):
        inside = (x, y + h / 3, z)
        for i in range(seg):
            a0 = start + 2 * math.pi * i / seg
            a1 = start + 2 * math.pi * (i + 1) / seg
            self.poly([(x, y + h, z), (x + r * math.sin(a1), y, z - r * math.cos(a1)),
                       (x + r * math.sin(a0), y, z - r * math.cos(a0))], colours[i % len(colours)], inside=inside)

    def dome(self, x, y, z, r, h, colours, seg=20, rings=6, top=None, start=0.0):
        """The top half of an ellipsoid: radius r, height h, its segments in `colours`"""
        centre = (x, y, z)
        for j in range(rings):
            t0 = (math.pi / 2) * j / rings
            t1 = (math.pi / 2) * (j + 1) / rings
            for i in range(seg):
                a0 = start + 2 * math.pi * i / seg
                a1 = start + 2 * math.pi * (i + 1) / seg

                def pt(a, t):
                    return (x + r * math.cos(t) * math.sin(a), y + h * math.sin(t), z - r * math.cos(t) * math.cos(a))

                colour = colours[i % len(colours)]
                if top is not None and j == rings - 1:
                    colour = top
                if j == rings - 1:
                    self.poly([pt(a0, t1), pt(a1, t0), pt(a0, t0)], colour, inside=centre)
                else:
                    self.poly([pt(a0, t1), pt(a1, t1), pt(a1, t0), pt(a0, t0)], colour, inside=centre)

    def sphere(self, x, y, z, r, colour, seg=10, rings=4, colours=None):
        self.dome(x, y, z, r, r, colours or [colour], seg, rings)
        # the bottom half, upside down
        centre = (x, y, z)
        for j in range(rings):
            t0 = -(math.pi / 2) * j / rings
            t1 = -(math.pi / 2) * (j + 1) / rings
            for i in range(seg):
                a0 = 2 * math.pi * i / seg
                a1 = 2 * math.pi * (i + 1) / seg

                def pt(a, t):
                    return (x + r * math.cos(t) * math.sin(a), y + r * math.sin(t), z - r * math.cos(t) * math.cos(a))

                c = (colours or [colour])[i % len(colours or [colour])]
                if j == rings - 1:
                    self.poly([pt(a0, t0), pt(a1, t0), pt(a0, t1)], c, inside=centre)
                else:
                    self.poly([pt(a0, t0), pt(a1, t0), pt(a1, t1), pt(a0, t1)], c, inside=centre)

    def pyramid(self, x, y, z, w, d, h, colour, ridge=0.0):
        """A hipped roof on a w x d base, its ridge `ridge` long along x"""
        x0, x1, z0, z1 = x - w / 2, x + w / 2, z - d / 2, z + d / 2
        rx0, rx1 = x - ridge / 2, x + ridge / 2
        top = y + h
        inside = (x, y, z)
        self.poly([(rx0, top, z), (rx1, top, z), (x1, y, z0), (x0, y, z0)], colour, inside=inside)
        self.poly([(rx1, top, z), (rx0, top, z), (x0, y, z1), (x1, y, z1)], colour, inside=inside)
        self.poly([(rx0, top, z), (x0, y, z0), (x0, y, z1)], colour, inside=inside)
        self.poly([(rx1, top, z), (x1, y, z1), (x1, y, z0)], colour, inside=inside)

    def panel(self, x0, y0, x1, y1, z, colour, texture=None, flip_u=False):
        """A flat quad facing the plaza (-z), the texture across it once"""
        uvs = None
        if texture is not None:
            uvs = [(0, 0), (texture.w, 0), (texture.w, texture.h), (0, texture.h)]
        self.poly([(x0, y1, z), (x1, y1, z), (x1, y0, z), (x0, y0, z)], colour, texture, uvs, normal=(0, 0, -1))

    def arc_panel(self, x, z, r, y0, y1, a0, a1, colour, textures, seg_per_tex=4):
        """A strip round a drum of radius r, from angle a0 to a1 (0 straight at the plaza,
        positive to the right), each texture across seg_per_tex of its segments"""
        n = len(textures) * seg_per_tex
        for i in range(n):
            t = textures[i // seg_per_tex]
            k = i % seg_per_tex
            b0 = a0 + (a1 - a0) * i / n
            b1 = a0 + (a1 - a0) * (i + 1) / n
            u0, u1 = t.w * k / seg_per_tex, t.w * (k + 1) / seg_per_tex
            p = [(x + r * math.sin(b0), y1, z - r * math.cos(b0)), (x + r * math.sin(b1), y1, z - r * math.cos(b1)),
                 (x + r * math.sin(b1), y0, z - r * math.cos(b1)), (x + r * math.sin(b0), y0, z - r * math.cos(b0))]
            nrm = norm((math.sin((b0 + b1) / 2), 0, -math.cos((b0 + b1) / 2)))
            self.poly(p, colour, t, [(u0, 0), (u1, 0), (u1, t.h), (u0, t.h)], normal=nrm)

    def tree(self, x, z, h, rnd):
        """A round tree: a trunk, a crown of a low dome over an upside-down cone"""
        self.frustum(x, 0, z, 10, 8, h * 0.45, [C["trunk"]], seg=4, start=rnd.random())
        leaves = rnd.sample([C["leaf"], C["leaf2"], C["leaf3"]], 3)
        r = h * 0.34
        start = rnd.random()
        self.dome(x, h * 0.6, z, r, h * 0.4, leaves, seg=6, rings=2, start=start)
        # the crown's underside
        for i in range(6):
            a0 = start + 2 * math.pi * i / 6
            a1 = start + 2 * math.pi * (i + 1) / 6
            self.poly([(x, h * 0.36, z), (x + r * math.sin(a1), h * 0.6, z - r * math.cos(a1)),
                       (x + r * math.sin(a0), h * 0.6, z - r * math.cos(a0))], leaves[i % 3],
                      normal=(0, -1, 0))

    def lamp(self, x, z):
        self.frustum(x, 0, z, 6, 4, 190, [C["dark"]], seg=5)
        self.box(x, 190, z, 28, 26, 28, C["dark"])
        self.box(x, 195, z, 22, 18, 22, C["yellow"])
        self.pyramid(x, 216, z, 34, 34, 14, C["dark"])

    def place(self, heading_deg, distance):
        """Into the world: the frame's origin `distance` along the heading, its z away from
        the plaza and its x to the right as seen from it"""
        a = math.radians(heading_deg)
        dx, dz = math.sin(a), math.cos(a)
        ox, oz = dx * distance, dz * distance
        # x: the screen's right facing (dx, dz) is (-dz, dx) (game-engine.md)
        rx, rz = -dz, dx

        def tf(p):
            return (ox + p[0] * rx + p[2] * dx, p[1], oz + p[0] * rz + p[2] * dz)

        def tn(n):
            return norm((n[0] * rx + n[2] * dx, n[1], n[0] * rz + n[2] * dz))

        for f in self.faces:
            f.verts = [tf(p) for p in f.verts]
            f.normal = tn(f.normal)
        return self


def place_heading(k):
    return -45.0 * k


# Textures ---------------------------------------------------------------------------------

def font(size):
    for path in FONT_PATHS:
        try:
            return ImageFont.truetype(path, size)
        except OSError:
            pass
    return ImageFont.load_default(size)


def noise_fill(img, base, amount, rnd):
    px = img.load()
    for y in range(img.size[1]):
        for x in range(img.size[0]):
            n = rnd.randint(-amount, amount)
            px[x, y] = tuple(max(0, min(255, c + n)) for c in base) + (255,)


def make_textures():
    rnd = random.Random(7)
    t = {}

    img = Image.new("RGBA", (32, 32))
    noise_fill(img, C["paving"], 10, rnd)
    d = ImageDraw.Draw(img)
    for i in (0, 16):
        d.line((0, i, 31, i), fill=mix(C["paving"], (90, 80, 70), 0.35))
    for y0, off in ((0, 0), (16, 8)):
        for x in (off, off + 16):
            d.line((x % 32, y0, x % 32, y0 + 15), fill=mix(C["paving"], (90, 80, 70), 0.35))
    t["paving"] = Texture("paving", img)

    img = Image.new("RGBA", (32, 32))
    noise_fill(img, C["street"], 12, rnd)
    d = ImageDraw.Draw(img)
    for y in range(0, 32, 8):
        d.line((0, y, 31, y), fill=mix(C["street"], (60, 55, 50), 0.4))
        off = 0 if (y // 8) % 2 == 0 else 8
        for x in range(off, 32, 16):
            d.line((x, y, x, y + 7), fill=mix(C["street"], (60, 55, 50), 0.4))
    t["street"] = Texture("street", img)

    # The Stadium's sign, in two halves of 64x32
    sign = Image.new("RGBA", (128, 32), C["offwhite"] + (255,))
    d = ImageDraw.Draw(sign)
    f = font(26)
    w = d.textlength("STADIUM", font=f)
    for ox, oy in ((-2, 0), (2, 0), (0, -2), (0, 2), (2, 2)):
        d.text(((128 - w) / 2 + ox, 1 + oy), "STADIUM", font=f, fill=(52, 70, 120, 255))
    d.text(((128 - w) / 2, 1), "STADIUM", font=f, fill=C["sign_blue"] + (255,))
    t["stadium_l"] = Texture("stadium_l", sign.crop((0, 0, 64, 32)), wrap=False)
    t["stadium_r"] = Texture("stadium_r", sign.crop((64, 0, 128, 32)), wrap=False)

    # The Stadium's windows: a band of glass between white pillars
    img = Image.new("RGBA", (32, 32), C["white"] + (255,))
    d = ImageDraw.Draw(img)
    d.rectangle((5, 4, 26, 27), fill=C["glass"] + (255,))
    d.rectangle((5, 4, 26, 9), fill=mix(C["glass"], (255, 255, 255), 0.3) + (255,))
    d.line((15, 4, 15, 27), fill=C["white"] + (255,))
    t["stadium_windows"] = Texture("stadium_windows", img)

    # The GB Tower's screen: a Poke Ball on the Game Boy's greens
    greens = [(15, 56, 15), (48, 98, 48), (139, 172, 15), (155, 188, 15)]
    img = Image.new("RGBA", (32, 32), greens[3] + (255,))
    d = ImageDraw.Draw(img)
    d.ellipse((6, 5, 25, 24), fill=greens[0] + (255,))
    d.ellipse((8, 7, 23, 22), fill=greens[2] + (255,))
    d.pieslice((8, 7, 23, 22), 180, 360, fill=greens[1] + (255,))
    d.line((8, 14, 23, 14), fill=greens[0] + (255,), width=2)
    d.ellipse((13, 12, 18, 17), fill=greens[0] + (255,))
    d.ellipse((14, 13, 17, 16), fill=greens[3] + (255,))
    d.rectangle((4, 27, 27, 28), fill=greens[1] + (255,))
    t["gb_screen"] = Texture("gb_screen", img, wrap=False)

    def word_sign(name, lines, bg, fg, outline):
        img = Image.new("RGBA", (64, 32), bg + (255,))
        d = ImageDraw.Draw(img)
        size = 13 if len(lines) > 1 else 18
        while size > 8 and max(d.textlength(line, font=font(size)) for line in lines) > 60:
            size -= 1
        f = font(size)
        y = 2 if len(lines) > 1 else (32 - size) // 2 - 1
        for line in lines:
            w = d.textlength(line, font=f)
            for ox, oy in ((-1, 0), (1, 0), (0, -1), (0, 1)):
                d.text(((64 - w) / 2 + ox, y + oy), line, font=f, fill=outline + (255,))
            d.text(((64 - w) / 2, y), line, font=f, fill=fg + (255,))
            y += 15
        t[name] = Texture(name, img, wrap=False)

    word_sign("kids_sign", ["KIDS", "CLUB"], C["yellow"], C["purple"], (255, 255, 255))
    word_sign("free_sign", ["FREE", "BATTLE"], C["blue"], (255, 255, 255), (30, 40, 90))
    word_sign("lab_sign", ["POK\xc9MON", "LAB"], C["white"], C["glass_dark"], (200, 210, 230))
    word_sign("gb_sign", ["GB TOWER"], C["gb_body"], C["gb_bezel"], (250, 240, 225))
    word_sign("palace_sign", ["VICTORY", "PALACE"], C["cream"], C["roof2"], C["gold"])

    # The palace's windows, arched, on its cream walls
    img = Image.new("RGBA", (32, 32))
    noise_fill(img, C["cream"], 4, rnd)
    d = ImageDraw.Draw(img)
    d.rectangle((9, 10, 22, 27), fill=C["glass_dark"] + (255,))
    d.ellipse((9, 4, 22, 17), fill=C["glass_dark"] + (255,))
    d.line((15, 6, 15, 27), fill=C["white"] + (255,))
    d.line((9, 17, 22, 17), fill=C["white"] + (255,))
    d.rectangle((7, 27, 24, 29), fill=C["white"] + (255,))
    t["palace_windows"] = Texture("palace_windows", img)

    # The Lab's glass: panes between white frames
    img = Image.new("RGBA", (32, 32), C["white"] + (255,))
    d = ImageDraw.Draw(img)
    d.rectangle((2, 6, 29, 25), fill=C["glass"] + (255,))
    d.line((16, 6, 16, 25), fill=C["white"] + (255,), width=2)
    d.polygon([(4, 24), (12, 7), (15, 7), (7, 24)], fill=mix(C["glass"], (255, 255, 255), 0.35) + (255,))
    t["lab_windows"] = Texture("lab_windows", img)

    # The castle's stone
    img = Image.new("RGBA", (32, 32))
    noise_fill(img, C["castle"], 12, rnd)
    d = ImageDraw.Draw(img)
    for y in range(0, 32, 8):
        d.line((0, y, 31, y), fill=mix(C["castle"], (40, 40, 40), 0.5))
        off = 0 if (y // 8) % 2 == 0 else 6
        for x in range(off, 32, 12):
            d.line((x, y, x, y + 7), fill=mix(C["castle"], (40, 40, 40), 0.5))
    t["castle_stone"] = Texture("castle_stone", img)

    # Rock
    img = Image.new("RGBA", (32, 32))
    noise_fill(img, C["rock"], 18, rnd)
    d = ImageDraw.Draw(img)
    for _ in range(14):
        x, y = rnd.randrange(32), rnd.randrange(32)
        d.line((x, y, x + rnd.randint(-8, 8), y + rnd.randint(2, 9)), fill=mix(C["rock"], (20, 18, 16), 0.5))
    t["rock"] = Texture("rock", img)
    return t


# The city ---------------------------------------------------------------------------------

PLAZA_R = 430
STREET_W = 260


def build_ground(tex, rnd):
    m = Mesh()
    # Grass out to the hills, in rings, its colour varying a little face by face
    radii = [PLAZA_R, 800, 1300, 2000, 2900, 4000, 5400, 7600]
    seg = 32
    for j in range(len(radii) - 1):
        r0, r1 = radii[j], radii[j + 1]
        for i in range(seg):
            a0 = 2 * math.pi * i / seg
            a1 = 2 * math.pi * (i + 1) / seg
            far = j / (len(radii) - 2)
            base = mix(C["grass"] if (i + j) % 2 else C["grass2"], C["grass_far"], far * 0.8)
            base = mix(base, (255, 255, 255), rnd.random() * 0.05)
            m.poly([(r1 * math.sin(a0), 0, r1 * math.cos(a0)), (r1 * math.sin(a1), 0, r1 * math.cos(a1)),
                    (r0 * math.sin(a1), 0, r0 * math.cos(a1)), (r0 * math.sin(a0), 0, r0 * math.cos(a0))],
                   base, normal=(0, 1, 0))
    # The plaza: paving, in two rings of slightly different stone round a middle circle
    pseg = 24
    for r0, r1, colour in ((0, 160, mix(C["paving"], C["stone"], 0.5)), (160, 200, C["stone_dark"]),
                           (200, PLAZA_R, C["paving"])):
        for i in range(pseg):
            a0 = 2 * math.pi * i / pseg
            a1 = 2 * math.pi * (i + 1) / pseg
            pts = [(r1 * math.sin(a0), 8, r1 * math.cos(a0)), (r1 * math.sin(a1), 8, r1 * math.cos(a1)),
                   (r0 * math.sin(a1), 8, r0 * math.cos(a1)), (r0 * math.sin(a0), 8, r0 * math.cos(a0))]
            tile = 120.0
            uvs = [(p[0] * 32 / tile + 512, p[2] * 32 / tile + 512) for p in pts]
            if r0 == 0:
                pts, uvs = pts[:3], uvs[:3]
            m.poly(pts, (255, 255, 255) if colour == C["paving"] else colour,
                   tex["paving"] if colour == C["paving"] else None, uvs if colour == C["paving"] else None,
                   normal=(0, 1, 0))
    # A low rim round the plaza, broken where the streets leave it
    for i in range(48):
        a0 = 2 * math.pi * i / 48
        a1 = 2 * math.pi * (i + 1) / 48
        mid = math.degrees((a0 + a1) / 2) % 45
        if mid < 11 or mid > 34:
            continue
        r0, r1 = PLAZA_R, PLAZA_R + 24
        p = lambda r, a, y: (r * math.sin(a), y, r * math.cos(a))
        m.poly([p(r0, a0, 22), p(r0, a1, 22), p(r0, a1, 0), p(r0, a0, 0)], C["curb"], inside=(0, 0, 0),
               normal=norm((-math.sin(a0), 0, -math.cos(a0))))
        m.poly([p(r1, a0, 22), p(r1, a1, 22), p(r0, a1, 22), p(r0, a0, 22)], C["curb"], normal=(0, 1, 0))
    return m


def build_street(tex, k, length):
    """A street from the plaza to place k, with its curbs and lamps, in that place's frame
    (z from the plaza's middle)"""
    m = Mesh()
    z0, z1 = PLAZA_R - 20, length
    w = STREET_W
    tile = 110.0
    step = 220.0
    z = z0
    while z < z1:
        za = min(z + step, z1)
        uvs = [(0, (z - z0) * 32 / tile), (w * 32 / tile, (z - z0) * 32 / tile),
               (w * 32 / tile, (za - z0) * 32 / tile), (0, (za - z0) * 32 / tile)]
        m.poly([(-w / 2, 8, z), (w / 2, 8, z), (w / 2, 8, za), (-w / 2, 8, za)], (255, 255, 255),
               tex["street"], uvs, normal=(0, 1, 0))
        z = za
    for side in (-1, 1):
        m.box(side * (w / 2 + 12), 0, (z0 + z1) / 2 + 20, 24, 16, z1 - z0 - 40, C["curb"])
    zl = z0 + 520
    while zl < z1 - 150:
        for side in (-1, 1):
            m.lamp(side * (w / 2 + 40), zl)
        zl += 380
    return m


def build_stadium(tex):
    m = Mesh()
    m.frustum(0, 0, 0, 1080, 1060, 50, [C["stone"], C["stone_dark"]], seg=32, cap=C["paving"])
    # The drum: white, a band of windows, a terrace on top
    m.frustum(0, 50, 0, 860, 840, 600, [C["offwhite"]], seg=32, bands=[(0.48, [C["offwhite"], C["white"]])])
    m.frustum(0, 50 + 600 * 0.48, 0, 846, 842, 120, [C["white"]], seg=32, texture=tex["stadium_windows"], tile=110)
    m.frustum(0, 50 + 600 * 0.48 + 120, 0, 842, 840, 600 * 0.52 - 120, [C["offwhite"], C["white"]], seg=32)
    m.frustum(0, 650, 0, 890, 900, 40, [C["white"]], seg=32, cap=C["stone"])
    m.frustum(0, 690, 0, 860, 850, 30, [C["stone_dark"]], seg=32)
    # The dome, its ribs in two yellows, and the lantern on top
    m.dome(0, 720, 0, 840, 560, [C["dome"], C["dome2"]], seg=32, rings=7, top=C["dome"])
    m.frustum(0, 1270, 0, 90, 80, 90, [C["white"]], seg=12)
    m.dome(0, 1360, 0, 95, 70, [C["dome"]], seg=12, rings=3)
    m.frustum(0, 1420, 0, 4, 4, 160, [C["dark"]], seg=4)
    m.panel(0, 1530, 90, 1580, 0, C["red"])
    # The sign over the entrance, round the drum
    m.arc_panel(0, 0, 868, 380, 520, -0.5, 0.5, (255, 255, 255), [tex["stadium_l"], tex["stadium_r"]])
    # The entrance: a dark doorway between pillars, under a canopy, steps up to it
    m.panel(-230, 50, 230, 300, -872, C["glass_dark"])
    for x in (-270, -90, 90, 270):
        m.box(x, 50, -880, 50, 290, 50, C["white"])
    m.box(0, 340, -900, 640, 30, 110, C["white"], top=C["stone"])
    for i, (w, d) in enumerate(((700, 260), (640, 200), (580, 140))):
        m.box(0, 0, -1000 + d / 2 - 40, w, 18 * (i + 1), d, C["stone"])
    # Flags round it
    for i in range(10):
        a = math.radians((70 + (i % 5) * 18) * (1 if i < 5 else -1))
        x, z = 1000 * math.sin(a), -1000 * math.cos(a)
        m.frustum(x, 50, z, 6, 5, 420, [C["dark"]], seg=4)
        colour = [C["red"], C["blue"], C["yellow"], C["teal"], C["pink"]][i % 5]
        m.poly([(x, 470, z), (x + 90, 450, z), (x + 90, 400, z), (x, 380, z)], colour, normal=(0, 0, -1))
    return m


def build_gb_tower(tex):
    m = Mesh()
    m.frustum(0, 0, 0, 560, 600, 130, [C["stone"], C["stone_dark"]], seg=24, cap=C["grass"])
    # The Game Boy, standing: its body, the screen's frame and screen, the buttons
    W, H, D, Y = 700, 1180, 230, 130
    m.box(0, Y, 0, W, H, D, C["gb_body"])
    # each layer on the front at least 15 units from the last, for the depth buffer's sake
    m.box(0, Y + 20, -D / 2 - 10, W - 60, H - 40, 20, mix(C["gb_body"], (255, 255, 255), 0.15))
    m.box(0, Y + 610, -D / 2 - 35, W - 130, 470, 30, C["gb_bezel"])
    m.panel(-200, Y + 660, 200, Y + 1030, -D / 2 - 66, (255, 255, 255), tex["gb_screen"])
    m.box(-235, Y + 900, -D / 2 - 60, 18, 18, 20, C["red"])  # the power light
    m.panel(-180, Y + 520, 180, Y + 590, -D / 2 - 36, (255, 255, 255), tex["gb_sign"])
    # The D-pad on the viewer's left, A and B on the right
    m.box(-185, Y + 330, -D / 2 - 45, 190, 60, 50, C["gb_dpad"])
    m.box(-185, Y + 265, -D / 2 - 45, 60, 190, 50, C["gb_dpad"])
    for bx, by in ((150, Y + 330), (250, Y + 390)):
        # buttons facing the plaza: discs in front of the body
        for i in range(12):
            a0 = 2 * math.pi * i / 12
            a1 = 2 * math.pi * (i + 1) / 12
            z = -D / 2 - 55
            m.poly([(bx, by, z), (bx + 50 * math.cos(a0), by + 50 * math.sin(a0), z),
                    (bx + 50 * math.cos(a1), by + 50 * math.sin(a1), z)], C["gb_button"], normal=(0, 0, -1))
    for x in (-60, 40):
        m.box(x, Y + 170, -D / 2 - 40, 70, 20, 40, C["gb_dpad"])
    for i in range(6):
        m.box(200 + i * 28, Y + 60, -D / 2 - 38, 10, 120, 36, C["gb_dpad"])
    return m


def build_free_battle(tex):
    m = Mesh()
    # The field, its lines, the stands round it
    rx, rz, cz = 700, 900, 1000
    seg = 32
    for i in range(seg):
        a0 = 2 * math.pi * i / seg
        a1 = 2 * math.pi * (i + 1) / seg
        m.poly([(0, 10, cz), (rx * math.sin(a0), 10, cz - rz * math.cos(a0)), (rx * math.sin(a1), 10, cz - rz * math.cos(a1))],
               C["field"] if i % 2 else mix(C["field"], C["grass"], 0.3), normal=(0, 1, 0))
    for r0, r1, sx in ((0.82, 0.85, 1.0), (0.2, 0.23, 1.0)):
        for i in range(seg):
            a0 = 2 * math.pi * i / seg
            a1 = 2 * math.pi * (i + 1) / seg
            p = lambda r, a: (rx * r * math.sin(a), 22, cz - rz * r * math.cos(a) * (0.62 if r < 0.5 else 1.0))
            m.poly([p(r1, a0), p(r1, a1), p(r0, a1), p(r0, a0)], C["line"], normal=(0, 1, 0))
    m.poly([(-rx * 0.82, 22, cz + 10), (rx * 0.82, 22, cz + 10), (rx * 0.82, 22, cz - 10), (-rx * 0.82, 22, cz - 10)],
           C["line"], normal=(0, 1, 0))
    for step in range(4):
        r0 = 1.0 + step * 0.07
        h = 40 + step * 45
        for i in range(seg):
            a0 = 2 * math.pi * i / seg
            a1 = 2 * math.pi * (i + 1) / seg
            if abs(math.degrees((a0 + a1) / 2) - 0) < 30 or abs(math.degrees((a0 + a1) / 2) - 360) < 30:
                continue  # the opening at the gate
            p = lambda r, a, y: (rx * r * math.sin(a), y, cz - rz * r * math.cos(a))
            seat = C["seat_red"] if (i // 2) % 2 else C["seat_blue"]
            m.poly([p(r0, a0, h), p(r0, a1, h), p(r0, a1, 0), p(r0, a0, 0)], seat, inside=(0, 0, cz))
            m.poly([p(r0 + 0.07, a0, h), p(r0 + 0.07, a1, h), p(r0, a1, h), p(r0, a0, h)], C["stone"], normal=(0, 1, 0))
    # The gate at the street's end
    for x in (-280, 280):
        m.box(x, 0, 60, 110, 820, 110, C["white"], top=C["stone"])
        m.dome(x, 820, 60, 75, 75, [C["red"]], seg=10, rings=3)
    m.box(0, 690, 60, 680, 90, 80, C["blue"])
    m.panel(-200, 580, 200, 780, -5, (255, 255, 255), tex["free_sign"])
    for x in (-500, 500):
        m.frustum(x, 0, 250, 6, 5, 520, [C["dark"]], seg=4)
        m.poly([(x, 510, 250), (x + 120 * (1 if x > 0 else -1), 490, 250), (x + 120 * (1 if x > 0 else -1), 430, 250),
                (x, 420, 250)], C["red"], normal=(0, 0, -1))
    return m


def build_kids_club(tex, rnd):
    m = Mesh()
    m.frustum(0, 0, 500, 820, 820, 16, [C["pink"], C["yellow"], C["teal"], C["purple"]], seg=24,
              cap=mix(C["paving"], C["pink"], 0.25))
    # The big top: purple and white stripes, a striped roof, a flag
    m.frustum(0, 16, 520, 400, 400, 520, [C["purple"], C["white"]], seg=20)
    m.cone(0, 536, 520, 470, 600, [C["purple"], C["yellow"]], seg=20)
    m.frustum(0, 1136, 520, 5, 5, 160, [C["dark"]], seg=4)
    m.panel(0, 1230, 110, 1290, 520, C["red"])
    # A pink tent on the viewer's left, a yellow one on the right, a teal house behind
    m.frustum(-560, 6, 420, 230, 230, 240, [C["pink"], C["white"]], seg=16)
    m.dome(-560, 246, 420, 250, 220, [C["pink"], mix(C["pink"], (255, 255, 255), 0.4)], seg=16, rings=4)
    m.sphere(-560, 490, 420, 30, C["yellow"], seg=8, rings=2)
    m.frustum(560, 6, 380, 200, 200, 200, [C["yellow"], C["orange"]], seg=16)
    m.cone(560, 206, 380, 260, 360, [C["yellow"], C["red"]], seg=16)
    m.box(250, 6, 900, 520, 360, 300, C["teal"], top=C["white"])
    m.pyramid(250, 366, 900, 560, 340, 140, C["blue"], ridge=200)
    # The gate: candy-striped posts, the sign, balloons
    for x in (-300, 300):
        m.frustum(x, 0, -150, 45, 45, 520, [C["red"]], seg=10,
                  bands=[(i / 8, [C["red"] if i % 2 else C["white"]]) for i in range(1, 9)])
        m.sphere(x, 560, -150, 60, C["yellow"], seg=10, rings=3)
    m.box(0, 430, -150, 660, 80, 50, C["purple"])
    m.panel(-170, 440, 170, 620, -200, (255, 255, 255), tex["kids_sign"])
    colours = [C["red"], C["yellow"], C["teal"], C["pink"], C["blue"], C["orange"]]
    for i in range(14):
        x = rnd.uniform(-420, 420)
        y = rnd.uniform(650, 900)
        z = -150 + rnd.uniform(-60, 60)
        m.sphere(x, y, z, 36, colours[i % len(colours)], seg=6, rings=2)
        m.poly([(x - 1, y - 36, z), (x + 1, y - 36, z), (x * 0.4 + 1, 470, -150), (x * 0.4 - 1, 470, -150)],
               C["white"], normal=(0, 0, -1))
    return m


def build_victory_palace(tex):
    m = Mesh()
    W, H, D = 1250, 700, 520
    m.box(0, 0, 0, W + 60, 40, D + 60, C["stone"])
    m.box(0, 40, 0, W, H, D, (255, 255, 255), front_tex=tex["palace_windows"], tile=125, sides_tex=True)
    m.box(0, 40 + H, 0, W + 30, 30, D + 30, C["white"])
    m.pyramid(0, 70 + H, 0, W + 20, D + 20, 260, C["roof"], ridge=W - D)
    # The wings
    for x in (-W / 2 - 220, W / 2 + 220):
        m.box(x, 0, 60, 460, 560, 420, (255, 255, 255), front_tex=tex["palace_windows"], tile=125, sides_tex=True)
        m.pyramid(x, 560, 60, 480, 440, 240, C["roof2"], ridge=40)
    # The tower in the middle, its spire and a golden ball
    m.box(0, 40, 0, 300, 1350, 300, (255, 255, 255), front_tex=tex["palace_windows"], tile=125, sides_tex=True)
    m.box(0, 1390, 0, 330, 30, 330, C["white"])
    m.pyramid(0, 1420, 0, 320, 320, 460, C["roof"])
    m.sphere(0, 1900, 0, 40, C["gold"], seg=10, rings=3)
    # The portico: columns, a pediment, steps, the sign
    for i in range(6):
        m.frustum(-300 + i * 120, 40, -D / 2 - 110, 30, 26, 460, [C["white"], C["offwhite"]], seg=8)
    m.box(0, 500, -D / 2 - 110, 760, 40, 160, C["white"])
    m.poly([(-390, 540, -D / 2 - 190), (390, 540, -D / 2 - 190), (0, 700, -D / 2 - 190)], C["cream"], normal=(0, 0, -1))
    m.poly([(-400, 535, -D / 2 - 195), (0, 710, -D / 2 - 195), (0, 710, -D / 2 - 20), (-400, 535, -D / 2 - 20)],
           C["roof"], inside=(0, 400, -D / 2 - 100))
    m.poly([(0, 710, -D / 2 - 195), (400, 535, -D / 2 - 195), (400, 535, -D / 2 - 20), (0, 710, -D / 2 - 20)],
           C["roof"], inside=(0, 400, -D / 2 - 100))
    m.panel(-130, 550, 130, 615, -D / 2 - 210, (255, 255, 255), tex["palace_sign"])
    for i, d in enumerate((330, 270, 210)):
        m.box(0, 0, -D / 2 - 40 - d / 2 + 20, 820, 14 * (i + 1) + 12, d, C["stone"])
    return m


def build_lab(tex):
    m = Mesh()
    W, H, D = 1300, 760, 560
    m.box(0, 0, 0, W + 40, 30, D + 40, C["stone"])
    m.box(0, 30, 0, W, H, D, (255, 255, 255), front_tex=tex["lab_windows"], tile=105, sides_tex=True)
    m.box(0, 30 + H, 0, W + 20, 26, D + 20, C["white"])
    T = 30 + H + 26
    m.box(-150, T, 40, 760, 420, 420, (255, 255, 255), front_tex=tex["lab_windows"], tile=105, sides_tex=True)
    m.box(-150, T + 420, 40, 780, 22, 440, C["white"])
    # The observatory on the roof
    m.frustum(330, T, 60, 230, 230, 200, [C["white"], C["offwhite"]], seg=20)
    m.dome(330, T + 200, 60, 240, 230, [C["white"], C["stone"]], seg=20, rings=5)
    m.panel(300, T + 214, 360, T + 414, 60 - 238, C["dark"])
    # The antenna and its dish
    m.frustum(-420, T + 442, 40, 8, 6, 300, [C["stone_dark"]], seg=4)
    m.dome(-420, T + 684, 40, 110, 50, [C["white"], C["stone"]], seg=12, rings=2)
    # The entrance: glass doors under a canopy, the sign over them
    m.panel(-170, 30, 170, 300, -D / 2 - 16, C["glass_dark"])
    m.box(0, 300, -D / 2 - 90, 520, 26, 200, C["white"], top=C["stone"])
    for x in (-240, 240):
        m.frustum(x, 30, -D / 2 - 170, 16, 16, 270, [C["stone_dark"]], seg=6)
    m.panel(-140, 330, 140, 450, -D / 2 - 18, (255, 255, 255), tex["lab_sign"])
    m.box(0, 0, -D / 2 - 120, 560, 14, 200, C["stone"])
    return m


def build_castle(tex, rnd):
    m = Mesh()
    # The pillar of rock, narrowing as it rises, twisting, with moss in bands
    y = 0
    r = 820
    angle = 0.0
    profile = [(0, r)]
    while y < 1350:
        h = rnd.uniform(240, 330)
        r1 = max(330, r * rnd.uniform(0.84, 0.93))
        # the texture's own colour, lighter or mossy band by band
        cols = [(240, 230, 220), (214, 204, 194)] if (y // 300) % 3 else [(190, 222, 150), (226, 216, 204)]
        m.frustum(rnd.uniform(-30, 30), y, rnd.uniform(-30, 30), r, r1, h, cols, seg=12, start=angle,
                  texture=tex["rock"], tile=220)
        y += h
        profile.append((y, r1))
        r = r1 * 1.04
        angle += 0.35
    m.frustum(0, y, 0, r, r * 0.95, 30, [C["grass"]], seg=12, cap=C["grass"])
    # The path winding up it
    turns = 2.2
    steps = 60
    for i in range(steps):
        t0, t1 = i / steps, (i + 1) / steps
        a0, a1 = turns * 2 * math.pi * t0, turns * 2 * math.pi * t1
        y0, y1 = 40 + t0 * (y - 80), 40 + t1 * (y - 80)
        def rock_r(yy):
            for (ya, r_a), (yb, r_b) in zip(profile, profile[1:]):
                if ya <= yy <= yb:
                    return r_a + (r_b - r_a) * (yy - ya) / (yb - ya)
            return profile[-1][1]

        ra = rock_r(y0) - 10
        rb = rock_r(y1) - 10
        p = lambda rr, a, yy: (rr * math.sin(a), yy, -rr * math.cos(a))
        m.poly([p(ra + 90, a0, y0), p(rb + 90, a1, y1), p(rb, a1, y1), p(ra, a0, y0)], C["path"],
               normal=(0, 1, 0))
        m.poly([p(ra + 90, a0, y0 - 30), p(rb + 90, a1, y1 - 30), p(rb + 90, a1, y1), p(ra + 90, a0, y0)],
               mix(C["path"], (0, 0, 0), 0.25), inside=(0, y0, 0))
    # The castle on top
    top = y + 30
    m.box(0, top, 0, 460, 330, 460, (255, 255, 255), front_tex=tex["castle_stone"], tile=120, sides_tex=True)
    for x in (-230, 230):
        for z in (-230, 230):
            m.frustum(x, top, z, 90, 90, 470, [(255, 255, 255)], seg=10, texture=tex["castle_stone"], tile=120)
            m.cone(x, top + 470, z, 120, 230, [C["castle_roof"], mix(C["castle_roof"], (0, 0, 0), 0.2)], seg=10)
    m.frustum(0, top + 330, 0, 130, 120, 360, [(255, 255, 255)], seg=12, texture=tex["castle_stone"], tile=120)
    m.cone(0, top + 690, 0, 170, 330, [C["castle_roof"], mix(C["castle_roof"], (0, 0, 0), 0.2)], seg=12)
    m.frustum(0, top + 1020, 0, 4, 4, 140, [C["dark"]], seg=4)
    m.panel(0, top + 1100, 110, top + 1160, 0, C["red"])
    m.panel(-80, top, 80, top + 180, -250, C["dark"])
    return m


def build_mewtwo_dais(tex):
    m = Mesh()
    m.frustum(0, 0, 0, 420, 400, 50, [C["mewtwo_stone"], mix(C["mewtwo_stone"], (0, 0, 0), 0.12)], seg=24,
              cap=C["mewtwo_stone"])
    m.frustum(0, 50, 0, 300, 300, 16, [C["mewtwo_glow"]], seg=24, cap=mix(C["mewtwo_stone"], C["mewtwo_glow"], 0.3))
    for i in range(6):
        a = 2 * math.pi * i / 6 + math.pi / 6
        x, z = 360 * math.sin(a), -360 * math.cos(a)
        m.frustum(x, 50, z, 50, 30, 420 + 80 * (i % 2), [C["crystal"], mix(C["crystal"], (255, 255, 255), 0.25)],
                  seg=5, start=a)
        m.cone(x, 470 + 80 * (i % 2), z, 30, 90, [C["crystal"], C["mewtwo_glow"]], seg=5, start=a)
    return m


def build_sky(rnd):
    """A dome over everything, deep blue overhead to pale at the horizon, with clouds; its
    colours as they are, unlit"""
    m = Mesh()
    top, horizon = hexrgb("#3f7fd6"), hexrgb("#d4e9f6")
    R, H = 9500, 5200
    seg, rings = 16, 5
    heights = [-1500] + [H * math.sin((math.pi / 2) * j / rings) for j in range(rings + 1)]
    radii = [R] + [R * math.cos((math.pi / 2) * j / rings) for j in range(rings + 1)]

    def colour(y):
        return mix(horizon, top, max(0.0, min(1.0, y / (H * 0.75))) ** 0.7)

    for j in range(len(heights) - 1):
        for i in range(seg):
            a0, a1 = 2 * math.pi * i / seg, 2 * math.pi * (i + 1) / seg
            y0, y1, r0, r1 = heights[j], heights[j + 1], radii[j], radii[j + 1]
            pts = [(r1 * math.sin(a0), y1, r1 * math.cos(a0)), (r1 * math.sin(a1), y1, r1 * math.cos(a1)),
                   (r0 * math.sin(a1), y0, r0 * math.cos(a1)), (r0 * math.sin(a0), y0, r0 * math.cos(a0))]
            cols = [colour(y1), colour(y1), colour(y0), colour(y0)]
            if r1 < 1:
                pts, cols = pts[1:], cols[1:]
            m.poly(pts, None, colours=cols, normal=(0, -1, 0), unlit=True)
    # Clouds: clusters of flat white puffs facing the middle, a little under the dome
    for c in range(14):
        a = 2 * math.pi * c / 14 + rnd.uniform(-0.15, 0.15)
        r = 8300
        y = rnd.uniform(1800, 3600)
        for _ in range(rnd.randint(3, 5)):
            da = rnd.uniform(-0.07, 0.07)
            cy = y + rnd.uniform(-150, 200)
            w, h = rnd.uniform(500, 900), rnd.uniform(220, 380)
            cx, cz = r * math.sin(a + da), r * math.cos(a + da)
            rx, rz = -math.cos(a + da), math.sin(a + da)  # across, seen from the middle
            shade_ = mix((255, 255, 255), horizon, rnd.uniform(0.0, 0.25))
            for i in range(8):
                b0, b1 = 2 * math.pi * i / 8, 2 * math.pi * (i + 1) / 8
                p = lambda b: (cx + rx * w * math.cos(b), cy + h * math.sin(b), cz + rz * w * math.cos(b))
                low = mix(shade_, (200, 210, 225), 0.4)
                m.poly([(cx, cy, cz), p(b0), p(b1)], None,
                       colours=[shade_, shade_ if math.sin(b0) > 0 else low, shade_ if math.sin(b1) > 0 else low],
                       normal=(0, 0, -1), unlit=True)
            r -= 60  # each puff a little in front of the last, for the depth buffer
    return m


def build_hills(rnd):
    m = Mesh()
    n = 40
    for i in range(n):
        a0 = 2 * math.pi * i / n
        a1 = 2 * math.pi * (i + 1) / n
        am = (a0 + a1) / 2 + rnd.uniform(-0.03, 0.03)
        r = rnd.uniform(6600, 7300)
        h = rnd.uniform(500, 1500) if i % 3 else rnd.uniform(1400, 2300)
        base = mix(C["mountain"], C["mountain2"], rnd.random())
        rr = 7600
        p0 = (rr * math.sin(a0 - 0.05), -40, rr * math.cos(a0 - 0.05))
        p1 = (rr * math.sin(a1 + 0.05), -40, rr * math.cos(a1 + 0.05))
        pk = (r * math.sin(am), h, r * math.cos(am))
        m.poly([pk, p1, p0], base, colours=[mix(base, C["snow"], 0.55 if h > 1700 else 0.15), base, base],
               normal=norm((-math.sin(am), 0.6, -math.cos(am))))
    # A ring of far trees in front of them
    for i in range(90):
        a = 2 * math.pi * i / 90 + rnd.uniform(-0.02, 0.02)
        r = rnd.uniform(4300, 5200)
        x, z = r * math.sin(a), r * math.cos(a)
        h = rnd.uniform(260, 420)
        colour = mix(C["leaf2"], C["grass_far"], 0.45)
        m.cone(x, 0, z, h * 0.35, h, [colour, mix(colour, (0, 0, 0), 0.1)], seg=5)
    return m


# The places, as the hub orders them (randomizer_hub.c's sPlaces): its heading, the distance
# to its frame's origin and to where its street ends
PLACES = [
    ("stadium", 0, 2400, 1250),
    ("gb_tower", 1, 2000, 1450),
    ("free_battle", 2, 1350, 1360),
    ("kids_club", 3, 1900, 1700),
    ("victory_palace", 4, 2500, 2000),
    ("lab", 5, 2150, 1750),
    ("castle", 6, 2700, 1800),
    ("mewtwo", 7, 1400, 950),
]


def build_city():
    rnd = random.Random(1996)
    tex = make_textures()
    parts = []  # (name, mesh, heading, half angle in degrees: None always drawn)

    parts.append(("sky", build_sky(rnd), 0, None))
    ground = build_ground(tex, rnd)
    for name, k, dist, street in PLACES:
        s = build_street(tex, k, street)
        s.place(place_heading(k), 0)
        ground.faces += s.faces
    parts.append(("ground", ground, 0, None))
    parts.append(("hills", build_hills(rnd), 0, None))
    # Trees between the streets, a part for each gap, drawn only when it's in view
    for k in range(8):
        trees = Mesh()
        heading = place_heading(k) - 22.5
        a = math.radians(heading)
        for r in (1150, 1500, 1900, 2400, 2900, 3500):
            for off in (-0.07, 0.07) if r > 1700 else (0.0,):
                rr = r + rnd.uniform(-90, 90)
                aa = a + off + rnd.uniform(-0.05, 0.05)
                trees.tree(rr * math.sin(aa), rr * math.cos(aa), rnd.uniform(230, 340), rnd)
        parts.append(("trees%d" % k, trees, heading, 14.0))

    builders = {
        "stadium": lambda: build_stadium(tex),
        "gb_tower": lambda: build_gb_tower(tex),
        "free_battle": lambda: build_free_battle(tex),
        "kids_club": lambda: build_kids_club(tex, rnd),
        "victory_palace": lambda: build_victory_palace(tex),
        "lab": lambda: build_lab(tex),
        "castle": lambda: build_castle(tex, rnd),
        "mewtwo": lambda: build_mewtwo_dais(tex),
    }
    for name, k, dist, street in PLACES:
        mesh = builders[name]()
        mesh.place(place_heading(k), dist)
        # How far round it reaches, seen from the middle
        half = 0.0
        for f in mesh.faces:
            for p in f.verts:
                a = math.degrees(math.atan2(p[0], p[2])) - place_heading(k)
                a = (a + 180) % 360 - 180
                half = max(half, abs(a))
        parts.append((name, mesh, place_heading(k), half))
    return tex, parts


# Output -----------------------------------------------------------------------------------

def emit(out_path):
    tex, parts = build_city()
    lines = ["// Made by tools/randomizer/gen_hub_city.py: don't edit", ""]
    used = set()
    for name, mesh, _, _ in parts:
        for f in mesh.faces:
            if f.texture is not None:
                used.add(f.texture.name)
    for t in tex.values():
        if t.name not in used:
            continue
        words = t.words()
        lines.append("static union { u16 px[%d]; long long align; } sCityTex_%s = { {" % (len(words), t.name))
        for i in range(0, len(words), 16):
            lines.append("    " + ", ".join("0x%04X" % w for w in words[i : i + 16]) + ",")
        lines.append("} };")
    lines.append("")

    total_verts = 0
    part_lines = []
    for name, mesh, heading, half in parts:
        # By texture, so each is loaded once
        groups = {}
        for f in mesh.faces:
            groups.setdefault(f.texture.name if f.texture else "", []).append(f)
        vtx = []
        gfx = []
        for tname in sorted(groups):
            faces = groups[tname]
            gfx.append("gsDPPipeSync()")
            if tname:
                t = tex[tname]
                cm = "G_TX_WRAP" if t.wrap else "G_TX_CLAMP"
                mw = int(math.log2(t.w)) if t.wrap else 0
                mh = int(math.log2(t.h)) if t.wrap else 0
                gfx.append("gsDPSetCombineMode(G_CC_MODULATEI, G_CC_MODULATEI)")
                gfx.append("gsSPTexture(0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON)")
                gfx.append(
                    "gsDPLoadTextureBlock(sCityTex_%s.px, G_IM_FMT_RGBA, G_IM_SIZ_16b, %d, %d, 0, "
                    "G_TX_NOMIRROR | %s, G_TX_NOMIRROR | %s, %d, %d, G_TX_NOLOD, G_TX_NOLOD)"
                    % (tname, t.w, t.h, cm, cm, mw, mh))
            else:
                gfx.append("gsDPSetCombineMode(G_CC_SHADE, G_CC_SHADE)")
                gfx.append("gsSPTexture(0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_OFF)")
            batch = []

            def flush():
                if not batch:
                    return
                start = len(vtx)
                tris = []
                for f in batch:
                    base = len(vtx) - start
                    for i, p in enumerate(f.verts):
                        col = f.colours[i] if f.unlit else shade(f.colours[i], f.normal)
                        uv = f.uvs[i] if f.uvs else (0, 0)
                        s, t2 = round(uv[0] * 32), round(uv[1] * 32)
                        assert -32768 <= s < 32768 and -32768 <= t2 < 32768, (name, uv)
                        vtx.append("{{{%d, %d, %d}, 0, {%d, %d}, {%d, %d, %d, 255}}}" % (
                            round(p[0]), round(p[1]), round(p[2]), s, t2, col[0], col[1], col[2]))
                    tris.append((base, base + 1, base + 2))
                    if len(f.verts) == 4:
                        tris.append((base, base + 2, base + 3))
                gfx.append("gsSPVertex(&sCityVtx_%s[%d], %d, 0)" % (name, start, len(vtx) - start))
                for i in range(0, len(tris) - 1, 2):
                    a, b = tris[i], tris[i + 1]
                    gfx.append("gsSP2Triangles(%d, %d, %d, 0, %d, %d, %d, 0)" % (a + b))
                if len(tris) % 2:
                    gfx.append("gsSP1Triangle(%d, %d, %d, 0)" % tris[-1])
                batch.clear()

            count = 0
            for f in faces:
                if count + len(f.verts) > VERTS_PER_LOAD:
                    flush()
                    count = 0
                batch.append(f)
                count += len(f.verts)
            flush()
        gfx.append("gsSPEndDisplayList()")
        total_verts += len(vtx)
        lines.append("static Vtx sCityVtx_%s[%d] = {" % (name, len(vtx)))
        lines += ["    %s," % v for v in vtx]
        lines.append("};")
        lines.append("static Gfx sCityDl_%s[] = {" % name)
        lines += ["    %s," % g for g in gfx]
        lines.append("};")
        lines.append("")
        if half is None:
            part_lines.append("    { sCityDl_%s, 0, 0x8000 }," % name)
        else:
            part_lines.append("    { sCityDl_%s, %d, 0x%04X }," % (
                name, round(heading / 360 * 0x10000), min(0x8000, round(half / 360 * 0x10000))))
            print("%-15s %5d vertices, %4.1f degrees either side" % (name, sum(len(f.verts) for f in mesh.faces), half))
    lines.append("// Each part: its display list, its heading and how far round it reaches from it")
    lines.append("static CityPart sCityParts[] = {")
    lines += part_lines
    lines.append("};")
    lines.append("")
    with open(out_path, "w", encoding="latin-1") as fh:
        fh.write("\n".join(lines))
    return total_verts


def main():
    parser = argparse.ArgumentParser(description=__doc__.strip().splitlines()[0])
    parser.add_argument("out")
    args = parser.parse_args()
    n = emit(args.out)
    print("%d vertices" % n)


if __name__ == "__main__":
    main()
