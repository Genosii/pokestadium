#!/usr/bin/env python3
"""
Generate src/fragments/61/randomizer_data.c from the data of the Pokemon Stadium
random team generator website (github.com/Genosii/pokemon-stadium-random-team-generator).

Only Stadium 1 (Gen 1) data is kept. Everything is emitted in the order the website
walks it, so that with the same seed the in-game randomizer consumes random numbers
the same way the website does:
  - species in dex order, which is also the order of the website's pools;
  - each learnset in the website's key order, keeping the moves its Stadium 1 filter
    keeps (with tradeback moves; each move is flagged if it's also learnable in Gen 1);
  - the "Chaos" move pool in move_ids.json order.

Moves get the Gen 1 values of json/gen1_move_overrides.json where the website's data has
Gen 2 ones (Karate Chop is a Normal move, Dig has 100 power, ...), as the website does in
its Stadium 1 modes.

Moves the N64 game can't hold (ids above 165) are dropped and listed on stderr.

The tables are written twice: as C for the PC build that checks the generator against
the website (tools/randomizer/parity), and Yay0-compressed for the ROM, as the bytes of
randomizer_logic.h's RandomizerData, which the game unpacks when it loads the randomizer.
Needs crunch64 (in the repo's .venv, from requirements.txt):

    .venv/bin/python3 tools/randomizer/gen_data.py PATH_TO_WEBSITE_REPO
"""

import json
import os
import re
import sys

import crunch64

GEN1_MAX_MOVE_ID = 165
NUM_SPECIES = 151
LOGIC_H = os.path.join(os.path.dirname(__file__), "..", "..", "src", "fragments", "61", "randomizer_logic.h")


def logic_defines():
    """randomizer_logic.h's numeric #defines: flag bits and table sizes."""
    with open(LOGIC_H) as f:
        text = f.read()
    values = {}
    for name, value in re.findall(r"#define (RANDOMIZER_\w+) \(1 << (\d+)\)", text):
        values[name] = 1 << int(value)
    for name, value in re.findall(r"#define (RANDOMIZER_NUM_\w+) (\d+)\b", text):
        values[name] = int(value)
    return values


def js_array(js, name):
    m = re.search(name + r"\s*=\s*\[(.*?)\]", js, re.S)
    if m is None:
        sys.exit(f"can't find {name} in randomize.js")
    return re.findall(r"""["']([^"']+)["']""", m.group(1))


