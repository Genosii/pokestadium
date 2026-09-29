/*
 * Options panels for the in-game randomizer on the Pokemon pick screen. C-Up opens the
 * team's options, C-Right the opponents'; Up/Down picks an option, Left/Right or A
 * changes it, B or the same C button closes the panel, the other one switches to it.
 * The team's options are the random team generator website's, plus the battle-select
 * auto pick; the opponents' are the mode (Normal, Factory or Rogue, see RandomizerMode)
 * and the same pool and move options, used for the trainers faced in cups and the Gym
 * Leader Castle when "Random opponents" is on. They're kept in
 * gRandomizerState (see randomizer_state.h) so they last until the console is switched
 * off. Built only with RANDOMIZER=1; empty otherwise so the default build still matches.
 *
 * The team panel's last row takes a seed for the next team, as a website seed: A opens
 * the eight digits, Left/Right picks one, Up/Down changes it, A sets it and B backs out.
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

// What a row shows and changes
enum {
    FIELD_MOVESET,
    FIELD_NO_TRADEBACK, // shown the other way round, as "Tradeback moves"
    FIELD_DVS,
    FIELD_STAT_EXP,
    FIELD_NO_LEGENDARIES, // shown the other way round, as "Legendaries"
    FIELD_FINAL_EVOS,
    FIELD_MONO_TYPE,
    FIELD_NO_SHARED_TYPES, // shown the other way round, as "Shared types"
    FIELD_AUTO_BATTLE_PICK,
    FIELD_RANDOM_OPPONENTS,
    FIELD_MODE,
    FIELD_SEED
};

typedef struct RandomizerRow {
    /* 0x0 */ const char* name;
    /* 0x4 */ s32 field;
} RandomizerRow; // size = 0x8

static RandomizerRow sTeamRows[] = {
    { "Moveset", FIELD_MOVESET },
    { "Tradeback moves", FIELD_NO_TRADEBACK },
    { "DVs", FIELD_DVS },
    { "Stat Exp", FIELD_STAT_EXP },
    { "Legendaries", FIELD_NO_LEGENDARIES },
    { "Final evos only", FIELD_FINAL_EVOS },
    { "Monotype", FIELD_MONO_TYPE },
    { "Shared types", FIELD_NO_SHARED_TYPES },
    { "Auto battle pick", FIELD_AUTO_BATTLE_PICK },
    { "Next team's seed", FIELD_SEED },
};

// The opponents' "Stadium" DVs and stat exp are their trainer's own
static RandomizerRow sOpponentRows[] = {
    { "Mode", FIELD_MODE },
    { "Random opponents", FIELD_RANDOM_OPPONENTS },
    { "Moveset", FIELD_MOVESET },
    { "Tradeback moves", FIELD_NO_TRADEBACK },
    { "DVs", FIELD_DVS },
    { "Stat Exp", FIELD_STAT_EXP },
    { "Legendaries", FIELD_NO_LEGENDARIES },
    { "Final evos only", FIELD_FINAL_EVOS },
    { "Monotype", FIELD_MONO_TYPE },
    { "Shared types", FIELD_NO_SHARED_TYPES },
};

typedef struct RandomizerPanel {
    /* 0x0 */ const char* title;
    /* 0x4 */ RandomizerRow* rows;
    /* 0x8 */ s32 numRows;
    /* 0xC */ u16 button; // opens and closes it
} RandomizerPanel;        // size = 0x10

enum { PANEL_TEAM, PANEL_OPPONENTS, PANEL_COUNT, PANEL_CLOSED = -1 };

static RandomizerPanel sPanels[PANEL_COUNT] = {
    { "Randomizer options", sTeamRows, ARRAY_COUNT(sTeamRows), BTN_CUP },
    { "Opponent options", sOpponentRows, ARRAY_COUNT(sOpponentRows), BTN_CRIGHT },
};

static const char* sMovesetNames[RANDOMIZER_MOVESET_COUNT] = { "Stadium", "Legal", "Strong", "Chaos" };
static const char* sStatSourceNames[RANDOMIZER_STATS_COUNT] = { "Stadium", "Max", "Random" };
static const char* sModeNames[RANDOMIZER_MODE_COUNT] = { "Normal", "Factory", "Rogue" };

// What each mode does, under the opponents' panel
static const char* sModeFooters[RANDOMIZER_MODE_COUNT] = {
    "Same seed, same trainers' teams",
    "Random foes, swap after each win",
    "Swaps, themed leaders, no retry",
};

