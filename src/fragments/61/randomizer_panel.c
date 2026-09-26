/*
 * Options panel for the in-game randomizer on the Pokemon pick screen: C-Up opens it,
 * Up/Down picks an option, Left/Right or A changes it, B or C-Up closes it. The options
 * are the random team generator website's, plus the battle-select auto pick, and are
 * kept in gRandomizerState so they last until the console is switched off. Built only
 * with RANDOMIZER=1; empty otherwise so the default build still matches.
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
#define PANEL_Y 64
#define PANEL_W 440
#define PANEL_H 352
#define PANEL_COLOR 0x2121 // the blue of the pick screen's own menus
#define LINE_HEIGHT 28
#define FIRST_LINE (PANEL_Y + 52)

enum {
    OPTION_MOVESET,
    OPTION_TRADEBACK,
    OPTION_RANDOM_DVS,
    OPTION_RANDOM_STAT_EXP,
    OPTION_NO_LEGENDARIES,
    OPTION_FINAL_EVOS,
    OPTION_MONO_TYPE,
    OPTION_AUTO_BATTLE_PICK,
    OPTION_COUNT
};

static const char* sOptionNames[OPTION_COUNT] = {
    "Moveset",          "Tradeback moves", "Random DVs",     "Random Stat Exp",
    "No legendaries",   "Final evos only", "Mono-type team", "Auto battle pick",
};

static const char* sMovesetNames[] = { "Legal", "Stadium", "Chaos" };

static s32 sPanelOpen;
static s32 sCursor;

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
    } else {
        u8* flag = Randomizer_OptionFlag(state, option);

        *flag = !*flag;
    }
}

/*
 * Called at the top of the rental list's input handler (func_8420AA08). Returns 1 if the
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
        func_80048B90(4);
        return 1;
    }

    if (BTN_IS_PRESSED(cont, BTN_B | BTN_CUP)) {
        sPanelOpen = 0;
        D_84210D40 = 2;
        func_80048B90(3);
    } else if (BTN_IS_PRESSED(cont, BTN_DUP)) {
        sCursor = (sCursor + OPTION_COUNT - 1) % OPTION_COUNT;
        func_80048B90(1);
    } else if (BTN_IS_PRESSED(cont, BTN_DDOWN)) {
        sCursor = (sCursor + 1) % OPTION_COUNT;
        func_80048B90(1);
    } else if (BTN_IS_PRESSED(cont, BTN_DLEFT)) {
        Randomizer_ChangeOption(state, sCursor, -1);
        func_80048B90(2);
    } else if (BTN_IS_PRESSED(cont, BTN_DRIGHT | BTN_A)) {
        Randomizer_ChangeOption(state, sCursor, 1);
        func_80048B90(2);
    }
    return 1;
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
        const char* value = Randomizer_OptionValue(state, i);
        s32 y = FIRST_LINE + (i * LINE_HEIGHT);

        if (i == sCursor) {
            func_8001F324(0xFF, 0xFF, 0, 0xFF);
        } else {
            func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
        }
        func_8001F1E8(PANEL_X + 60, y, "%s", sOptionNames[i]);
        func_8001F1E8(PANEL_X + PANEL_W - 24 - func_8001F5B0(0, 0, "%s", value), y, "%s", value);
    }

    func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
    if (state->lastSeed != 0) {
        func_8001F1E8(PANEL_X + 24, PANEL_Y + PANEL_H - 40, "Last team's seed: %08X", state->lastSeed);
    } else {
        func_8001F1E8(PANEL_X + 24, PANEL_Y + PANEL_H - 40, "Z: random team");
    }

    func_8001F444();

    func_80020928(PANEL_X + 20, FIRST_LINE + (sCursor * LINE_HEIGHT));
}

#endif
