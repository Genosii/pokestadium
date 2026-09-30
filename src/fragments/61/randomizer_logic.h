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
#define RANDOMIZER_NUM_LEARNSET_MOVES 4714 // every species' learnset, one after the other
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
    /* 0x1 */ u8 gen1;    // learnable in Gen 1 itself, not only through tradeback
} RandomizerLearnsetMove; // size = 0x2

// RandomizerMove.flags
#define RANDOMIZER_MOVE_GOOD_SUPPORT (1 << 0) // on the website's GOOD_SUPPORT_MOVES list
#define RANDOMIZER_MOVE_UNRELIABLE (1 << 1)   // on its UNRELIABLE_MOVES list: no Strong set builds on it

typedef struct RandomizerMove {
    /* 0x0 */ u8 type; // Gen 1's own values (the website's gen1_move_overrides.json)
    /* 0x1 */ u8 power;
    /* 0x2 */ u8 accuracy;
    /* 0x3 */ u8 flags;
} RandomizerMove; // size = 0x4

// The generator's tables (randomizer_data.c, made by tools/randomizer/gen_data.py, which
// also packs them for the ROM byte for byte as laid out here: change it with them)
typedef struct RandomizerData {
    RandomizerSpecies species[RANDOMIZER_NUM_SPECIES + 1];
    RandomizerLearnsetMove learnsets[RANDOMIZER_NUM_LEARNSET_MOVES];
    RandomizerMove moves[RANDOMIZER_NUM_MOVES + 1];
    u8 chaosMoves[RANDOMIZER_NUM_CHAOS_MOVES];
} RandomizerData;

#ifdef RANDOMIZER_HOST
extern const RandomizerData gRandomizerData;
#else
// The ROM has them compressed; Randomizer_UnpackData unpacks them, and the pick screen's
// and the battle-select screen's randomizer fragments call it as they load
extern RandomizerData gRandomizerData;
void Randomizer_UnpackData(void);
#endif

#define gRandomizerSpecies (gRandomizerData.species)
#define gRandomizerLearnsets (gRandomizerData.learnsets)
#define gRandomizerMoves (gRandomizerData.moves)
#define gRandomizerChaosMoves (gRandomizerData.chaosMoves)

typedef enum RandomizerCup {
    RANDOMIZER_CUP_POKE,  // s1Poke on the website
    RANDOMIZER_CUP_PETIT, // s1Petit
    RANDOMIZER_CUP_PIKA,  // s1Pika
    RANDOMIZER_CUP_PRIME  // s1Prime
} RandomizerCup;

typedef enum RandomizerMoveset {
    RANDOMIZER_MOVESET_STADIUM, // the moves of the cup's rental Pokemon, or Strong without one
    RANDOMIZER_MOVESET_LEGAL,   // four random moves from the learnset
    RANDOMIZER_MOVESET_STRONG,  // the strongest STAB of each type, strong coverage, a support move
    RANDOMIZER_MOVESET_CHAOS,   // any Gen 1 move, learnsets ignored
    RANDOMIZER_MOVESET_COUNT
} RandomizerMoveset;

// Where DVs and stat exp come from
typedef enum RandomizerStatSource {
    RANDOMIZER_STATS_STADIUM, // the cup's rental Pokemon's, or maximum without one
    RANDOMIZER_STATS_MAX,     // 15 DVs, 65535 stat exp
    RANDOMIZER_STATS_RANDOM,
    RANDOMIZER_STATS_COUNT
} RandomizerStatSource;

struct RandomizerMon;

// Fills rental with the cup's rental Pokemon of that species (moves, DVs and stat exp);
// returns 0 if there's none
typedef s32 (*RandomizerRentalLookup)(s32 species, struct RandomizerMon* rental);

typedef struct RandomizerRules {
    u8 cup;      // RandomizerCup: which species pool and level floor to use
    u8 level;    // if nonzero, every Pokemon gets this level
    u8 levelMin; // otherwise levels start here (raised to the species' minimum)...
    u8 levelMax; // ...and are capped here
    u8 levelSum; // if nonzero, the three lowest levels must add up to at most this
    // Not on the website: a type theme for a trainer, as a type id plus one (zero for
    // none). The team takes Pokemon of theme first, then of theme2 to make up six.
    u8 theme;
    u8 theme2;
    RandomizerRentalLookup rental; // the cup's rentals, for the "Stadium" options; none if 0
} RandomizerRules;

typedef struct RandomizerSettings {
    u8 moveset;     // RandomizerMoveset
    u8 noTradeback; // only moves learnable in Gen 1 itself
    u8 dvs;         // RandomizerStatSource
    u8 statExp;     // RandomizerStatSource
    u8 noLegendaries;
    u8 finalEvosOnly;
    u8 monoType;
    u8 noSharedTypes; // no type on more than one Pokemon (ignored with monoType)
} RandomizerSettings;

typedef struct RandomizerMon {
    /* 0x00 */ u8 species;
    /* 0x01 */ u8 level;
    /* 0x02 */ u8 moves[4];    // 0 = empty slot
    /* 0x06 */ u8 dvs[4];      // Attack, Defense, Speed, Special
    /* 0x0A */ u16 statExp[5]; // HP, Attack, Defense, Speed, Special
} RandomizerMon;               // size = 0x14

#ifdef RANDOMIZER
void Randomizer_Seed(u32 seed);
s32 Randomizer_GenerateTeam(const RandomizerSettings* settings, const RandomizerRules* rules,
                            RandomizerMon team[RANDOMIZER_TEAM_SIZE]);
s32 Randomizer_TeamFitsLevelSum(const RandomizerRules* rules, const RandomizerMon team[RANDOMIZER_TEAM_SIZE]);
#endif

#endif // _FRAGMENT61_RANDOMIZER_LOGIC_H_
