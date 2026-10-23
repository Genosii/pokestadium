/*
 * Entry point of the randomizer's menu fragment (randomizer_menu, see randomizer_menu.h),
 * with the settings it shows: set up the first time they're needed after power-on, from
 * the save file if they were saved there, and saved again as a screen they were changed on
 * ends (its leave hook). Built only with RANDOMIZER=1; empty otherwise so the default build
 * still matches.
 *
 * The settings are saved at the end of the save file's third bank, in bytes the game never
 * looks at (src/randomizer_save.h), with a checksum of their own; and so are the Battle
 * Frontier's rules, which "Choose your rules" sets, and which a run puts in the settings'
 * place in gRandomizerState while it lasts (Randomizer_FrontierBegin), the player's own
 * read back from the save file once it's over.
 */
#include "randomizer_menu.h"

#ifdef RANDOMIZER

#include "src/randomizer_save.h"

#define SAVE_MAGIC 0x524E4453          // "RNDS"
#define FRONTIER_SAVE_MAGIC 0x524E4446 // "RNDF"

typedef struct RandomizerSave {
    /* 0x00 */ u32 magic;
    /* 0x04 */ RandomizerSettings settings;
    /* 0x0C */ RandomizerSettings opponentSettings;
    /* 0x14 */ u8 mode;
    /* 0x15 */ u8 randomOpponents;
    /* 0x16 */ u8 autoBattlePick;
    /* 0x17 */ u8 originalCamera;
    /* 0x18 */ u32 checksum;
} RandomizerSave; // size = 0x1C

typedef struct RandomizerFrontierSave {
    /* 0x00 */ u32 magic;
    /* 0x04 */ RandomizerFrontierRules rules[2]; // Factory's, then Rogue's
    /* 0x1C */ u32 checksum;
} RandomizerFrontierSave; // size = 0x20

// The website's defaults
// Stadium movesets, DVs and stat exp: the game's own rentals (and trainers)
static const RandomizerSettings sDefaultSettings = {
    RANDOMIZER_MOVESET_STADIUM, 0, RANDOMIZER_STATS_STADIUM, RANDOMIZER_STATS_STADIUM, 0, 0, 0, 0,
};

// Factory's rules to begin with, and Rogue's, which starts out from Pokemon that evolve
static const RandomizerFrontierRules sDefaultFrontierRules[2] = {
    { FRONTIER_CUP_POKE, 1, 0, RANDOMIZER_MOVESET_STADIUM, RANDOMIZER_MOVESET_STADIUM, 1, 0, 1, 0 },
    { FRONTIER_CUP_POKE, 1, 0, RANDOMIZER_MOVESET_STADIUM, RANDOMIZER_MOVESET_STADIUM, 1, 0, 0, 0 },
};

// The Stadium's rule sets (and modes) for "Choose your rules"' cups
static const u8 sFrontierRuleSets[FRONTIER_CUP_COUNT] = { 3, 6, 4, 5 };

// Gen 1's types, as the game numbers them, for the Monotype row
static const u8 sFrontierTypes[FRONTIER_MONO_TYPES] = { 0, 20, 21, 23, 22, 25, 1, 3, 4, 2, 24, 7, 5, 8, 26 };
static const char* sFrontierTypeNames[FRONTIER_MONO_TYPES + 1] = {
    "Off",    "Normal", "Fire",   "Water",   "Electric", "Grass", "Ice",   "Fighting",
    "Poison", "Ground", "Flying", "Psychic", "Bug",      "Rock",  "Ghost", "Dragon",
};

static RandomizerFrontierRules sFrontierRules[2];
static s32 sFrontierLoaded;

void Randomizer_MenuEntry(void) {
}

static u32 Randomizer_Checksum(void* data, u32 size, u32 magic) {
    u8* bytes = (u8*)data;
    u32 sum = magic;
    u32 i;

    // Everything before the checksum, the last field
    for (i = 0; i < size - sizeof(u32); i++) {
        sum = (sum * 31) + bytes[i];
    }
    return sum;
}

