#ifndef _FRAGMENT62_RANDOMIZER_BATTLE_UI_H_
#define _FRAGMENT62_RANDOMIZER_BATTLE_UI_H_

#include "fragment62.h"
#include "src/3FB0.h"

#ifdef RANDOMIZER
/*
 * The randomizer's additions to the battle UI: the party box shown while R is held in
 * the Pokemon menu also lists each Pokemon's moves.
 *
 * fragment62's relocation table can't be rebuilt from the ELF (the decomp doesn't know
 * every address in it), so fragment62 keeps its exact layout: only a few functions
 * change, at their original size, and only their entries in the table are regenerated
 * (the splice line in yamls/us/fragment_regen.txt). The drawing is in a fragment of its
 * own, randomizer_battleui (linker_scripts/us/randomizer.ld), which the battle's setup
 * loads right after fragment31 and which hooks in through gRandomizerState.
 */

// In place of func_84340ACC, which nothing calls (randomizer_battle_ui_stub.s)
ret_func_80004454 Randomizer_BattleUiLoad(s32 id, u8* romStart, u8* romEnd);
void Randomizer_Party3Stub(unk_D_84390010* arg0, unk_D_800AE540_0004* arg1, s16 arg2, s16 arg3, s32 arg4);
void Randomizer_Party6Stub(unk_D_84390010* arg0, s16 arg1, s16 arg2, s32 arg3);

// The fragment, and its id, which randomizer_battle_ui_stub.s has to spell out
extern u8 randomizer_battleui_ROM_START[];
extern u8 randomizer_battleui_relocs_ROM_END[];
#define RANDOMIZER_BATTLE_UI_ID 0xB2 // 0x8C200000

void Randomizer_BattleUiEntry(void);
#endif

#endif // _FRAGMENT62_RANDOMIZER_BATTLE_UI_H_
