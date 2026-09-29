#ifndef _FRAGMENT63_RANDOMIZER_CUP_H_
#define _FRAGMENT63_RANDOMIZER_CUP_H_

#include "fragment63.h"

#ifdef RANDOMIZER
#include "src/randomizer_state.h"

/*
 * The randomizer's cup screen code is a fragment of its own, randomizer_cup
 * (linker_scripts/us/randomizer.ld), so that fragment63, the screen between a cup's or
 * the Gym Leader Castle's battles, keeps its original size. The screen loads it when it
 * starts (func_84B03194), and its entry puts these functions in gRandomizerState, where
 * fragment63 reaches them at a fixed address without relocations of its own.
 */
typedef struct RandomizerCupHooks {
    void (*draw)(void);         // at the end of every frame's drawing (func_84B014DC)
    void (*afterWin)(void);     // after a win, before the next battle's menu (func_84B022A0)
    s32 (*runOver)(void);       // after a loss: 1 if it ends the run, with the screen told to quit
    void (*pikachuCheck)(void); // func_84B01994, moved here to make room in fragment63
} RandomizerCupHooks;

#define RANDOMIZER_CUP_HOOKS ((RandomizerCupHooks*)gRandomizerState.cupHooks)

// The next-battle menu's line afterWin adds in Factory and Rogue: "Swap a Pokemon"
#define RANDOMIZER_CUP_SWAP_ITEM 2

typedef void (*RandomizerCupEntry)(void);

extern u8 randomizer_cup_TEXT_START[];
extern u8 randomizer_cup_ROM_START[];
extern u8 randomizer_cup_relocs_ROM_END[];

void Randomizer_CupEntry(void);
#endif

#endif // _FRAGMENT63_RANDOMIZER_CUP_H_
