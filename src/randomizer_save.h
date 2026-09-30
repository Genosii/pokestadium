#ifndef _RANDOMIZER_SAVE_H_
#define _RANDOMIZER_SAVE_H_

/*
 * Where the randomizer keeps its data in the save file. The third bank (0x3F80 bytes, the
 * Options' settings, the Stadium's records and so on) ends with 0x17C bytes that none of its
 * sections covers; the game reads and writes them with the rest of the bank and never looks
 * at them (docs/game-data.md). Everything the randomizer saves goes there, each with a magic
 * value of its own, so a save file from the original game, or one whose bytes there are
 * anything else, simply gives the defaults:
 *
 *   0x3E04  RandomizerSave, the options (randomizer_menu.c), 0x1C bytes
 *   0x3E20  RandomizerBootCount, for the intro (randomizer_intro.c), 8 bytes
 *
 * Erasing the data on the Options screen leaves them.
 */

#include "src/26820.h"

#ifdef RANDOMIZER

// The save file's banks as loaded (D_800AE4E8, static in 26820.c; the first megabyte of the
// ROM doesn't move in RANDOMIZER=1 builds, so neither does it)
#define RANDOMIZER_SAVE_BANKS ((unk_D_800AE4E8*)0x800AE4E8)
#define RANDOMIZER_SAVE_BANK 2
#define RANDOMIZER_SAVE_BANK_DIRTY 2 // unk_00: written to the cartridge by func_800284B4

#define RANDOMIZER_SAVE_SETTINGS 0x3E04
#define RANDOMIZER_SAVE_BOOT_COUNT 0x3E20

// Bytes at offset in bank 2, which has to be loaded (func_80028AFC)
#define RANDOMIZER_SAVE_AT(offset) ((u8*)RANDOMIZER_SAVE_BANKS[RANDOMIZER_SAVE_BANK].unk_04.unk2 + (offset))

// Writes bank 2 to the cartridge after changing the randomizer's bytes in it, the way the
// game saves the bank after changing a section
#define RANDOMIZER_SAVE_WRITE()                                                           \
    do {                                                                                  \
        RANDOMIZER_SAVE_BANKS[RANDOMIZER_SAVE_BANK].unk_00 |= RANDOMIZER_SAVE_BANK_DIRTY; \
        func_800284B4(RANDOMIZER_SAVE_BANK);                                              \
    } while (0)

#endif

#endif // _RANDOMIZER_SAVE_H_
