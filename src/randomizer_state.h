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

#define RANDOMIZER_STATE_MAGIC 0x524E4434 // "RND4"; changes whenever the layout does

typedef struct RandomizerState {
    /* 0x00 */ u32 magic;
    /* 0x04 */ u32 lastSeed;    // seed of the last team made with Z
    /* 0x08 */ u32 enteredSeed; // seed typed in on the options panel, for the next team only,
                                // as the website does with a shared seed (useEnteredSeed)
                                // Set for each battle by the randomizer's battle UI fragment, which fragment62 can't
                                // refer to directly, so randomizer_battle_ui_stub.s reads them at fixed addresses
    /* 0x0C */ void* battlePartyHook3; // 0x80000328
    /* 0x10 */ void* battlePartyHook6; // 0x8000032C
    /* 0x14 */ RandomizerSettings settings;
    /* 0x1C */ u8 autoBattlePick; // pick a random three on the battle-select screen by itself
    /* 0x1D */ u8 useEnteredSeed;
} RandomizerState; // size = 0x20, must fit in OS_APP_NMI_BUFSIZE (64)

#ifdef __GNUC__
// Checked by the build's GCC syntax pass: randomizer_battle_ui_stub.s has these offsets
// written out, and the whole state must fit in osAppNMIBuffer
typedef char RandomizerStateLayoutCheck[((__builtin_offsetof(RandomizerState, battlePartyHook3) == 0x0C) &&
                                         (__builtin_offsetof(RandomizerState, battlePartyHook6) == 0x10) &&
                                         (sizeof(RandomizerState) <= OS_APP_NMI_BUFSIZE))
                                            ? 1
                                            : -1];
#endif

#define gRandomizerState (*(RandomizerState*)osAppNMIBuffer)

#define RANDOMIZER_STATE_VALID() (gRandomizerState.magic == RANDOMIZER_STATE_MAGIC)

#endif

#endif // _RANDOMIZER_STATE_H_
