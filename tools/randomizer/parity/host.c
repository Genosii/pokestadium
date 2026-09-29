/*
 * Runs the in-game randomizer's team generation on a PC, for comparing with the website.
 * Reads cases from stdin, one per line: "<cup> <moveset> <dvs> <statexp> <flags> <seed>":
 *   cup:        poke | petit | pika | prime
 *   moveset:    stadium | legal | strong | chaos
 *   dvs, statexp: stadium | max | random
 *   flags:      bits: 1 no tradeback, 2 no legendaries, 4 final evolutions only,
 *               8 mono type, 16 no shared types
 * and prints one JSON array per case: [[species, level, [moves], [dvs], [stat exp]], ...]
 * or null if no team could be made.
 *
 * rentals.h, written by check.py from the game's rental tables, has each cup's rental
 * Pokemon for the "Stadium" options, as the game's pick screen lists them.
 */
#include <stdio.h>
#include <string.h>

#include "randomizer_logic.h"

typedef struct HostRental {
    u8 present;
    u8 moves[4];
    u8 dvs[4];
    u16 statExp[5];
} HostRental;

#include "rentals.h" // static const HostRental sRentals[4][RANDOMIZER_NUM_SPECIES + 1]

static int sCup;

static s32 HostRentalLookup(s32 species, RandomizerMon* rental) {
    const HostRental* entry = &sRentals[sCup][species];

    if (!entry->present) {
        return 0;
    }
    rental->species = species;
    memcpy(rental->moves, entry->moves, sizeof(rental->moves));
    memcpy(rental->dvs, entry->dvs, sizeof(rental->dvs));
    memcpy(rental->statExp, entry->statExp, sizeof(rental->statExp));
    return 1;
}

static const RandomizerRules sRules[] = {
    { RANDOMIZER_CUP_POKE, 0, 50, 55, 155, 0, 0, HostRentalLookup },
    { RANDOMIZER_CUP_PETIT, 0, 25, 30, 80, 0, 0, HostRentalLookup },
    { RANDOMIZER_CUP_PIKA, 0, 15, 20, 50, 0, 0, HostRentalLookup },
    { RANDOMIZER_CUP_PRIME, 100, 0, 0, 0, 0, 0, HostRentalLookup },
};

static int lookup(const char* word, const char* const* names, int count) {
    int i;

    for (i = 0; i < count; i++) {
        if (strcmp(word, names[i]) == 0) {
            return i;
        }
    }
    return -1;
}

int main(void) {
    static const char* const cups[] = { "poke", "petit", "pika", "prime" };
    static const char* const movesets[] = { "stadium", "legal", "strong", "chaos" };
    static const char* const sources[] = { "stadium", "max", "random" };
    char cupName[16];
    char movesetName[16];
    char dvsName[16];
    char statExpName[16];
    unsigned flags;
    unsigned long seed;

    while (scanf("%15s %15s %15s %15s %u %lu", cupName, movesetName, dvsName, statExpName, &flags, &seed) == 6) {
        RandomizerSettings settings;
        RandomizerMon team[RANDOMIZER_TEAM_SIZE];
        int moveset = lookup(movesetName, movesets, RANDOMIZER_MOVESET_COUNT);
        int dvs = lookup(dvsName, sources, RANDOMIZER_STATS_COUNT);
        int statExp = lookup(statExpName, sources, RANDOMIZER_STATS_COUNT);
        int i;
        int j;

        sCup = lookup(cupName, cups, 4);
        if ((sCup < 0) || (moveset < 0) || (dvs < 0) || (statExp < 0)) {
            fprintf(stderr, "bad case: %s %s %s %s\n", cupName, movesetName, dvsName, statExpName);
            return 1;
        }
        settings.moveset = moveset;
        settings.dvs = dvs;
        settings.statExp = statExp;
        settings.noTradeback = (flags & 1) != 0;
        settings.noLegendaries = (flags & 2) != 0;
        settings.finalEvosOnly = (flags & 4) != 0;
        settings.monoType = (flags & 8) != 0;
        settings.noSharedTypes = (flags & 16) != 0;

        Randomizer_Seed((u32)seed);
        if (!Randomizer_GenerateTeam(&settings, &sRules[sCup], team)) {
            printf("null\n");
            continue;
        }

        printf("[");
        for (i = 0; i < RANDOMIZER_TEAM_SIZE; i++) {
            RandomizerMon* mon = &team[i];

            printf("%s[%d,%d,[", i ? "," : "", mon->species, mon->level);
            for (j = 0; j < 4; j++) {
                printf("%s%d", j ? "," : "", mon->moves[j]);
            }
            printf("],[");
            for (j = 0; j < 4; j++) {
                printf("%s%d", j ? "," : "", mon->dvs[j]);
            }
            printf("],[");
            for (j = 0; j < 5; j++) {
                printf("%s%d", j ? "," : "", mon->statExp[j]);
            }
            printf("]]");
        }
        printf("]\n");
    }
    return 0;
}
