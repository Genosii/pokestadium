#ifndef _FRAGMENT61_RANDOMIZER_H_
#define _FRAGMENT61_RANDOMIZER_H_

#include "fragment61.h"

#ifdef RANDOMIZER
#include "src/randomizer_state.h"

RandomizerState* Randomizer_State(void);
s32 Randomizer_FillTeam(unk_D_842168A0* list);
s32 Randomizer_PanelInput(Controller* cont);
void Randomizer_PanelDraw(void);
#endif

#endif // _FRAGMENT61_RANDOMIZER_H_
