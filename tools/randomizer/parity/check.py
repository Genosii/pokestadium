#!/usr/bin/env python3
"""
Checks that the in-game randomizer builds the same teams as the website for the same
seed and settings, by running both on a set of cases and comparing.

    tools/randomizer/parity/check.py PATH_TO_WEBSITE_REPO [--seeds N] [--as-is]

Every cup, moveset style, DV and stat exp source and combination of the other options is
tried with N seeds (default 20). The in-game side gets the rental Pokemon ("Stadium"
options) from the game's own tables in assets/us/898000 (`make init` extracts them), the
website from its json/s1_rentals.json (tools/randomizer/gen_rentals.py), so this also
checks that the two agree.
By default the website is run with --fix-hyphens (see website.js), since the game can't
hold the moves that bug lets through; --as-is runs it unchanged to see what differs.
"""

import argparse
import itertools
import json
import os
import random
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "..", "..", "..", "src", "fragments", "61")
ASSETS = os.path.join(HERE, "..", "..", "..", "assets", "us", "898000")

CUPS = ["poke", "petit", "pika", "prime"]
MOVESETS = ["stadium", "legal", "strong", "chaos"]
SOURCES = ["stadium", "max", "random"]
NUM_FLAGS = 32

# Each cup's rental table (func_84203C90), in CUPS order
RENTAL_TABLES = [0x1C, 0x17, 0x18, 0x19]
NUM_SPECIES = 151
MON_SIZE = 0x54


def rentals_header():
    """rentals.h for host.c: each cup's rental Pokemon, by species"""
    lines = [f"static const HostRental sRentals[{len(RENTAL_TABLES)}][{NUM_SPECIES + 1}] = {{"]
    for table in RENTAL_TABLES:
        with open(os.path.join(ASSETS, f"{table}.bin"), "rb") as f:
            data = f.read()
        (count,) = struct.unpack_from(">I", data, 0)
        rows = ["{ 0 }"] * (NUM_SPECIES + 1)
        for i in range(count):
            mon = data[4 + i * MON_SIZE : 4 + (i + 1) * MON_SIZE]
            dvs = struct.unpack_from(">H", mon, 0x1E)[0]
            moves = ", ".join(str(m) for m in mon[0x09:0x0D])
            stat_exp = ", ".join(str(v) for v in struct.unpack_from(">5H", mon, 0x14))
            rows[mon[0]] = (f"{{ 1, {{ {moves} }}, {{ {dvs >> 12 & 0xF}, {dvs >> 8 & 0xF}, {dvs >> 4 & 0xF}, "
                            f"{dvs & 0xF} }}, {{ {stat_exp} }} }}")
        lines.append("    {\n        " + ",\n        ".join(rows) + ",\n    },")
    lines.append("};")
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("website")
    parser.add_argument("--seeds", type=int, default=20)
    parser.add_argument("--as-is", action="store_true", help="run the website without --fix-hyphens")
    args = parser.parse_args()

    rng = random.Random(1234)
    cases = []
    for cup, moveset, dvs, stat_exp, flags in itertools.product(CUPS, MOVESETS, SOURCES, SOURCES, range(NUM_FLAGS)):
        for _ in range(args.seeds):
            cases.append(f"{cup} {moveset} {dvs} {stat_exp} {flags} {rng.getrandbits(32)}")
    stdin = "\n".join(cases) + "\n"

    with tempfile.TemporaryDirectory() as tmp:
        host = os.path.join(tmp, "host")
        with open(os.path.join(tmp, "rentals.h"), "w") as f:
            f.write(rentals_header())
        subprocess.run(
            ["gcc", "-std=gnu89", "-O2", "-Wall", "-DRANDOMIZER", "-DRANDOMIZER_HOST", "-I", SRC, "-I", tmp, "-o", host,
             os.path.join(HERE, "host.c"), os.path.join(SRC, "randomizer_logic.c"),
             os.path.join(SRC, "randomizer_data.c")],
            check=True,
        )
        ours = subprocess.run([host], input=stdin, capture_output=True, text=True, check=True).stdout.splitlines()

    website_cmd = ["node", os.path.join(HERE, "website.js"), args.website]
    if not args.as_is:
        website_cmd.append("--fix-hyphens")
    theirs = subprocess.run(website_cmd, input=stdin, capture_output=True, text=True, check=True).stdout.splitlines()

    if len(ours) != len(cases) or len(theirs) != len(cases):
        sys.exit(f"expected {len(cases)} results, got {len(ours)} in-game and {len(theirs)} from the website")

    mismatches = [(case, a, b) for case, a, b in zip(cases, ours, theirs) if json.loads(a) != json.loads(b)]
    print(f"{len(cases) - len(mismatches)}/{len(cases)} teams match")
    for case, a, b in mismatches[:5]:
        print(f"  {case}\n    in-game: {a}\n    website: {b}")
    return 1 if mismatches else 0


if __name__ == "__main__":
    sys.exit(main())
