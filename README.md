# Pokemon Stadium (US)
A WIP decomp of Pokemon Stadium (US).

It builds the following ROMs:

* pokestadium.z64: `md5: ed1378bc12115f71209a77844965ba50`

Note: To use this repository, you must already have a rom for the game.

# Prerequisites

Under Debian / Ubuntu (which we recommend using), you can install them with the following commands:

```bash
sudo apt update
sudo apt install make git build-essential binutils-mips-linux-gnu python3 python3-pip python3-venv
```

**Please also ensure that the Python version installed is >3.7.**

The build process has a few python packages required that are located in `requirements.txt`.

To install them simply run in a terminal:

```bash
python3 -m pip install -r requirements.txt
```

# To use
1. Place the US Pokemon Stadium 1.0 rom into the repository's "/baseroms/us/" folder as "baserom.z64".
2. Set up tools and extract the rom: `make init`
3. Re-assemble the rom: `make`

# Pokemon Stadium Custom
`make RANDOMIZER=1` builds Pokemon Stadium Custom: the game with an in-game team randomizer,
made the way the
[random team generator website](https://github.com/Genosii/pokemon-stadium-random-team-generator)
makes teams, a teambuilder, new modes and a clearer interface. That ROM no longer matches, so
the MD5 check is skipped; a plain `make` still builds the original. (The build option and the
code keep the name they started with, the randomizer.)

- Pokemon pick screen: Z fills all six entry slots with a random team for the current cup.
  C-Up opens the options and shows the last team's seed, which gives the same team on the
  website with the same cup and options. A website seed can be typed in there too, for the
  next team. The options:
  - Moveset: Stadium (standard: the game's own rental Pokemon's moves), Legal (four random
    moves from the learnset), Strong (for each of its types, one of the Pokemon's strongest
    moves of that type, then strong coverage and a support move) or Chaos (any move).
  - DVs and Stat Exp: Stadium (standard: the rentals' own), Max or Random.
  - Tradeback moves, Legendaries, Final evos only, Monotype, Shared types, and Auto battle
    pick.

  Teams use Gen 1 move data (Karate Chop, Gust, Sand-Attack and Bite are Normal moves, Dig
  has 100 power, ...), from the game. The moves of every Pokemon in the entry box show next
  to the OK / Reselect menu once the team is complete, and whenever C-Down is held. A line
  under the list shows these buttons.
- Teambuilder: once a team is complete, "Edit Pokemon" in its menu (next to OK and
  Reselect, when entering a cup and in Registration) opens a window to change each
  Pokemon's moves, level, DVs and stat exp. Moves are picked from a list of the ones the
  Pokemon can legally learn (tradeback-only ones marked, and left out with "Tradeback
  moves" off), with their type, power and accuracy; levels stay within the cup's range;
  stats update as they change. L/R go from Pokemon to Pokemon, A sets a DV or stat exp to
  its highest (or lowest), Z every one to the highest. "OK to Register" saves the team as
  edited, and in "Check registered Pokemon", Z edits a registered team and saves it over
  itself.
- Opponents: C-Right on the pick screen opens the opponents' options (the same moveset,
  stat and pool options). With "Random opponents" on, the computer trainers of the cups
  and the Gym Leader Castle get random teams at their own levels; their "Stadium" DVs and
  stat exp are the trainer's own, which grow from ball to ball and gym to gym. Each
  trainer's team follows from the run's seed (your team's, when it was made with Z), so a
  trainer faced again has the same team and a shared seed gives the same whole run.
- Modes, the first row of the opponents' options:
  - Factory: random opponents, and after each win in a cup or the Gym Leader Castle (but
    the last), a panel shows the three Pokemon the trainer battled with next to your team.
    Take one (A) and pick which of yours it replaces, or keep your team (B); the menu that
    follows then has a "Swap a Pokemon" line in case you change your mind. The Pokemon
    keeps its level, DVs, stat exp and moves, so later trainers' Pokemon are worth more.
  - Rogue: Factory, with Gym Leaders and the Elite Four given teams of their type (Brock
    Rock, Misty Water, ..., Lance Dragon; a second type makes up six where Gen 1 has too
    few), and no retries: a loss ends the run.
- Battle-select screen: Z picks a random three that fit the cup's level-sum rule, or it
  happens by itself with "Auto battle pick" on. The footer shows the Z button next to L and R.
- Battle: A shows your Pokemon's moves next to the move menu and B the party, in one window
  with each Pokemon's HP, status, moves and stats, so R isn't needed to check them. A bar
  shows "L Cancel", since B no longer backs out of these menus. Against the computer, the
  party box R shows is that window too; two-player battles keep R.
- Intro: the Pokemon in its first four scenes are random, a new set every boot (counted in
  the save file, so emulators get one too): ones of about the same size on the ground,
  fliers in the sky and swimmers underwater. The last scene, with Pikachu, Psyduck,
  Clefairy and Jigglypuff, stays as it is.
- Title screen: "Custom" under the "Pokemon Stadium" logo, fading in with the screen
  (`tools/randomizer/gen_title_subtitle.py` draws it from text, or takes finished artwork).
- Text in mixed case instead of the Game Boy games' capitals, everywhere: Pokemon, moves,
  items, trainers and nicknames ("Bulbasaur", "Karate Chop", "Bug Boy"), menus in sentence
  case ("Delete saved data"), abbreviations kept (HP, PP, OT, COM). Every string keeps its
  length (`tools/randomizer/gen_text_case.py`, run by the build). Pokemon from a Game Boy
  cartridge keep the names they have there, and text that's part of a picture ("PRESS
  START") stays as it is.
- Options (after the title screen) has a "Randomizer" line, showing the mode, which opens
  the same options panels. Rules (in the cups, the Gym Leader Castle and the other modes)
  opens them with Z, as its bottom bar shows; the game's own rules stay as they are. On
  every panel, two lines under the options tell what the one picked does.

The options are saved whenever a panel closes with them changed, in bytes of the save file
that the game writes but never uses (after the last section of its third bank), with a
checksum of their own; a save file without them gives the defaults, and the game's own data
isn't touched. The randomizer's code
goes in the free space at the end of the ROM (`linker_scripts/us/randomizer.ld`) as fragments
of its own that the screens it changes load, and nothing of the original game moves, so the first
megabyte of the ROM and the checksum in its header are the original's. Emulators that
recognise games by that checksum, like Project64, then use their Pokemon Stadium settings.

The code is in `src/fragments/61/randomizer*.c` (`randomizer_menu.c` and `randomizer_panel.c`
are the options panels and the settings, shared by the screens that show them),
`src/fragments/64/randomizer*.c`, `src/fragments/63/randomizer_cup*`,
`src/fragments/62/randomizer_battle_ui*`, `src/fragments/56/randomizer_options*`,
`src/fragments/55/randomizer_rules*`, `src/fragments/36/randomizer_title*` and
`src/fragments/17/randomizer_intro*`; `src/randomizer_state.h` and `src/randomizer_save.h`
are what they keep in memory and in the save file.
`randomizer_data.c` is generated from the website's data with
`.venv/bin/python3 tools/randomizer/gen_data.py PATH_TO_WEBSITE`, and the website's rental data from the
game's own with `tools/randomizer/gen_rentals.py PATH_TO_WEBSITE`;
`tools/randomizer/parity/check.py PATH_TO_WEBSITE` checks that the port builds the same teams
as the website for the same seeds (it needs gcc and Node.js).

[docs/](docs/README.md) has notes on how the game works inside (fragments, the save file,
the Pokemon structures, the screens the randomizer changes) and on how the randomizer is
built, for anyone extending it, recompiling the game or doing the same for Stadium 2.

For contacts and other pret projects, see [pret.github.io](https://pret.github.io/).
