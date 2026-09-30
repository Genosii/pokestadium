#!/usr/bin/env python3
"""
Pokemon Stadium Custom's mixed-case text. Rewrites the game's text archive (textdata) and
its trainers' and rental Pokemon (the archive at ROM 0x898000) from the Game Boy games'
capitals to mixed case, for RANDOMIZER=1 builds (the plain build keeps the originals):

    tools/randomizer/gen_text_case.py ASSETS_DIR OUT_DIR [--review FILE.md]

ASSETS_DIR is assets/us (from `make init`); OUT_DIR gets textdata.bin and 898000.bin.
Only letters change case, so every string keeps its length and both archives their size:
nothing else in the ROM moves. --review writes every string that changes, before and
after.

The rules:
  - Names (Pokemon, moves, items, types, stats, places, cups, people, trainer classes,
    nicknames) are capitalised word by word, and after a hyphen or a full stop:
    "Thunderpunch", "Double-Edge", "Mr.Mime", "Hall of Fame", "Poke Cup".
  - Abbreviations stay in capitals: HP, PP, OT, ID, TM, COM, GB, KO... and START, the
    button's name. So do words with digits (TM01, N64).
  - Text that's all in capitals otherwise, menu labels mostly, goes to sentence case:
    "DISPLAY ORDER" -> "Display order", with the names in it capitalised.
  - In sentences, other words in capitals stand for a menu item or a name ("choose
    CONTINUE") and are capitalised, except a few plain words (on, off, now, ball, stone).
  - The staff credits are left as they are.
"""

import argparse
import os
import re
import struct
import sys

# Files of the text archive
FILE_TRAINER_CLASSES = (34, 35)
FILE_GYMS = 25
FILE_SPECIES = 36
FILE_MOVES = 37
FILE_TYPES = 38
FILE_ITEMS = 39
FILE_CATEGORIES = 40
FILE_CREDITS = 45
# What each file of the text archive holds, for the review
TEXT_FILES = {
    0: "The PC", 1: "The PC", 2: "Boxes", 3: "Items", 4: "Boxes", 5: "Lists", 6: "Pokemon summaries",
    8: "The Pokedex", 9: "Pokedex areas", 10: "Pokedex entries", 11: "Move descriptions",
    12: "Item and TM descriptions", 13: "Game Boy connection messages", 14: "Modes and rounds",
    15: "Free Battle", 16: "Mode descriptions", 19: "Cups", 21: "Options", 23: "Mode descriptions",
    24: "Modes", 25: "Gyms", 26: "Rentals and entry", 27: "Modes and rounds", 28: "Surfing Pikachu",
    29: "Kids Club", 30: "Battle messages", 31: "Hall of Fame", 32: "Rules", 33: "Trade service",
    34: "Trainer classes", 35: "Trainer classes", 36: "Pokemon", 37: "Moves", 39: "Items",
    40: "Pokemon categories", 41: "Status", 44: "Album and gallery",
}
# Lists of names, capitalised throughout
NAME_FILES = {FILE_GYMS, *FILE_TRAINER_CLASSES, FILE_SPECIES, FILE_MOVES, FILE_ITEMS, FILE_CATEGORIES}
SKIP_FILES = {FILE_CREDITS}

ACRONYMS = {"HP", "PP", "OT", "ID", "TM", "HM", "PC", "COM", "GB", "KO", "HT", "WT", "OK", "VS", "XX", "START"}
PLAIN_WORDS = {"ON", "OFF", "NOW", "BALL", "STONE"}  # lowercase in sentences
SMALL_WORDS = {"OF", "THE", "AND", "TO", "IN"}  # lowercase inside a name

# Names that aren't in the name lists (files 25, 34 to 40), as they're written in the text
EXTRA_NAMES = """
POKéMON|POKé|POKéDEX|POKéMON STADIUM|STADIUM|GAME BOY|GAME PAK|TRANSFER PAK|BOX|PROF. OAK|OAK
POKé CUP|PETIT CUP|PIKA CUP|PRIME CUP|GYM LEADER CASTLE|GYM LEADER|GYM|FREE BATTLE|EVENT BATTLE
VS MEWTWO|ANYTHING GOES|HALL OF FAME|ELITE FOUR|RIVAL|CHAMPION|KIDS CLUB|GB TOWER|TRADE SERVICE
GALLERY|ALBUM|POKéMON LAB|POKéMON CENTER|POKéMON MART|POKéMON TOWER|POKéMON MANSION|SAFARI ZONE
SAFARI ZONE WARDEN|VIRIDIAN FOREST|MT. MOON|DIGLETT'S CAVE|ROCK TUNNEL|SEAFOAM ISLAND|VICTORY ROAD
POWER PLANT|UNKNOWN DUNGEON|CERULEAN CITY|S.S. ANNE|ROUTE|PALLET TOWN|INDIGO PLATEAU
PEWTER|CERULEAN|VERMILION|LAVENDER|CELADON|FUCHSIA|SAFFRON|CINNABAR|VIRIDIAN
BROCK|MISTY|SURGE|LT. SURGE|ERIKA|KOGA|SABRINA|BLAINE|GIOVANNI|GIOVANI|LORELEI|BRUNO|AGATHA|LANCE
MAGIKARP'S SPLASH|MAGIKARP SPLASH|CLEFAIRY SAYS|SNORE WAR|THUNDERING DYNAMO|SUSHI-GO-ROUND
HOOP HURL|ROCK HARDEN|ATTACK|DEFENSE|SPEED|SPECIAL|ACCURACY|EVASION|PWR
"""