static u32 Randomizer_SaveChecksum(RandomizerSave* save) {
    return Randomizer_Checksum(save, sizeof(RandomizerSave), SAVE_MAGIC);
}

// Bank 2 as loaded, loading it first if no screen has; NULL if there's no memory for it
static RandomizerSave* Randomizer_SaveSlot(void) {
    if (!func_80028AFC(RANDOMIZER_SAVE_BANK)) {
        return NULL;
    }
    return (RandomizerSave*)RANDOMIZER_SAVE_AT(RANDOMIZER_SAVE_SETTINGS);
}

static void Randomizer_SaveFill(RandomizerSave* save, RandomizerState* state) {
    bzero(save, sizeof(RandomizerSave));
    save->magic = SAVE_MAGIC;
    save->settings = state->settings;
    save->opponentSettings = state->opponentSettings;
    save->mode = RANDOMIZER_MODE_NORMAL; // the options window's "Playstyle" is gone
    save->randomOpponents = state->randomOpponents;
    save->autoBattlePick = state->autoBattlePick;
    save->originalCamera = state->originalCamera;
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
        (save.mode >= RANDOMIZER_MODE_COUNT) || (save.originalCamera > (CAMERA_ORIGINAL | CAMERA_NO_REPLAY))) {
        return;
    }
    state->settings = save.settings;
    state->opponentSettings = save.opponentSettings;
    state->mode = RANDOMIZER_MODE_NORMAL; // Factory and Rogue are the Battle Frontier's now
    state->randomOpponents = save.randomOpponents;
    state->autoBattlePick = save.autoBattlePick;
    state->originalCamera = save.originalCamera;
}

void Randomizer_SaveSettings(void) {
    RandomizerSave save;
    RandomizerSave* slot;

    // A Battle Frontier run's rules are where the options are, until it's over
    if (gRandomizerState.frontier != FRONTIER_NONE) {
        return;
    }
    slot = Randomizer_SaveSlot();
    if (slot == NULL) {
        return;
    }
    Randomizer_SaveFill(&save, Randomizer_State());
    if (bcmp(slot, &save, sizeof(save)) == 0) {
        return;
    }
    bcopy(&save, slot, sizeof(save));
    RANDOMIZER_SAVE_WRITE();
}

// The player's own options: the defaults, then the save file's if it has them
static void Randomizer_OwnSettings(RandomizerState* state) {
    state->settings = sDefaultSettings;
    state->opponentSettings = sDefaultSettings;
    state->mode = RANDOMIZER_MODE_NORMAL;
    state->randomOpponents = 0;
    state->autoBattlePick = 0;
    Randomizer_LoadSettings(state);
}

// The settings, set up if they aren't there yet (see randomizer_state.h); after a Battle
// Frontier run (or a Reset during one), the player's own again
RandomizerState* Randomizer_State(void) {
    if (!RANDOMIZER_STATE_VALID()) {
        bzero(&gRandomizerState, sizeof(gRandomizerState));
        gRandomizerState.opponentSeed = osGetCount();
        gRandomizerState.magic = RANDOMIZER_STATE_MAGIC;
        Randomizer_OwnSettings(&gRandomizerState);
    } else if (gRandomizerState.frontier & FRONTIER_RESTORE) {
        Randomizer_FrontierEnd();
    }
    return &gRandomizerState;
}

/* The Battle Frontier's rules */

static RandomizerFrontierSave* Randomizer_FrontierSlot(void) {
    if (!func_80028AFC(RANDOMIZER_SAVE_BANK)) {
        return NULL;
    }
    return (RandomizerFrontierSave*)RANDOMIZER_SAVE_AT(RANDOMIZER_SAVE_FRONTIER);
}

static s32 Randomizer_FrontierRulesValid(RandomizerFrontierRules* rules) {
    return (rules->cup < FRONTIER_CUP_COUNT) && (rules->moveset < RANDOMIZER_MOVESET_COUNT) &&
           (rules->opponentMoveset < RANDOMIZER_MOVESET_COUNT) && (rules->monoType <= FRONTIER_MONO_TYPES);
}

