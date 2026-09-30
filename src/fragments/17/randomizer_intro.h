#ifndef _FRAGMENT17_RANDOMIZER_INTRO_H_
#define _FRAGMENT17_RANDOMIZER_INTRO_H_

#include "fragment17.h"

#ifdef RANDOMIZER
/*
 * The randomizer's intro: random Pokemon in its first four scenes. The code is a fragment
 * of its own, randomizer_intro (linker_scripts/us/randomizer.ld), run once as the intro
 * starts and freed straight after, since all it does is change the scenes' tables.
 *
 * fragment17's relocation table can't be rebuilt from the ELF (see docs/game-screens.md),
 * so fragment17 keeps its exact layout: func_86B044B0, which nothing calls, is replaced by
 * a stub of the same size that runs the fragment (randomizer_intro_stub.s), the intro's
 * setup (func_86B01190) calls that stub in place of func_8002D510, which the stub then
 * calls, and only those two functions' table entries are regenerated (the splice line in
 * yamls/us/fragment_regen_randomizer.txt).
 */

// In place of func_86B044B0 (randomizer_intro_stub.s)
void Randomizer_IntroLoad(void);

// The fragment, and its id, which randomizer_intro_stub.s has to spell out
extern u8 randomizer_intro_ROM_START[];
extern u8 randomizer_intro_relocs_ROM_END[];
#define RANDOMIZER_INTRO_ID 0xB9 // 0x8C900000

s32 Randomizer_IntroEntry(s32 arg0, s32 arg1);
#endif

#endif // _FRAGMENT17_RANDOMIZER_INTRO_H_
