#!/usr/bin/env python3
"""Reads RDRAM from a mupen64plus savestate (a script's `<frame> save <path>`).

    savestate.py battle STATE [FRAGMENT62_BASE]

prints the battle director's state and both Pokemon's models (visibility, position,
animation). fragment62 is relocated to wherever it's loaded: 0x80123360 for VRAM
0x84300000 in a cup battle reached the usual way (docs/game-engine.md); check the base if
the numbers look wrong. As a module: load(path) gives RDRAM as big-endian bytes, and u8, u16,
s16, u32, s32 and f32 read it at a (KSEG0) address.
"""
import gzip
import struct
import sys
import zipfile


def load(path):
    raw = open(path, 'rb').read()
    if raw[:2] == b'PK':
        z = zipfile.ZipFile(path)
        raw = z.read(z.namelist()[0])
    elif raw[:2] == b'\x1f\x8b':
        raw = gzip.decompress(raw)
    base = raw.find(struct.pack('<I', 0x8C870008)) - 0xB7F0  # a known word, to find RDRAM
    words = raw[base:base + 0x800000]
    out = bytearray(len(words))
    for i in range(0, len(words) - 3, 4):  # little-endian words in the file
        out[i:i + 4] = words[i:i + 4][::-1]
    return bytes(out)


def u8(m, a): return m[a & 0x7FFFFF]
def u16(m, a): return struct.unpack_from('>H', m, a & 0x7FFFFF)[0]
def s16(m, a): return struct.unpack_from('>h', m, a & 0x7FFFFF)[0]
def u32(m, a): return struct.unpack_from('>I', m, a & 0x7FFFFF)[0]
def s32(m, a): return struct.unpack_from('>i', m, a & 0x7FFFFF)[0]
def f32(m, a): return struct.unpack_from('>f', m, a & 0x7FFFFF)[0]


def battle(path, frag_base=0x80123360):
    m = load(path)

    def p(v):  # a fragment62 address, where it's loaded
        return v - 0x84300000 + frag_base if 0x84300000 <= v < 0x84400000 else v

    d = p(u32(m, p(0x84390240)))
    att, dfn = u32(m, p(0x84390204)), u32(m, p(0x84390200))
    print(f"mode {s32(m, d + 0x1C)} script {s32(m, d + 0x38)} step {s32(m, d + 0x20)} "
          f"outcome(unk_1A) {u8(m, d + 0x1A)}")
    for i in range(2):
        a = u32(m, p(0x84390010) + 4 * i)
        q = p(a)
        tag = 'attacker' if a == att else 'defender' if a == dfn else ''
        print(f"  side {i} {tag:8} species {s16(m, q + 0x1A)} shown {u8(m, q + 1) & 1} alpha {u8(m, q + 0x1D)} "
              f"pos ({f32(m, q + 0x24):.0f} {f32(m, q + 0x28):.0f} {f32(m, q + 0x2C):.0f}) "
              f"anim {s16(m, q + 0x40)} frame {s32(m, q + 0x48) / 65536:.1f} speed {u32(m, q + 0x4C) / 65536:.2f}")


if __name__ == '__main__':
    if len(sys.argv) >= 3 and sys.argv[1] == 'battle':
        battle(sys.argv[2], int(sys.argv[3], 0) if len(sys.argv) > 3 else 0x80123360)
    else:
        sys.exit(__doc__)
