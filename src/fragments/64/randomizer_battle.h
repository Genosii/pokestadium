#ifndef _FRAGMENT64_RANDOMIZER_BATTLE_H_
#define _FRAGMENT64_RANDOMIZER_BATTLE_H_

#include "fragment64.h"

#ifdef RANDOMIZER
/*
 * The randomizer's battle-select code is a fragment of its own, randomizer_battle
 * (linker_scripts/us/randomizer.ld), so that fragment64 keeps its original size. The
 * battle-select screen (func_84803368) loads it when it starts, and gets back the
 * functions it calls into it through.
 */
typedef struct RandomizerBattleHooks {
    void (*pickInput)(Controller* cont, unk_D_848037A0* player); // a player's picking (func_848027F0)
    void (*drawFooter)(char** texts);                            // the footer (func_84800020)
} RandomizerBattleHooks;

typedef RandomizerBattleHooks* (*RandomizerBattleEntry)(void);

extern u8 randomizer_battle_TEXT_START[];
extern u8 randomizer_battle_ROM_START[];
extern u8 randomizer_battle_relocs_ROM_END[];

RandomizerBattleHooks* Randomizer_BattleEntry(void);
void Randomizer_PickInput(Controller* cont, unk_D_848037A0* player);
void Randomizer_DrawFooter(char** texts);
#endif

#endif // _FRAGMENT64_RANDOMIZER_BATTLE_H_
