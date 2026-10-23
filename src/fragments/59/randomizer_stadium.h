#ifndef _FRAGMENT59_RANDOMIZER_STADIUM_H_
#define _FRAGMENT59_RANDOMIZER_STADIUM_H_

#include "fragment59.h"

#ifdef RANDOMIZER
#include "src/3FB0.h"
#include "src/fragments/61/randomizer_menu.h"

/*
 * The Stadium's cup screen with the Battle Frontier's two modes, Rogue and Factory, in the
 * two places the game has for more cups but never shows (slots 1 and 2, with the modes,
 * trainers and pictures of two cups the US version left out). Its code is a fragment of its
 * own, randomizer_stadium (linker_scripts/us/randomizer.ld), so that fragment59 keeps its
 * size: the screen loads it as it starts (func_841022C0), after randomizer_menu, which has
 * "Choose your rules" and the rules, and calls into it through these.
 */
typedef struct RandomizerStadiumHooks {
    void (*setup)(void);       // func_84101BFC: the six cups' panels
    void (*select)(void);      // func_84101EC8: the screen until a cup is picked, or B
    void (*caption)(s16 arg0); // func_84101210: the cup's name and lines, and "Choose your rules"
    void (*leave)(void);       // as the screen ends: the rules saved if they changed
} RandomizerStadiumHooks;

typedef RandomizerStadiumHooks* (*RandomizerStadiumEntry)(void);

// The slots the Battle Frontier's modes have
#define STADIUM_SLOT_ROGUE 1
#define STADIUM_SLOT_FACTORY 2
#define STADIUM_SLOTS 6

extern u8 randomizer_stadium_TEXT_START[];
extern u8 randomizer_stadium_ROM_START[];
extern u8 randomizer_stadium_relocs_ROM_END[];

// In fragment59
extern char** D_84103440;
extern BinArchive* D_84103444;
extern void* D_8410344C;
extern unk_D_86002F58_004_000 D_84103458[6];
extern s16 D_84103CC8;
extern unk_D_80068BB0* D_84103CD0[6];
extern GraphNode* D_84103CE8;
extern GraphNode* D_84103CEC;
extern u8 D_84102430[];
extern s16 D_84102444[];
extern u8 D_84102528[][2];

RandomizerStadiumHooks* Randomizer_StadiumEntry(void);
#endif

#endif // _FRAGMENT59_RANDOMIZER_STADIUM_H_
