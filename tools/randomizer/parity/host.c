/*
 * Runs the in-game randomizer's team generation on a PC, for comparing with the website.
 * Reads cases from stdin, one per line: "<cup> <moveset> <flags> <seed>", where
 *   cup:     poke | petit | pika | prime
 *   moveset: legal | stadium | chaos
 *   flags:   bits: 1 no tradeback, 2 random DVs, 4 random stat exp, 8 no legendaries,
 *            16 final evolutions only, 32 mono type
 * and prints one JSON array per case: [[species, level, [moves], [dvs], [stat exp]], ...]
 * or null if no team could be made.
 */
#include <stdio.h>
#include <string.h>

#include "randomizer_logic.h"

static const RandomizerRules sRules[] = {
    { RANDOMIZER_CUP_POKE, 0, 50, 55, 155 },
    { RANDOMIZER_CUP_PETIT, 0, 25, 30, 80 },
    { RANDOMIZER_CUP_PIKA, 0, 15, 20, 50 },
    { RANDOMIZER_CUP_PRIME, 100, 0, 0, 0 },
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
    static const char* const movesets[] = { "legal", "stadium", "chaos" };
    char cupName[16];
    char movesetName[16];
    unsigned flags;
    unsigned long seed;

    while (scanf("%15s %15s %u %lu", cupName, movesetName, &flags, &seed) == 4) {
        RandomizerSettings settings;
        RandomizerMon team[RANDOMIZER_TEAM_SIZE];
        int cup = lookup(cupName, cups, 4);
        int i;
        int j;

        settings.moveset = lookup(movesetName, movesets, 3);
        if ((cup < 0) || (settings.moveset > 2)) {
            fprintf(stderr, "bad case: %s %s\n", cupName, movesetName);
            return 1;
        }
        settings.noTradeback = (flags & 1) != 0;
        settings.randomDvs = (flags & 2) != 0;
        settings.randomStatExp = (flags & 4) != 0;
        settings.noLegendaries = (flags & 8) != 0;
        settings.finalEvosOnly = (flags & 16) != 0;
        settings.monoType = (flags & 32) != 0;

        Randomizer_Seed((u32)seed);
        if (!Randomizer_GenerateTeam(&settings, &sRules[cup], team)) {
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