// When a screen showing "Choose your rules" starts (this fragment's memory isn't cleared)
void Randomizer_FrontierReset(void) {
    sFrontierLoaded = 0;
}

RandomizerFrontierRules* Randomizer_FrontierRules(s32 kind) {
    RandomizerFrontierSave save;
    RandomizerFrontierSave* slot;
    s32 i;

    if (!sFrontierLoaded) {
        sFrontierLoaded = 1;
        bcopy(sDefaultFrontierRules, sFrontierRules, sizeof(sFrontierRules));
        slot = Randomizer_FrontierSlot();
        if (slot != NULL) {
            // Copied first: the bank's bytes there aren't necessarily aligned
            bcopy(slot, &save, sizeof(save));
            if ((save.magic == FRONTIER_SAVE_MAGIC) &&
                (save.checksum == Randomizer_Checksum(&save, sizeof(save), FRONTIER_SAVE_MAGIC))) {
                for (i = 0; i < 2; i++) {
                    if (Randomizer_FrontierRulesValid(&save.rules[i])) {
                        sFrontierRules[i] = save.rules[i];
                    }
                }
            }
        }
    }
    return &sFrontierRules[(kind == FRONTIER_ROGUE) ? 1 : 0];
}

void Randomizer_FrontierSave(void) {
    RandomizerFrontierSave save;
    RandomizerFrontierSave* slot;

    if (!sFrontierLoaded) {
        return;
    }
    slot = Randomizer_FrontierSlot();
    if (slot == NULL) {
        return;
    }
    bzero(&save, sizeof(save));
    save.magic = FRONTIER_SAVE_MAGIC;
    bcopy(sFrontierRules, save.rules, sizeof(save.rules));
    save.checksum = Randomizer_Checksum(&save, sizeof(save), FRONTIER_SAVE_MAGIC);
    if (bcmp(slot, &save, sizeof(save)) == 0) {
        return;
    }
    bcopy(&save, slot, sizeof(save));
    RANDOMIZER_SAVE_WRITE();
}

s32 Randomizer_FrontierRuleSet(s32 cup) {
    return sFrontierRuleSets[cup];
}

s32 Randomizer_FrontierType(s32 choice) {
    return sFrontierTypes[choice - 1];
}

const char* Randomizer_FrontierTypeName(s32 choice) {
    return sFrontierTypeNames[choice];
}

void Randomizer_FrontierBegin(s32 kind) {
    RandomizerState* state = Randomizer_State();
    RandomizerFrontierRules* rules = Randomizer_FrontierRules(kind);
    // Factory's Pokemon at their highest; Rogue's the rentals' and the trainers' own, whose
    // DVs and stat exp grow from ball to ball
    u8 stats = (kind == FRONTIER_FACTORY) ? RANDOMIZER_STATS_MAX : RANDOMIZER_STATS_STADIUM;
    RandomizerSettings settings;

    settings.moveset = rules->moveset;
    settings.noTradeback = !rules->tradeback;
    settings.dvs = stats;
    settings.statExp = stats;
    settings.noLegendaries = !rules->legendaries;
    settings.finalEvosOnly = rules->finalEvos ? 1 : RANDOMIZER_NOT_FINAL_EVOS;
    settings.monoType = 0;
    settings.noSharedTypes = 0;
    state->opponentSettings = settings;
    state->opponentSettings.moveset = rules->opponentMoveset;
    if (rules->monoType != 0) {
        settings.monoType = RANDOMIZER_MONO_TYPE(Randomizer_FrontierType(rules->monoType));
    }
    state->settings = settings;

    state->randomOpponents = rules->randomOpponents;
    state->autoBattlePick = rules->autoBattlePick;
    state->useEnteredSeed = 0;
    state->mode = RANDOMIZER_MODE_NORMAL;
    state->frontier = kind;
}

void Randomizer_FrontierEnd(void) {
    gRandomizerState.frontier = FRONTIER_NONE;
    Randomizer_OwnSettings(&gRandomizerState);
}

#endif
