#!/usr/bin/env python3
"""
Regenerate fragment headers and relocation tables from the linked ELF.

Fragments are code overlays that the game loads anywhere in RAM and then
relocates (see Memmap_RelocateFragment in src/memmap.c). Each one starts with a
0x20 byte header and is followed in ROM by a table of relocations. Both are
extracted from the base ROM as raw binaries, so they stop matching the code as
soon as a fragment changes size or layout. This tool rebuilds them from the
relocations the linker keeps in the ELF (LDFLAGS has --emit-relocs).

Only fragments named in a list file are regenerated. The original tables can't
be reproduced for every fragment (some reference data the decomp hasn't
symbolized yet, and some carry extra no-op entries), so a fragment should only
be listed once `check --all` shows that it regenerates byte for byte on a
matching build. Every other fragment keeps its extracted table.

List file, one fragment per line:
    fragment61 func_84203E6C
where the second column is the fragment's entry point: the function the jump
in its header goes to.

Header layout (struct Fragment in src/memmap.h):
    0x00  j <entry>        entry point, relocated like any other R_MIPS_26
    0x04  nop
    0x08  "FRAGMENT"
    0x10  headerSize       always 0x20
    0x14  relocOffset      size of the fragment's code + data + rodata
    0x18  sizeInRom        relocOffset + size of the relocation table
    0x1C  sizeInRam        relocOffset + size of the fragment's bss

Relocation table: u32 count, then one u32 per relocation holding
(type << 24) | offset from the start of the fragment, zero padded to 16 bytes.
Only relocations whose target lies in the fragment address window
(0x81000000-0x8FFFFFFF, see Memmap_GetFragmentVaddr) are listed; everything
else points at code or data that never moves.

Usage:
    fragment_relocs.py check ELF LIST... [--all]
        Compare generated headers and tables with the ones linked into ELF.
        Fails if a listed fragment differs. --all reports every fragment.
    fragment_relocs.py update ELF LIST...
        Rewrite the header and relocation objects of the listed fragments.
        Exits with status 3 if anything changed, meaning ELF must be relinked.
"""

import argparse
import os
import re
import struct
import subprocess
import sys
import tempfile

R_MIPS_32 = 2
R_MIPS_26 = 4
R_MIPS_HI16 = 5
R_MIPS_LO16 = 6
R_MIPS_GPREL16 = 7
R_MIPS_PC16 = 10

FRAGMENT_VRAM_MIN = 0x81000000
FRAGMENT_VRAM_MAX = 0x90000000

HEADER_SIZE = 0x20
HEADER_MAGIC = b"FRAGMENT"

SHT_SYMTAB = 2
SHT_NOBITS = 8
SHT_REL = 9

SECTION_RE = re.compile(r"^\.(fragment\d+)$")

# Symbols the linker script defines around each segment. Code reads them as
# plain numbers (FRAGMENT_ID() turns fragmentN_TEXT_START into an id), so the
# game never relocates references to them.
LINKER_SYMBOL_RE = re.compile(
    r"_(VRAM|VRAM_END|ROM_START|ROM_END|TEXT_START|TEXT_END|TEXT_SIZE|DATA_START|DATA_END|DATA_SIZE"
    r"|RODATA_START|RODATA_END|RODATA_SIZE|BSS_START|BSS_END|BSS_SIZE)$"
)


class Section:
    def __init__(self, name, sh_type, addr, size, link, data):
        self.name = name
        self.type = sh_type
        self.addr = addr
        self.size = size
        self.link = link
        self.data = data


class Elf:
    def __init__(self, path):
        with open(path, "rb") as f:
            raw = f.read()

        if raw[:4] != b"\x7fELF" or raw[4] != 1 or raw[5] != 2:
            sys.exit(f"{path}: not a 32-bit big-endian ELF")

        (e_shoff,) = struct.unpack_from(">I", raw, 0x20)
        e_shentsize, e_shnum, e_shstrndx = struct.unpack_from(">HHH", raw, 0x2E)

        headers = [struct.unpack_from(">IIIIIIIIII", raw, e_shoff + i * e_shentsize) for i in range(e_shnum)]
        shstr = headers[e_shstrndx]
        shstr_data = raw[shstr[4] : shstr[4] + shstr[5]]

        self.sections = []
        for name, sh_type, _flags, addr, offset, size, link, _info, _align, _entsize in headers:
            data = b"" if sh_type == SHT_NOBITS else raw[offset : offset + size]
            self.sections.append(Section(cstr(shstr_data, name), sh_type, addr, size, link, data))

        self.by_name = {s.name: s for s in self.sections}

        # (name, value) by symbol table index, as relocations refer to them
        self.symbols = []
        for sec in self.sections:
            if sec.type == SHT_SYMTAB:
                strtab = self.sections[sec.link].data
                for i in range(0, len(sec.data), 16):
                    st_name, st_value = struct.unpack_from(">II", sec.data, i)
                    self.symbols.append((cstr(strtab, st_name), st_value))
                break

    def relocations(self, section):
        """(address, type, symbol name) for every relocation the linker kept for section."""
        rel = self.by_name.get(".rel" + section.name)
        if rel is None or rel.type != SHT_REL:
            return []
        out = []
        for i in range(0, len(rel.data), 8):
            r_offset, r_info = struct.unpack_from(">II", rel.data, i)
            out.append((r_offset, r_info & 0xFF, self.symbols[r_info >> 8][0]))
        return out

    def symbol_value(self, name):
        for sym_name, value in self.symbols:
            if sym_name == name:
                return value
        return None


