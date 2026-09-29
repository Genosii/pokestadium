#ifndef _FRAGMENT55_RANDOMIZER_RULES_H_
#define _FRAGMENT55_RANDOMIZER_RULES_H_

#include "fragment55.h"

#ifdef RANDOMIZER
#include "src/fragments/61/randomizer_menu.h"

/*
 * The randomizer's Rules screen code is a fragment of its own, randomizer_rules
 * (linker_scripts/us/randomizer.ld), so that fragment55 keeps its original size. The screen
 * loads it when it starts (func_83002120), after randomizer_menu, the options panels
 * it opens, and gets back the functions it calls into it through.
 */
typedef struct RandomizerRulesHooks {
    void (*input)(void);                  // func_8300059C, the list's input, with the randomizer's buttons
    void (*drawList)(s16 arg0, s16 arg1); // func_830015EC, the list's text, with the button and the panels
} RandomizerRulesHooks;

typedef RandomizerRulesHooks* (*RandomizerRulesEntry)(void);

extern u8 randomizer_rules_TEXT_START[];
extern u8 randomizer_rules_ROM_START[];
extern u8 randomizer_rules_relocs_ROM_END[];

// In fragment55
extern s16 D_83003C80;
extern s16 D_83003C82;
extern s16 D_83003C90;
extern char* D_83003C9C;
extern s16* D_83003CA0;
extern s16 D_83003CA4;
extern s16 D_83003CA6;
extern char* D_83003CBC;

RandomizerRulesHooks* Randomizer_RulesEntry(void);
#endif

#endif // _FRAGMENT55_RANDOMIZER_RULES_H_
