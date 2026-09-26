#!/usr/bin/env python3
"""
Checks that the in-game randomizer builds the same teams as the website for the same
seed and settings, by running both on a set of cases and comparing.

    tools/randomizer/parity/check.py PATH_TO_WEBSITE_REPO [--seeds N] [--as-is]

Every cup, moveset style and combination of options is tried with N seeds (default 20).
By default the website is run with --fix-hyphens (see website.js), since the game can't
hold the moves that bug lets through; --as-is runs it unchanged to see what differs.
"""

import argparse
import itertools
import json
import os
import random
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "..", "..", "..", "src", "fragments", "61")

CUPS = ["poke", "petit", "pika", "prime"]
MOVESETS = ["legal", "stadium", "chaos"]


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("website")
    parser.add_argument("--seeds", type=int, default=20)
    parser.add_argument("--as-is", action="store_true", help="run the website without --fix-hyphens")
    args = parser.parse_args()

    rng = random.Random(1234)
    cases = []
    for cup, moveset, flags in itertools.product(CUPS, MOVESETS, range(64)):
        for _ in range(args.seeds):
            cases.append(f"{cup} {moveset} {flags} {rng.getrandbits(32)}")
    stdin = "\n".join(cases) + "\n"

    with tempfile.TemporaryDirectory() as tmp:
        host = os.path.join(tmp, "host")
        subprocess.run(
            ["gcc", "-std=gnu89", "-O2", "-Wall", "-DRANDOMIZER", "-DRANDOMIZER_HOST", "-I", SRC, "-o", host,
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
