#!/usr/bin/env node
/*
 * Runs the random team generator website's own code (js/randomize.js and
 * js/savefile.js) in Node, for comparing with the in-game randomizer. Takes the same
 * cases on stdin as host.c and prints teams in the same format.
 *
 *   website.js PATH_TO_WEBSITE_REPO [--fix-hyphens]
 *
 * --fix-hyphens also excludes Mud-Slap and Lock-On, which the website means to drop
 * from Stadium 1 movesets but misses because of the hyphen in their names. The game
 * can't hold either move, so this is what the in-game port does.
 */
'use strict';

const fs = require('fs');
const path = require('path');
const readline = require('readline');
const vm = require('vm');

const web = process.argv[2];
const fixHyphens = process.argv.includes('--fix-hyphens');

// Just enough of a browser for the scripts to load and run without a page
function element() {
  return {
    value: '', innerHTML: '', textContent: '', checked: false, style: {}, dataset: {},
    classList: { add() {}, remove() {}, toggle() {}, contains() { return false; } },
    addEventListener() {}, removeEventListener() {}, appendChild() {}, removeChild() {},
    setAttribute() {}, getAttribute() { return null; }, focus() {}, click() {},
    querySelector() { return element(); }, querySelectorAll() { return []; },
  };
}

const context = vm.createContext({
  document: {
    addEventListener() {}, getElementById() { return element(); }, createElement() { return element(); },
    querySelector() { return element(); }, querySelectorAll() { return []; }, body: element(),
  },
  window: { location: { href: 'http://localhost/', search: '' }, addEventListener() {} },
  localStorage: { getItem() { return null; }, setItem() {} },
  console: { log() {}, warn() {}, error() {} },
  setTimeout() {}, clearTimeout() {}, fetch() { return new Promise(() => {}); },
  URL, URLSearchParams,
});

for (const file of ['js/randomize.js', 'js/savefile.js']) {
  vm.runInContext(fs.readFileSync(path.join(web, file), 'utf8'), context, { filename: file });
}

const json = name => JSON.parse(fs.readFileSync(path.join(web, 'json', name), 'utf8'));
context.__data = {
  pokemon: json('pokemon_data.json'),
  moves: json('pokemon_moves.json'),
  species: json('pokemon_species.json'),
  moveIds: json('move_ids.json'),
  rentals: json('s1_rentals.json'),
  gen1: json('gen1_move_overrides.json'),
};

vm.runInContext(`
  pokemonData = __data.pokemon;
  moveData = __data.moves;
  speciesData = __data.species;
  moveIdData = __data.moveIds;
  s1Rentals = __data.rentals;
  gen1MoveOverrides = __data.gen1;
  renderTeam = function () {};
  ${fixHyphens ? "GEN2_MOVE_EXCLUSIONS.push('mudslap', 'lockon');" : ''}

  function __run(mode, moveset, dvs, statExp, flags, seed) {
    currentMode = mode;
    movesetMode = moveset;
    dvMode = dvs;
    statExpMode = statExp;
    noTradebackMoves = (flags & 1) !== 0;
    noLegendaries = (flags & 2) !== 0;
    finalEvosOnly = (flags & 4) !== 0;
    monoTypeTeam = (flags & 8) !== 0;
    noSharedTypes = (flags & 16) !== 0;
    currentFixedTeam = [];

    pendingSeed = seed;
    randomizeFullTeam();
    const team = currentFixedTeam;
    if (team.length !== 6) return null;

    // Exporting the team is what rolls stat exp (and a trainer ID before it)
    const save = buildGen1Save(team);
    return team.map((mon, i) => {
      const moves = mon.moves.map(m => {
        const key = moveKey(m);
        // Struggle is only padding outside Chaos; the game leaves the slot empty
        if (key === 'struggle' && moveset !== 'chaos') return 0;
        return moveIdData[key].id;
      });
      const base = 0x2F34 + 44 * i;
      const statExp = [0, 1, 2, 3, 4].map(s => (save[base + 0x11 + 2 * s] << 8) | save[base + 0x12 + 2 * s]);
      return [pokemonData[mon.name].dex, mon.level, moves,
              [mon.dvs.atk, mon.dvs.def, mon.dvs.spd, mon.dvs.spc], statExp];
    });
  }
`, context);

const modes = { poke: 's1Poke', petit: 's1Petit', pika: 's1Pika', prime: 's1Prime' };

const lines = readline.createInterface({ input: process.stdin });
lines.on('line', line => {
  const [cup, moveset, dvs, statExp, flags, seed] = line.trim().split(/\s+/);
  if (!cup) return;
  const team = context.__run(modes[cup], moveset, dvs, statExp, Number(flags), Number(seed) >>> 0);
  process.stdout.write(JSON.stringify(team) + '\n');
});