def cstr(blob, offset):
    return blob[offset : blob.index(b"\x00", offset)].decode("ascii")


def to_s16(v):
    return v - 0x10000 if v & 0x8000 else v


def in_fragment_window(addr):
    return FRAGMENT_VRAM_MIN <= addr < FRAGMENT_VRAM_MAX


def align16(blob):
    return blob + b"\x00" * (-len(blob) % 16)


def build_relocations(elf, section):
    """The relocation table for one fragment, as the game expects it."""
    base = section.addr

    def word(addr):
        return struct.unpack_from(">I", section.data, addr - base)[0]

    # The header comes from an incbin, so it has no ELF relocations; its entry
    # jump is added separately below. The rest keep the order the compiler
    # emitted them in, which puts each LO16 right after its HI16 even when a
    # jump sits between the two instructions. The original tables use that order.
    # The linker lists relocations object by object (an object's code, then its
    # data), while the fragment is laid out as all code, then all data, then all
    # rodata; the tables follow the layout.
    relocs = [
        r for r in elf.relocations(section) if r[0] >= base + HEADER_SIZE and not LINKER_SYMBOL_RE.search(r[2])
    ]
    region_starts = sorted(
        v
        for v in (elf.symbol_value(section.name[1:] + suffix) for suffix in ("_DATA_START", "_RODATA_START"))
        if v is not None
    )
    relocs.sort(key=lambda r: sum(r[0] >= start for start in region_starts))

    kept = set()
    # Mirror Memmap_RelocateFragment: a HI16 is remembered per register and the
    # next LO16 based on that register completes the address.
    pending_hi = {}
    for addr, rtype, _sym in relocs:
        insn = word(addr)
        if rtype == R_MIPS_32:
            if in_fragment_window(insn):
                kept.add(addr)
        elif rtype == R_MIPS_26:
            if in_fragment_window(((insn & 0x03FFFFFF) << 2) | 0x80000000):
                kept.add(addr)
        elif rtype == R_MIPS_HI16:
            pending_hi[(insn >> 16) & 0x1F] = addr
        elif rtype == R_MIPS_LO16:
            reg = (insn >> 21) & 0x1F
            hi_addr = pending_hi.get(reg)
            if hi_addr is None:
                sys.exit(f"{section.name}: LO16 at {addr:08X} has no preceding HI16 for register {reg}")
            target = (((word(hi_addr) & 0xFFFF) << 16) + to_s16(insn & 0xFFFF)) & 0xFFFFFFFF
            if in_fragment_window(target):
                kept.add(hi_addr)
                kept.add(addr)
        elif rtype in (R_MIPS_PC16, R_MIPS_GPREL16):
            pass  # PC-relative branches and $gp offsets don't move with the fragment
        else:
            sys.exit(f"{section.name}: unsupported relocation type {rtype} at {addr:08X}")

    entries = [(R_MIPS_26 << 24) | 0]  # the header's "j <entry>"
    entries += [(rtype << 24) | (addr - base) for addr, rtype, _sym in relocs if addr in kept]
    return align16(struct.pack(f">{len(entries) + 1}I", len(entries), *entries))


def build_header(entry_addr, section, bss_size, table_size):
    jump = 0x08000000 | ((entry_addr >> 2) & 0x03FFFFFF)
    return (
        struct.pack(">II", jump, 0)
        + HEADER_MAGIC
        + struct.pack(">IIII", HEADER_SIZE, section.size, section.size + table_size, section.size + bss_size)
    )


def fragments(elf):
    """name -> (code section, bss size, relocs section) for every fragment in ELF."""
    out = {}
    for sec in elf.sections:
        m = SECTION_RE.match(sec.name)
        if m is None or not in_fragment_window(sec.addr):
            continue
        relocs = elf.by_name.get(f".{m.group(1)}_relocs")
        if relocs is None:
            continue
        bss = elf.by_name.get(f".{m.group(1)}_bss")
        out[m.group(1)] = (sec, bss.size if bss is not None else 0, relocs)
    return out


def read_lists(paths):
    listed = {}
    for path in paths:
        with open(path) as f:
            for line in f:
                line = line.split("#", 1)[0].strip()
                if line:
                    name, entry = line.split()
                    listed[name] = entry
    return listed


