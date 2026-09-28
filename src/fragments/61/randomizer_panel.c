/*
 * Options panel for the in-game randomizer on the Pokemon pick screen: C-Up opens it,
 * Up/Down picks an option, Left/Right or A changes it, B or C-Up closes it. The options
 * are the random team generator website's, plus the battle-select auto pick, and are
 * kept in gRandomizerState (see randomizer_state.h) so they last until the console is
 * switched off. Built only with RANDOMIZER=1; empty otherwise so the default build
 * still matches.
 *
 * The last row takes a seed for the next team, as a website seed: A opens the eight
 * digits, Left/Right picks one, Up/Down changes it, A sets it and B backs out.
 * Left/Right on the row goes back to a random seed.
 */
#include "randomizer.h"

#ifdef RANDOMIZER

#include "src/1CF30.h"
#include "src/20470.h"
#include "src/49790.h"
#include "src/randomizer_state.h"

// Set to have the pick screen redraw everything (func_84202718)
extern s16 D_84210D40;

#define PANEL_X 100
#define PANEL_Y 44
#define PANEL_W 440
#define PANEL_H 392
#define PANEL_COLOR 0x2121 // the blue of the pick screen's own menus
#define LINE_HEIGHT 28
#define FIRST_LINE (PANEL_Y + 52)
#define VALUE_RIGHT (PANEL_X + PANEL_W - 24)
#define FOOTER_Y (PANEL_Y + PANEL_H - 40)

#define SEED_DIGITS 8

enum {
    OPTION_MOVESET,
    OPTION_TRADEBACK,
    OPTION_RANDOM_DVS,
    OPTION_RANDOM_STAT_EXP,
    OPTION_NO_LEGENDARIES,
    OPTION_FINAL_EVOS,
    OPTION_MONO_TYPE,
    OPTION_NO_SHARED_TYPES,
    OPTION_AUTO_BATTLE_PICK,
    OPTION_SEED,
    OPTION_COUNT
};

static const char* sOptionNames[OPTION_COUNT] = {
    "Moveset",         "Tradeback moves", "Random DVs",      "Random Stat Exp",  "No legendaries",
    "Final evos only", "Mono-type team",  "No shared types", "Auto battle pick", "Next team's seed",
};

static const char* sMovesetNames[] = { "Legal", "Stadium", "Chaos" };

static s32 sPanelOpen;
static s32 sCursor;
static s32 sEditingSeed;
static u32 sSeedDraft;
static s32 sSeedDigit; // 0 = leftmost

// When the pick screen starts
void Randomizer_PanelReset(void) {
    sPanelOpen = 0;
    sCursor = 0;
    sEditingSeed = 0;
}

s32 Randomizer_PanelIsOpen(void) {
    return sPanelOpen;
}

// The byte an on/off option lives in; Tradeback is shown the other way round
static u8* Randomizer_OptionFlag(RandomizerState* state, s32 option) {
    switch (option) {
        case OPTION_TRADEBACK:
            return &state->settings.noTradeback;
        case OPTION_RANDOM_DVS:
            return &state->settings.randomDvs;
        case OPTION_RANDOM_STAT_EXP:
            return &state->settings.randomStatExp;
        case OPTION_NO_LEGENDARIES:
            return &state->settings.noLegendaries;
        case OPTION_FINAL_EVOS:
            return &state->settings.finalEvosOnly;
        case OPTION_MONO_TYPE:
            return &state->settings.monoType;
        case OPTION_NO_SHARED_TYPES:
            return &state->settings.noSharedTypes;
        default:
            return &state->autoBattlePick;
    }
}

static const char* Randomizer_OptionValue(RandomizerState* state, s32 option) {
    s32 on;

    if (option == OPTION_MOVESET) {
        return sMovesetNames[state->settings.moveset];
    }
    on = *Randomizer_OptionFlag(state, option) != 0;
    if (option == OPTION_TRADEBACK) {
        on = !on;
    }
    return on ? "On" : "Off";
}

static void Randomizer_ChangeOption(RandomizerState* state, s32 option, s32 step) {
    if (option == OPTION_MOVESET) {
        state->settings.moveset = (state->settings.moveset + 3 + step) % 3;
    } else if (option == OPTION_SEED) {
        state->useEnteredSeed = 0;
    } else {
        u8* flag = Randomizer_OptionFlag(state, option);

        *flag = !*flag;

        // A mono-type team shares its type by definition, so each turns the other off
        if (*flag && (option == OPTION_MONO_TYPE)) {
            state->settings.noSharedTypes = 0;
        } else if (*flag && (option == OPTION_NO_SHARED_TYPES)) {
            state->settings.monoType = 0;
        }
    }
}

static s32 Randomizer_SeedDigit(u32 seed, s32 digit) {
    return (seed >> ((SEED_DIGITS - 1 - digit) * 4)) & 0xF;
}

static void Randomizer_SeedEditInput(RandomizerState* state, Controller* cont) {
    s32 shift = (SEED_DIGITS - 1 - sSeedDigit) * 4;
    s32 value = Randomizer_SeedDigit(sSeedDraft, sSeedDigit);

    if (BTN_IS_PRESSED(cont, BTN_A)) {
        state->enteredSeed = sSeedDraft;
        state->useEnteredSeed = 1;
        sEditingSeed = 0;
        func_80048B90(2);
    } else if (BTN_IS_PRESSED(cont, BTN_B)) {
        sEditingSeed = 0;
        func_80048B90(3);
    } else if (BTN_IS_PRESSED(cont, BTN_DLEFT)) {
        sSeedDigit = (sSeedDigit + SEED_DIGITS - 1) % SEED_DIGITS;
        func_80048B90(1);
    } else if (BTN_IS_PRESSED(cont, BTN_DRIGHT)) {
        sSeedDigit = (sSeedDigit + 1) % SEED_DIGITS;
        func_80048B90(1);
    } else if (BTN_IS_PRESSED(cont, BTN_DUP | BTN_DDOWN)) {
        value = (value + (BTN_IS_PRESSED(cont, BTN_DUP) ? 1 : 15)) & 0xF;
        sSeedDraft = (sSeedDraft & ~(0xF << shift)) | ((u32)value << shift);
        func_80048B90(1);
    }
}

