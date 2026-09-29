/*
 * Entry point of the randomizer's menu fragment (randomizer_menu, see randomizer_menu.h),
 * with the settings it shows: set up the first time they're needed after power-on, from
 * the save file if they were saved there, and saved again when a panel closes with them
 * changed. Built only with RANDOMIZER=1; empty otherwise so the default build still matches.
 *
 * The save file's third bank (0x3F80 bytes, the Options' settings, the Stadium's records
 * and so on) ends with 0x17C bytes that none of its sections covers; the game reads and
 * writes them with the rest of the bank and never looks at them. The settings go there,
 * with a magic value and a checksum of their own, so a save file from the original game,
 * or one whose bytes there are anything else, simply gives the defaults. Erasing the data
 * on the Options screen leaves them.
 */
#include "randomizer_menu.h"

#ifdef RANDOMIZER

#include "src/26820.h"

// The save file's banks as loaded (D_800AE4E8, static in 26820.c; the first megabyte of the
// ROM doesn't move in RANDOMIZER=1 builds, so neither does it)
#define SAVE_BANKS ((unk_D_800AE4E8*)0x800AE4E8)
#define SAVE_BANK 2
#define SAVE_BANK_DIRTY 2  // unk_00: written to the cartridge by func_800284B4
#define SAVE_OFFSET 0x3E04 // after the last section (unk_3280)

#define SAVE_MAGIC 0x524E4453 // "RNDS"

typedef struct RandomizerSave {
    /* 0x00 */ u32 magic;
    /* 0x04 */ RandomizerSettings settings;
    /* 0x0C */ RandomizerSettings opponentSettings;
    /* 0x14 */ u8 mode;
    /* 0x15 */ u8 randomOpponents;
    /* 0x16 */ u8 autoBattlePick;
    /* 0x17 */ u8 pad;
    /* 0x18 */ u32 checksum;
} RandomizerSave; // size = 0x1C

// The website's defaults
// Stadium movesets, DVs and stat exp: the game's own rentals (and trainers)
static const RandomizerSettings sDefaultSettings = {
    RANDOMIZER_MOVESET_STADIUM, 0, RANDOMIZER_STATS_STADIUM, RANDOMIZER_STATS_STADIUM, 0, 0, 0, 0,
};

void Randomizer_MenuEntry(void) {
}

static u32 Randomizer_SaveChecksum(RandomizerSave* save) {
    u8* bytes = (u8*)save;
    u32 sum = SAVE_MAGIC;
    u32 i;

    // Everything before the checksum, the last field
    for (i = 0; i < sizeof(RandomizerSave) - sizeof(save->checksum); i++) {
        sum = (sum * 31) + bytes[i];
    }
    return sum;
}

// Bank 2 as loaded, loading it first if no screen has; NULL if there's no memory for it
static RandomizerSave* Randomizer_SaveSlot(void) {
    if (!func_80028AFC(SAVE_BANK)) {
        return NULL;
    }
    return (RandomizerSave*)((u8*)SAVE_BANKS[SAVE_BANK].unk_04.unk2 + SAVE_OFFSET);
}

static void Randomizer_SaveFill(RandomizerSave* save, RandomizerState* state) {
    bzero(save, sizeof(RandomizerSave));
    save->magic = SAVE_MAGIC;
    save->settings = state->settings;
    save->opponentSettings = state->opponentSettings;
    save->mode = state->mode;
    save->randomOpponents = state->randomOpponents;
    save->autoBattlePick = state->autoBattlePick;
    save->checksum = Randomizer_SaveChecksum(save);
}

static s32 Randomizer_SettingsValid(RandomizerSettings* settings) {
    return (settings->moveset < RANDOMIZER_MOVESET_COUNT) && (settings->dvs < RANDOMIZER_STATS_COUNT) &&
           (settings->statExp < RANDOMIZER_STATS_COUNT);
}

static void Randomizer_LoadSettings(RandomizerState* state) {
    RandomizerSave save;
    RandomizerSave* slot = Randomizer_SaveSlot();

    if (slot == NULL) {
        return;
    }
    // Copied first: the bank's bytes there aren't necessarily aligned for the structure
    bcopy(slot, &save, sizeof(save));
    if ((save.magic != SAVE_MAGIC) || (save.checksum != Randomizer_SaveChecksum(&save)) ||
        !Randomizer_SettingsValid(&save.settings) || !Randomizer_SettingsValid(&save.opponentSettings) ||
        (save.mode >= RANDOMIZER_MODE_COUNT)) {
        return;
    }
    state->settings = save.settings;
    state->opponentSettings = save.opponentSettings;
    state->mode = save.mode;
    state->randomOpponents = save.randomOpponents;
    state->autoBattlePick = save.autoBattlePick;
}

void Randomizer_SaveSettings(void) {
    RandomizerSave save;
    RandomizerSave* slot = Randomizer_SaveSlot();

    if (slot == NULL) {
        return;
    }
    Randomizer_SaveFill(&save, Randomizer_State());
    if (bcmp(slot, &save, sizeof(save)) == 0) {
        return;
    }
    bcopy(&save, slot, sizeof(save));
    // How the game saves bank 2 after changing it
    SAVE_BANKS[SAVE_BANK].unk_00 |= SAVE_BANK_DIRTY;
    func_800284B4(SAVE_BANK);
}

// The settings, set up if they aren't there yet (see randomizer_state.h)
RandomizerState* Randomizer_State(void) {
    if (!RANDOMIZER_STATE_VALID()) {
        bzero(&gRandomizerState, sizeof(gRandomizerState));
        gRandomizerState.settings = sDefaultSettings;
        gRandomizerState.opponentSettings = sDefaultSettings;
        gRandomizerState.opponentSeed = osGetCount();
        gRandomizerState.magic = RANDOMIZER_STATE_MAGIC;
        Randomizer_LoadSettings(&gRandomizerState);
    }
    return &gRandomizerState;
}

#endif
