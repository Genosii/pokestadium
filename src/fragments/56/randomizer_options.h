#ifndef _FRAGMENT56_RANDOMIZER_OPTIONS_H_
#define _FRAGMENT56_RANDOMIZER_OPTIONS_H_

#include "fragment56.h"

#ifdef RANDOMIZER
#include "src/fragments/61/randomizer_menu.h"

/*
 * The randomizer's Options screen code is a fragment of its own, randomizer_options
 * (linker_scripts/us/randomizer.ld), so that fragment56 keeps its original size. The screen
 * loads it when it starts (func_82C014FC), after randomizer_menu, the options panels
 * it opens, and gets back the functions it calls into it through.
 */
typedef struct RandomizerOptionsHooks {
    void (*draw)(s16 arg0, s32 arg1); // func_82C00658, the options window, with the randomizer's line
    void (*run)(void);                // the randomizer's line picked: the panels, until they're closed
} RandomizerOptionsHooks;

typedef RandomizerOptionsHooks* (*RandomizerOptionsEntry)(void);

// The randomizer's line, before "Erase data" and the last one
#define RANDOMIZER_OPTIONS_ITEM 2
#define RANDOMIZER_OPTIONS_COUNT 5

extern u8 randomizer_options_TEXT_START[];
extern u8 randomizer_options_ROM_START[];
extern u8 randomizer_options_relocs_ROM_END[];

// In fragment56
extern char** D_82C01660;
extern s16 D_82C01664;
extern u16 D_82C01666;

RandomizerOptionsHooks* Randomizer_OptionsEntry(void);
#endif

#endif // _FRAGMENT56_RANDOMIZER_OPTIONS_H_
