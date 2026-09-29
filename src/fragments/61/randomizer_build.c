/*
 * The randomizer's core (randomizer_build.h): the rules each mode's teams follow, and
 * building the game's Pokemon from the generator's teams. Used by the pick screen for
 * the player's team and by the battle-select screen for random opponents. Built only
 * with RANDOMIZER=1; empty otherwise so the default build still matches.
 */
#include "randomizer_build.h"

#ifdef RANDOMIZER

#include "src/22630.h"
#include "src/232C0.h"
#include "src/hal_libc.h"

#define NICKNAME_LENGTH 10
#define MAX_PP_UPS (3 << 6)

// The fragment's entry point, which its header needs; nothing calls it
void Randomizer_CoreEntry(void) {
}

/*
 * The rules for a mode, from the rule set index it chose (D_800AE540.unk_0001). Level
 * ranges and caps are the ones the pick screen checks itself (func_8420ACA8,
 * func_84206A68); pools and levels follow the website's cups. Modes that allow any
 * level get anyLevel.
 */
void Randomizer_GetRules(s32 ruleSet, s32 anyLevel, RandomizerRules* rules) {
    rules->level = 0;
    rules->levelMin = 0;
    rules->levelMax = 0;
    rules->levelSum = 0;
    rules->theme = 0;
    rules->theme2 = 0;
    rules->rental = NULL;

    switch (ruleSet) {
        case 3: // Poke Cup
            rules->cup = RANDOMIZER_CUP_POKE;
            rules->levelMin = 50;
            rules->levelMax = 55;
            rules->levelSum = 155;
            break;

        case 1:
        case 2: // the other level 50-55 modes, which have no level-sum rule
            rules->cup = RANDOMIZER_CUP_POKE;
            rules->levelMin = 50;
            rules->levelMax = 55;
            break;

        case 4: // Petit Cup
            rules->cup = RANDOMIZER_CUP_PETIT;
            rules->levelMin = 25;
            rules->levelMax = 30;
            rules->levelSum = 80;
            break;

        case 5: // Pika Cup
            rules->cup = RANDOMIZER_CUP_PIKA;
            rules->levelMin = 15;
            rules->levelMax = 20;
            rules->levelSum = 50;
            break;

        case 6: // Prime Cup
            rules->cup = RANDOMIZER_CUP_PRIME;
            rules->level = 100;
            break;

        default:
            rules->cup = RANDOMIZER_CUP_PRIME;
            rules->level = anyLevel;
            break;
    }
}

/*
 * Builds a Pokemon from scratch: species data from the game's own tables, experience
 * for the level, and PP Ups on every move; func_80022734 then works out the level,
 * stats, HP and PP. The trainer name and ID are copied from trainer, one of the
 * Pokemon of the trainer it's for (not mon itself, which is cleared first).
 */
void Randomizer_BuildPokemon(unk_func_80026268_arg0* mon, const RandomizerMon* src,
                             const unk_func_80026268_arg0* trainer) {
    char name[0x20];
    s32 i;

    bzero(mon, sizeof(*mon));

    mon->unk_00.unk_00 = src->species;
    // Accessed through D_80070F84 the way CalculateStatValue does, the species' types
    // follow its base stats (Bulbasaur: grass, poison)
    mon->unk_06 = D_80070F84[src->species].unk0B[0];
    mon->unk_07 = D_80070F84[src->species].unk0B[1];
    for (i = 0; i < 4; i++) {
        mon->unk_09[i] = src->moves[i];
        mon->unk_20[i] = (src->moves[i] != 0) ? MAX_PP_UPS : 0;
    }
    mon->unk_0E = trainer->unk_0E;
    mon->unk_10 = func_800224B8(src->species, src->level);
    mon->unk_14 = src->statExp[0];
    mon->unk_16 = src->statExp[1];
    mon->unk_18 = src->statExp[2];
    mon->unk_1A = src->statExp[3];
    mon->unk_1C = src->statExp[4];
    mon->unk_1E = (src->dvs[0] << 12) | (src->dvs[1] << 8) | (src->dvs[2] << 4) | src->dvs[3];
    func_80022734(mon);

    func_80021CA4(name, src->species);
    for (i = 0; (i < NICKNAME_LENGTH) && (name[i] != '\0'); i++) {
        mon->unk_30[i] = name[i];
    }
    _bcopy(trainer->unk_3B, mon->unk_3B, sizeof(mon->unk_3B));
    func_800228B0(mon);
}

/*
 * The rental Pokemon of the current mode, for the "Stadium" moveset, DV and stat exp
 * options: the file of the trainer archive at ROM 0x898000 that the pick screen lists
 * (func_84203C90, by rule set; Round 2 has its own), or -1 for none.
 */
s32 Randomizer_RentalTable(void) {
    s32 table;

    switch (D_800AE540.unk_0001) {
        case 0:
        case 1:
            table = 0x1A;
            break;
        case 2:
            table = 0x1B;
            break;
        case 3:
            table = 0x1C;
            break;
        case 4:
            table = 0x17;
            break;
        case 5:
            table = 0x18;
            break;
        case 6:
        case 8:
            table = 0x19;
            break;
        case 7:
            table = 0x1E;
            break;
        default:
            return -1;
    }
    if (D_800AE540.unk_11F2 != 0) {
        table += 0x1F;
    }
    return table;
}

static const RandomizerRentalList* sRentals;

// RandomizerRentalLookup over sRentals
static s32 Randomizer_RentalLookup(s32 species, RandomizerMon* rental) {
    const unk_func_80026268_arg0* mon;
    u32 i;
    s32 j;

    for (i = 0; i < sRentals->count; i++) {
        mon = &sRentals->mons[i];
        if (mon->unk_00.unk_00 == species) {
            rental->species = species;
            for (j = 0; j < 4; j++) {
                rental->moves[j] = mon->unk_09[j];
            }
            rental->dvs[0] = (mon->unk_1E >> 12) & 0xF;
            rental->dvs[1] = (mon->unk_1E >> 8) & 0xF;
            rental->dvs[2] = (mon->unk_1E >> 4) & 0xF;
            rental->dvs[3] = mon->unk_1E & 0xF;
            rental->statExp[0] = mon->unk_14;
            rental->statExp[1] = mon->unk_16;
            rental->statExp[2] = mon->unk_18;
            rental->statExp[3] = mon->unk_1A;
            rental->statExp[4] = mon->unk_1C;
            return 1;
        }
    }
    return 0;
}

// Teams made with rules take their "Stadium" moves and stats from rentals (NULL: none)
void Randomizer_UseRentals(RandomizerRules* rules, const RandomizerRentalList* rentals) {
    sRentals = rentals;
    rules->rental = (rentals != NULL) ? Randomizer_RentalLookup : NULL;
}

#endif
