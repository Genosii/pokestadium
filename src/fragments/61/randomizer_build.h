#ifndef _FRAGMENT61_RANDOMIZER_BUILD_H_
#define _FRAGMENT61_RANDOMIZER_BUILD_H_

#include "global.h"
#include "randomizer_logic.h"
#include "src/29BA0.h"

#ifdef RANDOMIZER
/*
 * The randomizer's core: the team generator (randomizer_logic.c, randomizer_data.c) and
 * turning its teams into the game's Pokemon (randomizer_build.c). It's a fragment of its
 * own, randomizer_core (linker_scripts/us/randomizer.ld), that the pick screen and the
 * battle-select screen load before their own randomizer fragments, which use it.
 */

extern u8 randomizer_core_TEXT_START[];
extern u8 randomizer_core_ROM_START[];
extern u8 randomizer_core_relocs_ROM_END[];

void Randomizer_CoreEntry(void);
void Randomizer_GetRules(s32 ruleSet, s32 anyLevel, RandomizerRules* rules);
void Randomizer_BuildPokemon(unk_func_80026268_arg0* mon, const RandomizerMon* src,
                             const unk_func_80026268_arg0* trainer);

// A list of rental Pokemon as the game keeps them: a count, then the Pokemon
typedef struct RandomizerRentalList {
    /* 0x00 */ u32 count;
    /* 0x04 */ unk_func_80026268_arg0 mons[1];
} RandomizerRentalList;

s32 Randomizer_RentalTable(void);
void Randomizer_UseRentals(RandomizerRules* rules, const RandomizerRentalList* rentals);
#endif

#endif // _FRAGMENT61_RANDOMIZER_BUILD_H_
