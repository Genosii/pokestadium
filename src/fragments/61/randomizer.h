#ifndef _FRAGMENT61_RANDOMIZER_H_
#define _FRAGMENT61_RANDOMIZER_H_

#include "fragment61.h"

#ifdef RANDOMIZER
#include "src/randomizer_state.h"

/*
 * The randomizer's pick-screen code is a fragment of its own, randomizer_pick
 * (linker_scripts/us/randomizer.ld), so that fragment61 keeps its original size. The
 * pick screen (func_84203E6C) loads it when it starts, and gets back the functions
 * it calls into it through.
 */
typedef struct RandomizerPickHooks {
    void (*listInput)(unk_D_842168A0* list); // the rental list's input handler (func_8420AA08)
    void (*draw)(void);                      // at the end of each frame's drawing (func_84202718)
} RandomizerPickHooks;

typedef RandomizerPickHooks* (*RandomizerPickEntry)(void);

extern u8 randomizer_pick_TEXT_START[];
extern u8 randomizer_pick_ROM_START[];
extern u8 randomizer_pick_relocs_ROM_END[];

// In fragment61, set while the pick screen runs
extern RandomizerPickHooks* gRandomizerPickHooks;

// In randomizer_pick
RandomizerPickHooks* Randomizer_PickEntry(void);
void Randomizer_ListInput(unk_D_842168A0* list);
RandomizerState* Randomizer_State(void);
s32 Randomizer_FillTeam(unk_D_842168A0* list);
void Randomizer_PanelReset(void);
s32 Randomizer_PanelIsOpen(void);
s32 Randomizer_PanelInput(Controller* cont);
void Randomizer_PanelDraw(void);
void Randomizer_TeamReset(void);
void Randomizer_TeamDraw(void);
#endif

#endif // _FRAGMENT61_RANDOMIZER_H_
