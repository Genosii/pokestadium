#ifndef _FRAGMENT36_RANDOMIZER_TITLE_H_
#define _FRAGMENT36_RANDOMIZER_TITLE_H_

#include "fragment36.h"

#ifdef RANDOMIZER
/*
 * The randomizer's title screen code is a fragment of its own, randomizer_title
 * (linker_scripts/us/randomizer.ld), so that fragment36 keeps its original size. The title
 * screen loads it as it loads its pictures (func_82100B98), and gets back the functions it
 * calls into it through.
 */
typedef struct RandomizerTitleHooks {
    void (*draw)(s32 arg0, s32 arg1); // func_82100028, called every frame, empty in the game
} RandomizerTitleHooks;

typedef RandomizerTitleHooks* (*RandomizerTitleEntry)(void);

extern u8 randomizer_title_TEXT_START[];
extern u8 randomizer_title_ROM_START[];
extern u8 randomizer_title_relocs_ROM_END[];

RandomizerTitleHooks* Randomizer_TitleEntry(void);
#endif

#endif // _FRAGMENT36_RANDOMIZER_TITLE_H_
