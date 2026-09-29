#!/usr/bin/env python3
"""
Write the Stadium 1 rental Pokemon of each cup to the random team generator website,
as json/s1_rentals.json, for its "Stadium" moveset, DV and stat exp options.

    tools/randomizer/gen_rentals.py PATH_TO_WEBSITE_REPO

The rentals come from the game's own tables (assets/us/898000, extracted by
`make init`), the ones the Pokemon pick screen lists (func_84203C90), so the website
and the in-game randomizer, which reads the same tables, give the same Pokemon the
same sets. For each cup, keyed by the website's species name:

    {"moves": [move keys], "dvs": [Attack, Defense, Speed, Special],
     "statExp": [HP, Attack, Defense, Speed, Special]}
"""

import json
import os
import struct
import sys

ASSETS = os.path.join(os.path.dirname(__file__), "..", "..", "assets", "us", "898000")

# The website's Stadium 1 modes and the game's rental table for each (func_84203C90,
# by rule set: Poke Cup 3, Petit Cup 4, Pika Cup 5, Prime Cup 6, Gym Leader Castle 7)
TABLES = {
    "s1Poke": 0x1C,
    "s1Petit": 0x17,
    "s1Pika": 0x18,
    "s1Prime": 0x19,
    "s1Gym": 0x1E,
}

MON_SIZE = 0x54  # unk_func_80026268_arg0


def rentals(table):
    with open(os.path.join(ASSETS, f"{table}.bin"), "rb") as f:
        data = f.read()
    (count,) = struct.unpack_from(">I", data, 0)
    for i in range(count):
        mon = data[4 + i * MON_SIZE : 4 + (i + 1) * MON_SIZE]
        species = mon[0x00]
        moves = [m for m in mon[0x09:0x0D] if m != 0]
        dvs = struct.unpack_from(">H", mon, 0x1E)[0]
        stat_exp = list(struct.unpack_from(">5H", mon, 0x14))
        yield species, moves, [(dvs >> 12) & 0xF, (dvs >> 8) & 0xF, (dvs >> 4) & 0xF, dvs & 0xF], stat_exp


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    web = sys.argv[1]

    def load(name):
        with open(os.path.join(web, "json", name)) as f:
            return json.load(f)

    pokemon = load("pokemon_data.json")
    move_ids = load("move_ids.json")
    move_names = load("pokemon_moves.json")["moveTypes"]

    name_of = {info["dex"]: name for name, info in pokemon.items() if "dex" in info}
    # move_ids.json has a few spellings of some moves; use the one the website shows
    key_of = {}
    for key, info in move_ids.items():
        if info["id"] not in key_of or key in move_names:
            key_of[info["id"]] = key

    out = {}
    for mode, table in TABLES.items():
        cup = {}
        for species, moves, dvs, stat_exp in rentals(table):
            cup[name_of[species]] = {"moves": [key_of[m] for m in moves], "dvs": dvs, "statExp": stat_exp}
        out[mode] = cup

    path = os.path.join(web, "json", "s1_rentals.json")
    with open(path, "w") as f:
        json.dump(out, f, separators=(",", ":"), sort_keys=False)
        f.write("\n")
    print(f"wrote {path}: " + ", ".join(f"{mode} {len(cup)}" for mode, cup in out.items()))


if __name__ == "__main__":
    main()