static s32 sPanel;
static s32 sCursor;
static s32 sEditingSeed;
static u32 sSeedDraft;
static s32 sSeedDigit; // 0 = leftmost

// When the pick screen starts
void Randomizer_PanelReset(void) {
    sPanel = PANEL_CLOSED;
    sCursor = 0;
    sEditingSeed = 0;
}

s32 Randomizer_PanelIsOpen(void) {
    return sPanel != PANEL_CLOSED;
}

static RandomizerSettings* Randomizer_PanelSettings(RandomizerState* state) {
    return (sPanel == PANEL_OPPONENTS) ? &state->opponentSettings : &state->settings;
}

static RandomizerRow* Randomizer_Row(void) {
    return &sPanels[sPanel].rows[sCursor];
}

// The byte an on/off option lives in
static u8* Randomizer_Flag(RandomizerState* state, s32 field) {
    RandomizerSettings* settings = Randomizer_PanelSettings(state);

    switch (field) {
        case FIELD_NO_TRADEBACK:
            return &settings->noTradeback;
        case FIELD_DVS:
            return &settings->dvs;
        case FIELD_STAT_EXP:
            return &settings->statExp;
        case FIELD_NO_LEGENDARIES:
            return &settings->noLegendaries;
        case FIELD_FINAL_EVOS:
            return &settings->finalEvosOnly;
        case FIELD_MONO_TYPE:
            return &settings->monoType;
        case FIELD_NO_SHARED_TYPES:
            return &settings->noSharedTypes;
        case FIELD_AUTO_BATTLE_PICK:
            return &state->autoBattlePick;
        default:
            return &state->randomOpponents;
    }
}

static const char* Randomizer_FieldValue(RandomizerState* state, s32 field) {
    s32 on;

    if (field == FIELD_MOVESET) {
        return sMovesetNames[Randomizer_PanelSettings(state)->moveset];
    }
    if (field == FIELD_MODE) {
        return sModeNames[state->mode];
    }
    if ((field == FIELD_DVS) || (field == FIELD_STAT_EXP)) {
        return sStatSourceNames[*Randomizer_Flag(state, field)];
    }
    if ((field == FIELD_RANDOM_OPPONENTS) && (state->mode != RANDOMIZER_MODE_NORMAL)) {
        return "On"; // Factory and Rogue always have them
    }
    on = *Randomizer_Flag(state, field) != 0;
    if ((field == FIELD_NO_TRADEBACK) || (field == FIELD_NO_LEGENDARIES) || (field == FIELD_NO_SHARED_TYPES)) {
        on = !on;
    }
    return on ? "On" : "Off";
}