def generate(elf, frags, name, entry_addr):
    sec, bss_size, _relocs = frags[name]
    table = build_relocations(elf, sec)
    return build_header(entry_addr, sec, bss_size, len(table)), table


def cmd_check(args):
    elf = Elf(args.elf)
    frags = fragments(elf)
    listed = read_lists(args.lists)

    for name in listed:
        if name not in frags:
            sys.exit(f"{name}: listed but not in {args.elf}")

    names = list(frags) if args.all else list(listed)
    failed = []
    for name in names:
        sec, _bss, relocs_sec = frags[name]
        if name in listed:
            entry_addr = elf.symbol_value(listed[name])
            if entry_addr is None:
                sys.exit(f"{name}: entry symbol {listed[name]} not found in {args.elf}")
        else:
            jump = struct.unpack_from(">I", sec.data, 0)[0]
            entry_addr = (sec.addr & 0xF0000000) | ((jump & 0x03FFFFFF) << 2)

        header, table = generate(elf, frags, name, entry_addr)
        problems = []
        if header != sec.data[:HEADER_SIZE]:
            problems.append(f"header {header.hex()} != linked {sec.data[:HEADER_SIZE].hex()}")
        if table != relocs_sec.data:
            problems.append(f"relocation table differs ({len(table)} bytes generated, {len(relocs_sec.data)} linked)")

        tag = "listed" if name in listed else "not listed"
        if problems:
            print(f"{name} ({tag}): " + "; ".join(problems))
            if name in listed:
                failed.append(name)
        elif args.all or args.verbose:
            print(f"{name} ({tag}): OK")

    if failed:
        print(f"Fragment tables out of date: {', '.join(failed)}")
        return 1
    return 0


def cmd_update(args):
    elf = Elf(args.elf)
    frags = fragments(elf)
    changed = False

    for name, entry in read_lists(args.lists).items():
        if name not in frags:
            sys.exit(f"{name}: listed but not in {args.elf}")
        entry_addr = elf.symbol_value(entry)
        if entry_addr is None:
            sys.exit(f"{name}: entry symbol {entry} not found in {args.elf}")

        header, table = generate(elf, frags, name, entry_addr)
        num = name[len("fragment") :]
        obj_dir = os.path.join(args.build_dir, "asm", args.version, "data", "fragments", num)
        changed |= write_object(os.path.join(obj_dir, f"{name}_header.o"), ".text", header, args)
        changed |= write_object(os.path.join(obj_dir, f"{name}_reloc.o"), ".rodata", table, args)

    return 3 if changed else 0


def write_object(obj_path, section, payload, args):
    """Replace obj_path with an object whose section holds payload. True if it changed."""
    if read_object_section(obj_path, section, args) == payload:
        return False

    with tempfile.TemporaryDirectory() as tmp:
        bin_path = os.path.join(tmp, "payload.bin")
        s_path = os.path.join(tmp, "payload.s")
        with open(bin_path, "wb") as f:
            f.write(payload)
        flags = '"ax"' if section == ".text" else '"a"'
        with open(s_path, "w") as f:
            f.write(f'.section {section}, {flags}\n.incbin "{bin_path}"\n')
        subprocess.run([args.as_cmd, "-march=vr4300", "-32", "-G0", "-EB", "-o", obj_path, s_path], check=True)
    return True


def read_object_section(obj_path, section, args):
    if not os.path.exists(obj_path):
        return None
    with tempfile.TemporaryDirectory() as tmp:
        out = os.path.join(tmp, "section.bin")
        result = subprocess.run(
            [args.objcopy_cmd, "-O", "binary", "--only-section", section, obj_path, out], capture_output=True
        )
        if result.returncode != 0 or not os.path.exists(out):
            return None
        with open(out, "rb") as f:
            return f.read()


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("check", help="compare generated headers and tables with the linked ones")
    p.add_argument("elf")
    p.add_argument("lists", nargs="+", metavar="list")
    p.add_argument("--all", action="store_true", help="report every fragment, not just the listed ones")
    p.add_argument("-v", "--verbose", action="store_true")
    p.set_defaults(func=cmd_check)

    p = sub.add_parser("update", help="regenerate listed header/reloc objects; exit 3 if a relink is needed")
    p.add_argument("elf")
    p.add_argument("lists", nargs="+", metavar="list")
    p.add_argument("--build-dir", default="build")
    p.add_argument("--version", default="us")
    p.add_argument("--as", dest="as_cmd", default="mips-linux-gnu-as")
    p.add_argument("--objcopy", dest="objcopy_cmd", default="mips-linux-gnu-objcopy")
    p.set_defaults(func=cmd_update)

    args = parser.parse_args()
    sys.exit(args.func(args))


if __name__ == "__main__":
    main()
