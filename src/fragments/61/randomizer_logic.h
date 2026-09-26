#ifndef _FRAGMENT61_RANDOMIZER_LOGIC_H_
#define _FRAGMENT61_RANDOMIZER_LOGIC_H_

/*
 * Team generation for the in-game randomizer, ported from the random team generator
 * website (js/randomize.js). Kept free of game code so it can also be built on a PC
 * and checked against the website: see tools/randomizer/parity.
 */

#ifdef RANDOMIZER_HOST
#include <stdint.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t s32;
typedef uint64_t u64;
#else
#include "PR/ultratypes.h"
#endif

#define RANDOMIZER_NUM_SPECIES 151
#define RANDOMIZER_NUM_MOVES 165
#define RANDOMIZER_NUM_CHAOS_MOVES 165
#define RANDOMIZER_TEAM_SIZE 6

// RandomizerSpecies.flags
#define RANDOMIZER_SPECIES_FINAL_EVO (1 << 0)
#define RANDOMIZER_SPECIES_LEGENDARY (1 << 1)
#define RANDOMIZER_SPECIES_PHYSICAL (1 << 2)    // Attack >= Sp. Atk, for "Stadium" movesets
#define RANDOMIZER_SPECIES_PETIT_CUP (1 << 3)   // on the website's Petit Cup list
#define RANDOMIZER_SPECIES_PIKA_CUP (1 << 4)    // on the website's Pika Cup list
#define RANDOMIZER_SPECIES_GENDER_ROLL (1 << 5) // the website rolls a gender for it

typedef struct RandomizerSpecies {
    /* 0x0 */ u8 type1;
    /* 0x1 */ u8 type2;
    /* 0x2 */ u8 minLevel; // lowest level it can legally have
    /* 0x3 */ u8 flags;
    /* 0x4 */ u16 learnsetStart;
    /* 0x6 */ u8 learnsetCount;
} RandomizerSpecies; // size = 0x8

typedef struct RandomizerLearnsetMove {
    /* 0x0 */ u8 move;
    /* 0x1 */ u8 gen1; // learnable in Gen 1 itself, not only through tradeback
} RandomizerLearnsetMove; // size = 0x2

typedef struct RandomizerMove {
    /* 0x0 */ u8 type;
    /* 0x1 */ u8 power;
    /* 0x2 */ u8 accuracy;
    /* 0x3 */ u8 goodSupport; // on the website's GOOD_SUPPORT_MOVES list
} RandomizerMove; // size = 0x4

extern const RandomizerSpecies gRandomizerSpecies[RANDOMIZER_NUM_SPECIES + 1];
extern const RandomizerLearnsetMove gRandomizerLearnsets[];
extern const RandomizerMove gRandomizerMoves[RANDOMIZER_NUM_MOVES + 1];
extern const u8 gRandomizerChaosMoves[RANDOMIZER_NUM_CHAOS_MOVES];

typedef enum RandomizerCup {
    RANDOMIZER_CUP_POKE,  // s1Poke on the website
    RANDOMIZER_CUP_PETIT, // s1Petit
    RANDOMIZER_CUP_PIKA,  // s1Pika
    RANDOMIZER_CUP_PRIME  // s1Prime
} RandomizerCup;

typedef enum RandomizerMoveset {
    RANDOMIZER_MOVESET_LEGAL,   // four random moves from the learnset
    RANDOMIZER_MOVESET_STADIUM, // STAB + coverage + support + filler
    RANDOMIZER_MOVESET_CHAOS    // any Gen 1 move, learnsets ignored
} RandomizerMoveset;

typedef struct RandomizerRules {
    u8 cup;      // RandomizerCup: which species pool and level floor to use
    u8 level;    // if nonzero, every Pokemon gets this level
    u8 levelMin; // otherwise levels start here (raised to the species' minimum)...
    u8 levelMax; // ...and are capped here
    u8 levelSum; // if nonzero, the three lowest levels must add up to at most this
} RandomizerRules;

typedef struct RandomizerSettings {
    u8 moveset;       // RandomizerMoveset
    u8 noTradeback;   // only moves learnable in Gen 1 itself
    u8 randomDvs;     // otherwise all 15
    u8 randomStatExp; // otherwise all 65535
    u8 noLegendaries;
    u8 finalEvosOnly;
    u8 monoType;
} RandomizerSettings;

typedef struct RandomizerMon {
    /* 0x00 */ u8 species;
    /* 0x01 */ u8 level;
    /* 0x02 */ u8 moves[4]; // 0 = empty slot
    /* 0x06 */ u8 dvs[4];   // Attack, Defense, Speed, Special
    /* 0x0A */ u16 statExp[5]; // HP, Attack, Defense, Speed, Special
} RandomizerMon; // size = 0x14

#ifdef RANDOMIZER
void Randomizer_Seed(u32 seed);
s32 Randomizer_GenerateTeam(const RandomizerSettings* settings, const RandomizerRules* rules,
                            RandomizerMon team[RANDOMIZER_TEAM_SIZE]);
s32 Randomizer_TeamFitsLevelSum(const RandomizerRules* rules, const RandomizerMon team[RANDOMIZER_TEAM_SIZE]);
#endif

#endif // _FRAGMENT61_RANDOMIZER_LOGIC_H_