static void Randomizer_ChangeField(RandomizerState* state, s32 field, s32 step) {
    RandomizerSettings* settings = Randomizer_PanelSettings(state);

    if (field == FIELD_MOVESET) {
        settings->moveset = (settings->moveset + RANDOMIZER_MOVESET_COUNT + step) % RANDOMIZER_MOVESET_COUNT;
    } else if ((field == FIELD_DVS) || (field == FIELD_STAT_EXP)) {
        u8* source = Randomizer_Flag(state, field);

        *source = (*source + RANDOMIZER_STATS_COUNT + step) % RANDOMIZER_STATS_COUNT;
    } else if (field == FIELD_MODE) {
        state->mode = (state->mode + RANDOMIZER_MODE_COUNT + step) % RANDOMIZER_MODE_COUNT;
    } else if ((field == FIELD_RANDOM_OPPONENTS) && (state->mode != RANDOMIZER_MODE_NORMAL)) {
        // Stays on
    } else if (field == FIELD_SEED) {
        state->useEnteredSeed = 0;
    } else {
        u8* flag = Randomizer_Flag(state, field);

        *flag = !*flag;

        // A Monotype team shares its type by definition: Monotype turns Shared types back
        // on, and turning Shared types off turns Monotype off
        if (*flag && (field == FIELD_MONO_TYPE)) {
            settings->noSharedTypes = 0;
        } else if (*flag && (field == FIELD_NO_SHARED_TYPES)) {
            settings->monoType = 0;
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

static void Randomizer_OpenPanel(s32 panel) {
    sPanel = panel;
    sCursor = 0;
    sEditingSeed = 0;
    // The pick screen redraws everything, in case the other panel was bigger
    D_84210D40 = 2;
}

/*
 * Called at the top of the rental list's input handler (Randomizer_ListInput). Returns 1
 * if a panel took this frame's input: one was open, or its button just opened it.
 */
s32 Randomizer_PanelInput(Controller* cont) {
    RandomizerState* state = Randomizer_State();
    RandomizerPanel* panel;
    s32 i;

    if (sPanel == PANEL_CLOSED) {
        for (i = 0; i < PANEL_COUNT; i++) {
            if (BTN_IS_PRESSED(cont, sPanels[i].button)) {
                Randomizer_OpenPanel(i);
                func_80048B90(4);
                return 1;
            }
        }
        return 0;
    }
    panel = &sPanels[sPanel];

    if (sEditingSeed) {
        Randomizer_SeedEditInput(state, cont);
    } else if (BTN_IS_PRESSED(cont, BTN_B | panel->button)) {
        sPanel = PANEL_CLOSED;
        D_84210D40 = 2;
        func_80048B90(3);
    } else if (BTN_IS_PRESSED(cont, sPanels[PANEL_TEAM].button | sPanels[PANEL_OPPONENTS].button)) {
        Randomizer_OpenPanel((sPanel == PANEL_TEAM) ? PANEL_OPPONENTS : PANEL_TEAM);
        func_80048B90(4);
    } else if (BTN_IS_PRESSED(cont, BTN_DUP)) {
        sCursor = (sCursor + panel->numRows - 1) % panel->numRows;
        func_80048B90(1);
    } else if (BTN_IS_PRESSED(cont, BTN_DDOWN)) {
        sCursor = (sCursor + 1) % panel->numRows;
        func_80048B90(1);
    } else if ((Randomizer_Row()->field == FIELD_SEED) && BTN_IS_PRESSED(cont, BTN_A)) {
        // Start from the seed already entered, or else the last team's
        sSeedDraft = state->useEnteredSeed ? state->enteredSeed : state->lastSeed;
        sSeedDigit = 0;
        sEditingSeed = 1;
        func_80048B90(2);
    } else if (BTN_IS_PRESSED(cont, BTN_DLEFT)) {
        Randomizer_ChangeField(state, Randomizer_Row()->field, -1);
        func_80048B90(2);
    } else if (BTN_IS_PRESSED(cont, BTN_DRIGHT | BTN_A)) {
        Randomizer_ChangeField(state, Randomizer_Row()->field, 1);
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
    RandomizerPanel* panel;
    s32 i;

    if (sPanel == PANEL_CLOSED) {
        return;
    }
    state = Randomizer_State();
    panel = &sPanels[sPanel];

    func_80020460(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, PANEL_COLOR);

    func_8001F3F4();
    func_8001EBE0(0x10, 0);

    func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
    func_8001F1E8(PANEL_X + 24, PANEL_Y + 14, "%s", panel->title);

    for (i = 0; i < panel->numRows; i++) {
        s32 y = FIRST_LINE + (i * LINE_HEIGHT);
        s32 field = panel->rows[i].field;

        if ((i == sCursor) && !sEditingSeed) {
            func_8001F324(0xFF, 0xFF, 0, 0xFF);
        } else {
            func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
        }
        func_8001F1E8(PANEL_X + 60, y, "%s", panel->rows[i].name);

        if (field == FIELD_SEED) {
            Randomizer_DrawSeedValue(state, y);
        } else {
            const char* value = Randomizer_FieldValue(state, field);

            func_8001F1E8(VALUE_RIGHT - func_8001F5B0(0, 0, "%s", value), y, "%s", value);
        }
    }

    func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
    if (sEditingSeed) {
        func_8001F1E8(PANEL_X + 24, FOOTER_Y, "Up/Down: digit  A: set  B: back");
    } else if (sPanel == PANEL_OPPONENTS) {
        func_8001F1E8(PANEL_X + 24, FOOTER_Y, "%s", sModeFooters[state->mode]);
    } else if (state->lastSeed != 0) {
        func_8001F1E8(PANEL_X + 24, FOOTER_Y, "Last team's seed: %08X", state->lastSeed);
    } else {
        func_8001F1E8(PANEL_X + 24, FOOTER_Y, "Z: random team");
    }

    func_8001F444();

    func_80020928(PANEL_X + 20, FIRST_LINE + (sCursor * LINE_HEIGHT));
}

#endif
