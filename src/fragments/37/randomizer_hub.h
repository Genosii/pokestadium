#ifndef _FRAGMENT37_RANDOMIZER_HUB_H_
#define _FRAGMENT37_RANDOMIZER_HUB_H_

#include "fragment37.h"

#ifdef RANDOMIZER_HUB
/*
 * The 3D hub, a prototype in place of the area select (HUB=1 builds only): the Stadium
 * complex seen from the middle of the open field, turning from building to building. A
 * fragment of its own, randomizer_hub (linker_scripts/us/randomizer_hub.ld), past the
 * first 32 MB of the ROM. The area select's entry (func_82202DC4) loads it and returns
 * the state its entry returns, as the area select's would.
 */
typedef s32 (*RandomizerHubEntry)(void);

extern u8 randomizer_hub_TEXT_START[];
extern u8 randomizer_hub_ROM_START[];
extern u8 randomizer_hub_relocs_ROM_END[];

s32 Randomizer_HubEntry(void);
#endif

#endif // _FRAGMENT37_RANDOMIZER_HUB_H_