/*
 * Called at the top of the rental list's input handler (Randomizer_ListInput). Returns 1 if the
 * panel took this frame's input: it was open, or C-Up just opened it.
 */
s32 Randomizer_PanelInput(Controller* cont) {
    RandomizerState* state = Randomizer_State();

    if (!sPanelOpen) {
        if (!BTN_IS_PRESSED(cont, BTN_CUP)) {
            return 0;
        }
        sPanelOpen = 1;
        sCursor = 0;
        sEditingSeed = 0;
        func_80048B90(4);
        return 1;
    }

    if (sEditingSeed) {
        Randomizer_SeedEditInput(state, cont);
    } else if (BTN_IS_PRESSED(cont, BTN_B | BTN_CUP)) {
        sPanelOpen = 0;
        D_84210D40 = 2;
        func_80048B90(3);
    } else if (BTN_IS_PRESSED(cont, BTN_DUP)) {
        sCursor = (sCursor + OPTION_COUNT - 1) % OPTION_COUNT;
        func_80048B90(1);
    } else if (BTN_IS_PRESSED(cont, BTN_DDOWN)) {
        sCursor = (sCursor + 1) % OPTION_COUNT;
        func_80048B90(1);
    } else if ((sCursor == OPTION_SEED) && BTN_IS_PRESSED(cont, BTN_A)) {
        // Start from the seed already entered, or else the last team's
        sSeedDraft = state->useEnteredSeed ? state->enteredSeed : state->lastSeed;
        sSeedDigit = 0;
        sEditingSeed = 1;
        func_80048B90(2);
    } else if (BTN_IS_PRESSED(cont, BTN_DLEFT)) {
        Randomizer_ChangeOption(state, sCursor, -1);
        func_80048B90(2);
    } else if (BTN_IS_PRESSED(cont, BTN_DRIGHT | BTN_A)) {
        Randomizer_ChangeOption(state, sCursor, 1);
        func_80048B90(2);
    }
    return 1;
}

// The seed row's value: "Random", the entered seed, or the seed being edited with the
// digit under the cursor in yellow
static void Randomizer_DrawSeedValue(RandomizerState* state, s32 y) {
    char digits[SEED_DIGITS + 1];
    s32 x;
    s32 i;

    if (!sEditingSeed) {
        if (state->useEnteredSeed) {
            func_8001F1E8(VALUE_RIGHT - func_8001F5B0(0, 0, "%08X", state->enteredSeed), y, "%08X", state->enteredSeed);
        } else {
            func_8001F1E8(VALUE_RIGHT - func_8001F5B0(0, 0, "Random"), y, "Random");
        }
        return;
    }

    sprintf(digits, "%08X", sSeedDraft);
    x = VALUE_RIGHT - func_8001F5B0(0, 0, "%s", digits);
    for (i = 0; i < SEED_DIGITS; i++) {
        if (i == sSeedDigit) {
            func_8001F324(0xFF, 0xFF, 0, 0xFF);
        } else {
            func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
        }
        func_8001F1E8(x, y, "%c", digits[i]);
        x += func_8001F5B0(0, 0, "%c", digits[i]);
    }
}

// Called at the end of the pick screen's drawing (func_84202718), every frame
void Randomizer_PanelDraw(void) {
    RandomizerState* state;
    s32 i;

    if (!sPanelOpen) {
        return;
    }
    state = Randomizer_State();

    func_80020460(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, PANEL_COLOR);

    func_8001F3F4();
    func_8001EBE0(0x10, 0);

    func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
    func_8001F1E8(PANEL_X + 24, PANEL_Y + 14, "Randomizer options");

    for (i = 0; i < OPTION_COUNT; i++) {
        s32 y = FIRST_LINE + (i * LINE_HEIGHT);

        if ((i == sCursor) && !sEditingSeed) {
            func_8001F324(0xFF, 0xFF, 0, 0xFF);
        } else {
            func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
        }
        func_8001F1E8(PANEL_X + 60, y, "%s", sOptionNames[i]);

        if (i == OPTION_SEED) {
            Randomizer_DrawSeedValue(state, y);
        } else {
            const char* value = Randomizer_OptionValue(state, i);

            func_8001F1E8(VALUE_RIGHT - func_8001F5B0(0, 0, "%s", value), y, "%s", value);
        }
    }

    func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
    if (sEditingSeed) {
        func_8001F1E8(PANEL_X + 24, FOOTER_Y, "Up/Down: digit  A: set  B: back");
    } else if (state->lastSeed != 0) {
        func_8001F1E8(PANEL_X + 24, FOOTER_Y, "Last team's seed: %08X", state->lastSeed);
    } else {
        func_8001F1E8(PANEL_X + 24, FOOTER_Y, "Z: random team");
    }

    func_8001F444();

    func_80020928(PANEL_X + 20, FIRST_LINE + (sCursor * LINE_HEIGHT));
}

#endif
