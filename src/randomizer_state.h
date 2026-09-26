#ifndef _RANDOMIZER_STATE_H_
#define _RANDOMIZER_STATE_H_

/*
 * The in-game randomizer's settings, kept in RAM that stays put while screens load and
 * unload (just before the main memory pool), so the options chosen on the pick screen
 * still apply on the battle-select screen and the next time the pick screen opens.
 *
 * Nothing clears this memory at power-on on real hardware, so it only counts once the
 * magic value is there; the pick screen sets it up with the defaults the first time.
 */

#include "src/fragments/61/randomizer_logic.h"

#ifdef RANDOMIZER

#define RANDOMIZER_STATE_MAGIC 0x524E4433 // "RND3"

typedef struct RandomizerState {
    /* 0x00 */ u32 magic;
    /* 0x04 */ u32 lastSeed; // seed of the last team made with Z
    /* 0x08 */ RandomizerSettings settings;
    /* 0x0F */ u8 autoBattlePick; // pick a random three on the battle-select screen by itself
    /* 0x10 */ u32 enteredSeed;   // seed typed in on the options panel...
    /* 0x14 */ u8 useEnteredSeed; // ...for the next team only, as the website does with a shared seed
} RandomizerState; // size = 0x18

extern RandomizerState gRandomizerState;

#define RANDOMIZER_STATE_VALID() (gRandomizerState.magic == RANDOMIZER_STATE_MAGIC)

#endif

#endif // _RANDOMIZER_STATE_H_