def move_key(move):
    # moveKey() / the Stadium 1 filter on the website: lowercase, whitespace removed
    return re.sub(r"\s+", "", move.lower())


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    web = sys.argv[1]
    out_path = os.path.join(os.path.dirname(__file__), "..", "..", "src", "fragments", "61", "randomizer_data.c")

    def load(name):
        with open(os.path.join(web, "json", name)) as f:
            return json.load(f)

    pokemon = load("pokemon_data.json")
    species = load("pokemon_species.json")
    move_ids = load("move_ids.json")
    gen1_moves = {key: fields for key, fields in load("gen1_move_overrides.json").items() if not key.startswith("_")}
    with open(os.path.join(web, "js", "randomize.js")) as f:
        js = f.read()

    petit = set(js_array(js, "PETIT_CUP_POKEMON"))
    pika = set(js_array(js, "PIKA_CUP_POKEMON"))
    exclusions = {move_key(m) for m in js_array(js, "GEN2_MOVE_EXCLUSIONS")}
    good_support = set(js_array(js, "GOOD_SUPPORT_MOVES"))
    unreliable = set(js_array(js, "UNRELIABLE_MOVES"))
    legendaries = set(js_array(js, "LEGENDARIES"))

    by_dex = {}
    for name, data in pokemon.items():
        if 1 <= data.get("dex", 0) <= NUM_SPECIES:
            by_dex[data["dex"]] = name
    if len(by_dex) != NUM_SPECIES:
        sys.exit(f"expected {NUM_SPECIES} Gen 1 species, found {len(by_dex)}")

    dropped = {}
    species_rows = []
    learnset = []
    for dex in range(1, NUM_SPECIES + 1):
        name = by_dex[dex]
        sp = species[name]

        # getMinLegalLevel()
        evo = sp.get("minLevel") or 1
        wild = sp.get("minWildLevel")
        min_level = min(evo, wild) if wild else evo

        flags = []
        if sp.get("isFinalEvo"):
            flags.append("RANDOMIZER_SPECIES_FINAL_EVO")
        if name in legendaries:
            flags.append("RANDOMIZER_SPECIES_LEGENDARY")
        if (sp.get("attack") or 0) >= (sp.get("spAttack") or 0):
            flags.append("RANDOMIZER_SPECIES_PHYSICAL")
        if name in petit:
            flags.append("RANDOMIZER_SPECIES_PETIT_CUP")
        if name in pika:
            flags.append("RANDOMIZER_SPECIES_PIKA_CUP")
        # getRandomGender() rolls once unless the species has a fixed gender
        if sp.get("genderRatio") not in (None, 0, 254, 255):
            flags.append("RANDOMIZER_SPECIES_GENDER_ROLL")

        start = len(learnset)
        for move, methods in pokemon[name]["learnset"].items():
            key = move_key(move)
            if key in exclusions or not isinstance(methods, list):
                continue
            if not any(re.match(r"^[12][A-Z]", m) for m in methods):
                continue
            info = move_ids.get(key)
            if info is None or not info.get("id") or info["id"] > GEN1_MAX_MOVE_ID:
                dropped.setdefault(key, []).append(name)
                continue
            gen1 = any(re.match(r"^1[A-Z]", m) for m in methods)
            learnset.append((info["id"], gen1, key))

        species_rows.append((dex, name, sp["type1"], sp["type2"], min_level, flags, start, len(learnset) - start))

    moves = {}
    chaos = []
    for key, info in move_ids.items():
        mid = info.get("id")
        if not mid or mid > GEN1_MAX_MOVE_ID:
            continue
        # moveInfo() looks moves up by name; aliases share an id and must agree
        info = dict(info, **gen1_moves.get(key, {}))
        flags = []
        if key in good_support:
            flags.append("RANDOMIZER_MOVE_GOOD_SUPPORT")
        if key in unreliable:
            flags.append("RANDOMIZER_MOVE_UNRELIABLE")
        row = (info["type"], info["power"], info["accuracy"], " | ".join(flags) or "0")
        if mid in moves and moves[mid][0] != row:
            sys.exit(f"move {mid} has conflicting data for {moves[mid][1]} and {key}")
        if mid not in moves:
            moves[mid] = (row, key)
            chaos.append((mid, key))

    for key, names in sorted(dropped.items()):
        print(f"dropped {key} (not a Gen 1 move) from {len(names)} species", file=sys.stderr)

    # The ROM gets the tables as the bytes of a RandomizerData (randomizer_logic.h),
    # Yay0-compressed; the PC build that checks the generator gets them as C
    defines = logic_defines()
    if len(chaos) != defines["RANDOMIZER_NUM_CHAOS_MOVES"]:
        sys.exit(f"{len(chaos)} chaos moves: update RANDOMIZER_NUM_CHAOS_MOVES in randomizer_logic.h")
    if len(learnset) != defines["RANDOMIZER_NUM_LEARNSET_MOVES"]:
        sys.exit(f"{len(learnset)} learnset moves: update RANDOMIZER_NUM_LEARNSET_MOVES in randomizer_logic.h")
    packed = bytearray()
    for dex, name, t1, t2, min_level, flags, start, count in [(0, "", 0, 0, 0, [], 0, 0)] + species_rows:
        packed += bytes([t1, t2, min_level, sum(defines[f] for f in flags)])
        packed += start.to_bytes(2, "big") + bytes([count, 0])
    for mid, gen1, _key in learnset:
        packed += bytes([mid, int(gen1)])
    packed += bytes(4)  # move 0, none
    for mid in range(1, GEN1_MAX_MOVE_ID + 1):
        if mid not in moves:
            sys.exit(f"no data for move {mid}")
        (mtype, power, accuracy, flags), key = moves[mid]
        packed += bytes([mtype, power, accuracy, sum(defines[f] for f in flags.split(" | ") if f != "0")])
    packed += bytes(mid for mid, _key in chaos)
    packed += bytes(len(packed) % 2)  # the structure's size is a multiple of its u16s'
    compressed = crunch64.yay0.compress(bytes(packed))
    compressed += bytes(-len(compressed) % 4)

    lines = [
        "/*",
        " * Generated by tools/randomizer/gen_data.py from the random team generator",
        " * website's data. Do not edit by hand.",
        " */",
        '#include "randomizer_logic.h"',
        "",
        "#ifdef RANDOMIZER",
        "",
        "#ifdef RANDOMIZER_HOST",
        "",
        "// The tables as C, for the PC build that checks the generator against the website",
        "// (tools/randomizer/parity). The ROM has the same bytes, compressed (below).",
        "const RandomizerData gRandomizerData = {",
        "    {",
        "        { 0 },",
    ]
    for dex, name, t1, t2, min_level, flags, start, count in species_rows:
        flag_str = " | ".join(flags) if flags else "0"
        lines.append(f"        /* {dex:3} {name:<11} */ {{ {t1}, {t2}, {min_level}, {flag_str}, {start}, {count} }},")
    lines += ["    },", "    {"]

    for dex, name, *_rest, start, count in species_rows:
        entries = ", ".join(f"{{ {mid}, {int(gen1)} }}" for mid, gen1, _key in learnset[start : start + count])
        lines.append(f"        /* {name} */ {entries},")
    lines += ["    },", "    {", "        { 0 },"]

    for mid in range(1, GEN1_MAX_MOVE_ID + 1):
        (mtype, power, accuracy, flags), key = moves[mid]
        lines.append(f"        /* {mid:3} {key:<13} */ {{ {mtype}, {power}, {accuracy}, {flags} }},")
    lines += ["    },", "    {"]

    for i in range(0, len(chaos), 12):
        lines.append("        " + ", ".join(str(mid) for mid, _key in chaos[i : i + 12]) + ",")
    lines += [
        "    },",
        "};",
        "",
        "#else",
        "",
        '#include "functions.h"',
        "",
        "// Unpacked from sRandomizerDataYay0 by Randomizer_UnpackData",
        "RandomizerData gRandomizerData;",
        "",
        "// The size the tables were packed for: if the structures change, gen_data.py must too",
        f"typedef char RandomizerDataSize[(sizeof(RandomizerData) == {len(packed)}) ? 1 : -1];",
        "",
        "// The tables above, Yay0-compressed, since the room the randomizer has in the ROM is",
        "// limited (u32s, so it's 4-byte aligned for Yay0_Decompress)",
        "static u32 sRandomizerDataYay0[] = {",
    ]
    words = [int.from_bytes(compressed[i : i + 4], "big") for i in range(0, len(compressed), 4)]
    for i in range(0, len(words), 8):
        lines.append("    " + ", ".join(f"0x{w:08X}" for w in words[i : i + 8]) + ",")
    lines += [
        "};",
        "",
        "void Randomizer_UnpackData(void) {",
        "    Yay0_Decompress(sRandomizerDataYay0, &gRandomizerData);",
        "}",
        "",
        "#endif",
        "",
        "#endif",
        "",
    ]

    with open(out_path, "w") as f:
        f.write("\n".join(lines))

    print(
        f"wrote {out_path}: {NUM_SPECIES} species, {len(learnset)} learnset moves, {len(chaos)} chaos moves; "
        f"{len(packed)} bytes, {len(compressed)} compressed"
    )


if __name__ == "__main__":
    main()
