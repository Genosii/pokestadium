#ifndef _RANDOMIZER_STATE_H_
#define _RANDOMIZER_STATE_H_

/*
 * The in-game randomizer's settings, kept where screens loading and unloading can't
 * touch them, so the options chosen on the pick screen still apply on the
 * battle-select screen and the next time the pick screen opens.
 *
 * They live in osAppNMIBuffer, the 64 bytes at 0x8000031C that libultra sets aside
 * for the game and only clears at power-on (the game itself never uses them). Keeping
 * them out of the game's own memory leaves the first megabyte of the ROM exactly as it
 * was, so emulators that recognise games by its checksum (Project64) still apply
 * their settings for Pokemon Stadium. As a bonus they survive the Reset button.
 *
 * The buffer only counts as holding settings once the magic value is there; the pick
 * screen sets it up with the defaults the first time.
 */

#include "ultra64.h"
#include "src/fragments/61/randomizer_logic.h"

#ifdef RANDOMIZER

#define RANDOMIZER_STATE_MAGIC 0x524E4433 // "RND3"

typedef struct RandomizerState {
    /* 0x00 */ u32 magic;
    /* 0x04 */ u32 lastSeed; // seed of the last team made with Z
    /* 0x08 */ RandomizerSettings settings;
    /* 0x0F */ u8 autoBattlePick;      // pick a random three on the battle-select screen by itself
    /* 0x10 */ u32 enteredSeed;        // seed typed in on the options panel...
    /* 0x14 */ u8 useEnteredSeed;      // ...for the next team only, as the website does with a shared seed
    /* 0x18 */ void* battlePartyHook3; // set for each battle by the randomizer's battle UI fragment, which
    /* 0x1C */ void* battlePartyHook6; // fragment62 can't refer to directly (randomizer_battle_ui_stub.s)
} RandomizerState;                     // size = 0x20, must fit in OS_APP_NMI_BUFSIZE (64)

#define gRandomizerState (*(RandomizerState*)osAppNMIBuffer)

#define RANDOMIZER_STATE_VALID() (gRandomizerState.magic == RANDOMIZER_STATE_MAGIC)

#endif

#endif // _RANDOMIZER_STATE_H_