# Whole strings that the rules get wrong
EXACT = {"RUN, RATTATA, RUN": "Run, Rattata, Run"}

TOKEN = re.compile(r"(?<![A-Za-z0-9é])[A-Z0-9][A-Z0-9é¾©.'\-]*")
SENTENCE_END = re.compile(r"[.!?][\"')]*\s+$")


def read_archive(data):
    """The files of an archive: a 16-byte header ending with their count, then an entry
    of 16 bytes (offset, size, 0, 0) for each."""
    count = struct.unpack(">I", data[12:16])[0]
    return [struct.unpack(">II", data[16 + i * 16 : 24 + i * 16]) for i in range(count)]


def text_strings(data, offset):
    """(offset in the archive, bytes) of each string of a text file."""
    count = struct.unpack(">I", data[offset : offset + 4])[0]
    out = []
    for i in range(count):
        start = offset + struct.unpack(">I", data[offset + 4 + i * 4 : offset + 8 + i * 4])[0]
        out.append((start, data[start : data.index(b"\0", start)]))
    return out


def title(word):
    """"MR.MIME" -> "Mr.Mime", "DOUBLE-EDGE" -> "Double-Edge", "FARFETCH'D" -> "Farfetch'd"."""
    out = []
    cap = True
    for ch in word:
        if ch in ".-":
            cap = True
        elif ch == "'":
            cap = False
        elif "A" <= ch <= "Z":
            ch = ch if cap else ch.lower()
            cap = False
        elif ch.isalpha():
            cap = False
        out.append(ch)
    return "".join(out)


def capitalise(word):
    """For words that aren't names: "SEMI-FINAL" -> "Semi-final"."""
    return word[0] + lower(word[1:])


def lower(word):
    return "".join(ch.lower() if "A" <= ch <= "Z" else ch for ch in word)


def split_word(token):
    """A token's word and what trails it: "OAK'" -> ("OAK", "'"), "HP." -> ("HP", ".")."""
    word = token.rstrip(".'-")
    return word, token[len(word) :]


def is_caps(word):
    return sum("A" <= ch <= "Z" for ch in word) >= 2 and not re.search("[a-z]", word)


class Converter:
    def __init__(self, names):
        self.names = {tuple(n.split(" ")) for n in names if n}
        self.longest = max(len(n) for n in self.names)

    def match_name(self, words, i):
        """How many words from words[i] make a name, longest first (0 if none)."""
        for n in range(min(self.longest, len(words) - i), 0, -1):
            if tuple(words[i : i + n]) in self.names:
                return n
        return 0

    def convert(self, text, mode):
        """mode: "names" (a list of names), "label" (text all in capitals) or "sentence"."""
        tokens = [m for m in TOKEN.finditer(text) if is_caps(split_word(m.group(0))[0])]
        out = list(text)
        # Runs of words in capitals separated by single spaces, matched against the names
        runs = []
        for m in tokens:
            if runs and text[runs[-1][-1].end() : m.start()] == " ":
                runs[-1].append(m)
            else:
                runs.append([m])
        for run in runs:
            words = [split_word(m.group(0))[0] for m in run]
            i = 0
            while i < len(words):
                n = self.match_name(words, i)
                for j in range(i, i + max(n, 1)):
                    m = run[j]
                    word = words[j]
                    start = m.start()
                    at_start = start == 0 or SENTENCE_END.search(text[:start]) is not None
                    if any(ch.isdigit() for ch in word) or word in ACRONYMS:
                        new = word
                    elif n or mode == "names":
                        new = lower(word) if (j > i and word in SMALL_WORDS) else title(word)
                    elif mode == "label":
                        new = capitalise(word) if at_start else lower(word)
                    else:
                        new = capitalise(word) if (at_start or word not in PLAIN_WORDS) else lower(word)
                    out[start : start + len(word)] = new
                i += max(n, 1)
        return "".join(out)


def names_from_text(data, entries):
    names = set(EXTRA_NAMES.replace("\n", "|").split("|"))
    for f in (FILE_SPECIES, FILE_MOVES, FILE_ITEMS, FILE_GYMS, *FILE_TRAINER_CLASSES):
        names |= {s.decode("latin-1") for _o, s in text_strings(data, entries[f][0])}
    names |= {s.decode("latin-1").upper() for _o, s in text_strings(data, entries[FILE_TYPES][0])}
    return {n for n in names if n and is_caps(n.replace(" ", ""))}


