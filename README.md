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

# Randomizer
`make RANDOMIZER=1` builds the ROM with an in-game team randomizer, made the way the
[random team generator website](https://github.com/Genosii/pokemon-stadium-random-team-generator)
makes teams. That ROM no longer matches, so the MD5 check is skipped; a plain `make` still builds
the original.

- Pokemon pick screen: Z fills all six entry slots with a random team for the current cup.
  C-Up opens the options (moveset style, tradeback moves, random DVs and stat exp, pool
  filters, auto battle pick) and shows the last team's seed, which gives the same team on
  the website with the same cup and options. A website seed can be typed in there too, for
  the next team. The moves of every Pokemon in the entry box show next to the OK / Reselect
  menu once the team is complete, and whenever C-Down is held. A line under the list shows
  these buttons.
- Battle-select screen: Z picks a random three that fit the cup's level-sum rule, or it
  happens by itself with "Auto battle pick" on. The footer shows the Z button next to L and R.
- Battle: the party box shown while R is held in the Pokemon menu gets a panel with each
  Pokemon's moves.

The options last until the console is switched off (Reset keeps them). The randomizer's code
goes in the free space at the end of the ROM (`linker_scripts/us/randomizer.ld`) as fragments
of its own that the two screens load, and nothing of the original game moves, so the first
megabyte of the ROM and the checksum in its header are the original's. Emulators that
recognise games by that checksum, like Project64, then use their Pokemon Stadium settings.

The code is in `src/fragments/61/randomizer*.c`, `src/fragments/64/randomizer_battle.c` and
`src/fragments/62/randomizer_battle_ui*`.
`randomizer_data.c` is generated from the website's data with
`tools/randomizer/gen_data.py PATH_TO_WEBSITE`, and
`tools/randomizer/parity/check.py PATH_TO_WEBSITE` checks that the port builds the same teams
as the website for the same seeds (it needs gcc and Node.js).

For contacts and other pret projects, see [pret.github.io](https://pret.github.io/).