def convert_textdata(data, conv, review):
    data = bytearray(data)
    entries = read_archive(data)
    for f, (offset, _size) in enumerate(entries):
        if f in SKIP_FILES:
            continue
        for start, raw in text_strings(bytes(data), offset):
            text = raw.decode("latin-1")
            mode = "names" if f in NAME_FILES else ("label" if not re.search("[a-z]", text) else "sentence")
            new = EXACT.get(text) or conv.convert(text, mode)
            assert len(new) == len(text) and new.upper() == text.upper(), (text, new)
            if new != text:
                data[start : start + len(raw)] = new.encode("latin-1")
                review.append((TEXT_FILES.get(f, "Menus and messages"), text, new))
    return bytes(data)


# The trainer and rental archive: files of trainers (0x230 bytes each: name at 0x00 and
# 0x0C, then six Pokemon from 0x38) and of rental Pokemon (0x54 bytes each)
TRAINER_SIZE = 0x230
MON_SIZE = 0x54
MON_STRINGS = (0x30, 0x3B)  # nickname, original trainer
MON_OT_GB = 0x46  # the original trainer again, in the Game Boy's characters
GB_UPPER = range(0x80, 0x9A)  # A to Z; a to z are 0x20 further


def convert_string(data, at, length, conv, review, where):
    raw = bytes(data[at : at + length]).split(b"\0")[0]
    text = raw.decode("latin-1")
    new = conv.convert(text, "names")
    if new != text:
        data[at : at + len(raw)] = new.encode("latin-1")
        review.append((where, text, new))
    return text, new


def convert_mon(data, at, conv, review, where):
    convert_string(data, at + MON_STRINGS[0], 11, conv, review, where + " nickname")
    ot, new_ot = convert_string(data, at + MON_STRINGS[1], 11, conv, review, where + " OT")
    gb = at + MON_OT_GB
    for i, (a, b) in enumerate(zip(ot, new_ot)):
        if a != b and data[gb + i] in GB_UPPER:
            data[gb + i] += 0x20


def convert_trainers(data, conv, review):
    data = bytearray(data)
    for f, (offset, size) in enumerate(read_archive(data)):
        count = struct.unpack(">I", data[offset : offset + 4])[0]
        if 0 <= size - (4 + count * TRAINER_SIZE) < 16:
            for t in range(count):
                at = offset + 4 + t * TRAINER_SIZE
                name, _ = convert_string(data, at, 12, conv, review, f"trainers {f}")
                convert_string(data, at + 12, 12, conv, review, f"trainers {f}")
                for m in range(data[at + 0x37]):
                    convert_mon(data, at + 0x38 + m * MON_SIZE, conv, review, f"trainers {f} ({name})")
        elif 0 <= size - (4 + count * MON_SIZE) < 16:
            for m in range(count):
                convert_mon(data, offset + 4 + m * MON_SIZE, conv, review, f"rentals {f}")
        else:
            sys.exit(f"898000 file {f} is neither trainers nor rental Pokemon")
    return bytes(data)


def write_review(path, review):
    """Every change once, grouped by where it is, with how many times it's there."""
    groups = {}
    for where, before, after in review:
        key = where
        if where.startswith("trainers"):
            key = "Computer trainers and their Pokemon"
        elif where.startswith("rentals"):
            key = "Rental Pokemon"
        groups.setdefault(key, {}).setdefault((before, after), 0)
        groups[key][(before, after)] += 1
    with open(path, "w", encoding="utf-8") as f:
        f.write("# Pokemon Stadium Custom: text in mixed case\n\n")
        f.write(f"{len(review)} strings change. Each change is listed once per place, ")
        f.write("with a count when it's there more than once; `/` is a line break.\n")
        for key, changes in groups.items():
            f.write(f"\n## {key}\n\n| Before | After |\n|---|---|\n")
            for (before, after), n in changes.items():
                cell = lambda s: s.replace("\n", " / ").replace("|", "\\|").replace("¾", "♀").replace("©", "♂")
                f.write(f"| {cell(before)} | {cell(after)}{f' (x{n})' if n > 1 else ''} |\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("assets", help="assets/us")
    parser.add_argument("out", help="where textdata.bin and 898000.bin go")
    parser.add_argument("--review", help="also write every change to this Markdown file")
    args = parser.parse_args()

    with open(os.path.join(args.assets, "textdata.bin"), "rb") as f:
        textdata = f.read()
    with open(os.path.join(args.assets, "898000.bin"), "rb") as f:
        trainers = f.read()

    conv = Converter(names_from_text(textdata, read_archive(textdata)))
    review = []
    new_textdata = convert_textdata(textdata, conv, review)
    new_trainers = convert_trainers(trainers, conv, review)
    assert len(new_textdata) == len(textdata) and len(new_trainers) == len(trainers)

    os.makedirs(args.out, exist_ok=True)
    with open(os.path.join(args.out, "textdata.bin"), "wb") as f:
        f.write(new_textdata)
    with open(os.path.join(args.out, "898000.bin"), "wb") as f:
        f.write(new_trainers)
    if args.review:
        write_review(args.review, review)
    print(f"{args.out}: {len(review)} strings in mixed case")


if __name__ == "__main__":
    main()
