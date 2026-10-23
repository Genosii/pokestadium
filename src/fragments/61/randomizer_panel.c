/*
 * The randomizer's options window, "Randomization options", in the randomizer_menu
 * fragment (see randomizer_menu.h). C-Up opens it on the Pokemon pick screen and Rules;
 * Options and Rules' Z open it too. It has three tabs, which L and R go through:
 *   - Mode: random opponents, the battle-select auto pick and the next team's seed;
 *   - Player: the random team generator website's options, for your team;
 *   - Opponent: the same, for the trainers faced in the cups and the Gym Leader Castle
 *     when opponents are random.
 * Up/Down picks an option, Left/Right or A changes it (holding the D-pad repeats), B or
 * C-Up closes the window. Under the options, two lines tell what the one picked does.
 * They're kept in gRandomizerState (see randomizer_state.h), and saved when the window
 * closes with them changed (randomizer_menu.c). Built only with RANDOMIZER=1; empty
 * otherwise so the default build still matches.
 *
 * The seed row takes a seed for the next team, as a website seed: A opens the eight
 * digits, Left/Right picks one, Up/Down changes it, A sets it and B backs out.
 * Left/Right on the row goes back to a random seed.
 *
 * "Choose your rules" is the same window for the Stadium's Battle Frontier modes, Factory
 * and Rogue (randomizer_stadium.c): one tab of their rules (RandomizerFrontierRules), two
 * rows of which the mode sets and can't be changed. Start begins the run, B goes back.
 */
#include "randomizer_menu.h"

#ifdef RANDOMIZER

#include "src/1CF30.h"
#include "src/20470.h"
#include "src/49790.h"

#define PANEL_X 100
#define PANEL_Y 24
#define PANEL_W 440
#define PANEL_H 428
#define PANEL_COLOR 0x2121 // the blue of the pick screen's own menus
#define LINE_HEIGHT 28
#define MAX_ROWS 8
#define TABS_Y (PANEL_Y + 48)
#define TAB_GAP 36
#define FIRST_LINE (PANEL_Y + 90)
#define VALUE_RIGHT (PANEL_X + PANEL_W - 24)
#define TEXT_X (PANEL_X + 24)
#define DESCRIPTION_LINE_HEIGHT 26
#define DESCRIPTION_LINE_MAX 48
#define FOOTER_Y (PANEL_Y + PANEL_H - 36)

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
    FIELD_SEED,
    // "Choose your rules"
    FIELD_CUP,
    FIELD_RULES_RANDOM_OPPONENTS,
    FIELD_RULES_AUTO_PICK,
    FIELD_RULES_MOVESET,
    FIELD_RULES_OPPONENT_MOVESET,
    FIELD_RULES_TRADEBACK,
    FIELD_RULES_STATS, // set by the mode
    FIELD_RULES_LEGENDARIES,
    FIELD_RULES_FINAL_EVOS,
    FIELD_RULES_MONO_TYPE
};

typedef struct RandomizerRow {
    /* 0x0 */ const char* name;
    /* 0x4 */ s32 field;
} RandomizerRow; // size = 0x8

static RandomizerRow sModeRows[] = {
    { "Random opponents", FIELD_RANDOM_OPPONENTS },
    { "Auto pick battle team", FIELD_AUTO_BATTLE_PICK },
    { "Team seed", FIELD_SEED },
};

// Your team's and the opponents' (whose "Stadium" DVs and stat exp are their trainer's own)
static RandomizerRow sTeamRows[] = {
    { "Moveset", FIELD_MOVESET },
    { "Tradeback", FIELD_NO_TRADEBACK },
    { "DVs", FIELD_DVS },
    { "Stat EXP", FIELD_STAT_EXP },
    { "Legendaries", FIELD_NO_LEGENDARIES },
    { "Final evos", FIELD_FINAL_EVOS },
    { "Monotype", FIELD_MONO_TYPE },
    { "Shared types", FIELD_NO_SHARED_TYPES },
};

static RandomizerRow sRulesRows[] = {
    { "Cup rules", FIELD_CUP },
    { "Random opponents", FIELD_RULES_RANDOM_OPPONENTS },
    { "Auto pick battle team", FIELD_RULES_AUTO_PICK },
    { "Player movesets", FIELD_RULES_MOVESET },
    { "Opponent movesets", FIELD_RULES_OPPONENT_MOVESET },
    { "Tradeback", FIELD_RULES_TRADEBACK },
    { "DVs and Stat EXP", FIELD_RULES_STATS },
    { "Legendaries", FIELD_RULES_LEGENDARIES },
    { "Final evos", FIELD_RULES_FINAL_EVOS },
    { "Monotype", FIELD_RULES_MONO_TYPE },
};

typedef struct RandomizerTab {
    /* 0x0 */ const char* name;
    /* 0x4 */ RandomizerRow* rows;
    /* 0x8 */ s32 numRows;
} RandomizerTab; // size = 0xC

#define TAB_MODE RANDOMIZER_TAB_MODE
#define TAB_PLAYER RANDOMIZER_TAB_PLAYER
#define TAB_OPPONENT RANDOMIZER_TAB_OPPONENT
#define TAB_COUNT RANDOMIZER_TAB_COUNT
#define TAB_FRONTIER RANDOMIZER_TAB_FRONTIER
#define PANEL_CLOSED RANDOMIZER_PANEL_CLOSED

static RandomizerTab sTabs[TAB_COUNT + 1] = {
    { "Mode", sModeRows, ARRAY_COUNT(sModeRows) },
    { "Player", sTeamRows, ARRAY_COUNT(sTeamRows) },
    { "Opponent", sTeamRows, ARRAY_COUNT(sTeamRows) },
    { "Choose your rules", sRulesRows, ARRAY_COUNT(sRulesRows) },
};

// "Choose your rules" has more rows and no tabs: its rows start higher, closer together
#define RULES_FIRST_LINE (PANEL_Y + 56)
#define RULES_LINE_HEIGHT 26

#define PANEL_BUTTON BTN_CUP // opens and closes the window on the pick screen and Rules

static const char* sMovesetNames[RANDOMIZER_MOVESET_COUNT] = { "Stadium", "Legal", "Strong", "Chaos" };
static const char* sStatSourceNames[RANDOMIZER_STATS_COUNT] = { "Stadium", "Max", "Random" };
static const char* sCupNames[FRONTIER_CUP_COUNT] = { "Pok\xE9 Cup", "Prime Cup", "Petit Cup", "Pika Cup" };

// What each option does, under the options. Those whose values do different things have a
// line for each value.
static const char* sMovesetDescriptions[RANDOMIZER_MOVESET_COUNT] = {
    "The moves of the game's own\nrental Pok\xE9mon",
    "Four random moves the Pok\xE9mon\ncan learn",
    "Its strongest moves of its own\ntypes, then coverage and support",
    "Any four moves at all",
};
static const char* sTeamDvDescriptions[RANDOMIZER_STATS_COUNT] = {
    "The DVs of the game's own\nrental Pok\xE9mon",
    "Every DV at 15",
    "Every DV at random, 0 to 15",
};
static const char* sTeamStatExpDescriptions[RANDOMIZER_STATS_COUNT] = {
    "The stat exp of the game's own\nrental Pok\xE9mon",
    "Every stat's exp at 65535",
    "Every stat's exp at random",
};
static const char* sOpponentDvDescriptions[RANDOMIZER_STATS_COUNT] = {
    "The trainer's own DVs, which grow\nfrom cup to cup and gym to gym",
    "Every DV at 15",
    "Every DV at random, 0 to 15",
};
static const char* sOpponentStatExpDescriptions[RANDOMIZER_STATS_COUNT] = {
    "The trainer's own stat exp, which\ngrows from cup to cup",
    "Every stat's exp at 65535",
    "Every stat's exp at random",
};
static const char* sCupDescriptions[FRONTIER_CUP_COUNT] = {
    "Levels 50 to 55: the Pok\xE9 Cup's\ntrainers and rules",
    "Level 100: the Prime Cup's\ntrainers and rules",
    "Levels 25 to 30: the Petit Cup's\nsmall Pok\xE9mon and trainers",
    "Levels 15 to 20: the Pika Cup's\ntrainers and rules",
};

static s32 sPanel; // the tab shown, or PANEL_CLOSED
static s32 sLastTab;
static s32 sCursor;
static s32 sEditingSeed;
static s32 sFrontierKind; // whose rules "Choose your rules" shows
static u32 sSeedDraft;
static s32 sSeedDigit; // 0 = leftmost
static void (*sRedraw)(void);

// When a screen showing the panels starts
void Randomizer_PanelReset(void (*redraw)(void)) {
    sPanel = PANEL_CLOSED;
    sLastTab = TAB_MODE;
    sCursor = 0;
    sEditingSeed = 0;
    sRedraw = redraw;
}

static void Randomizer_Redraw(void) {
    if (sRedraw != NULL) {
        sRedraw();
    }
}

s32 Randomizer_PanelIsOpen(void) {
    return sPanel != PANEL_CLOSED;
}

static RandomizerSettings* Randomizer_PanelSettings(RandomizerState* state) {
    return (sPanel == TAB_OPPONENT) ? &state->opponentSettings : &state->settings;
}

static RandomizerRow* Randomizer_Row(void) {
    return &sTabs[sPanel].rows[sCursor];
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

// The byte a "Choose your rules" option lives in
static u8* Randomizer_RulesField(RandomizerFrontierRules* rules, s32 field) {
    switch (field) {
        case FIELD_CUP:
            return &rules->cup;
        case FIELD_RULES_RANDOM_OPPONENTS:
            return &rules->randomOpponents;
        case FIELD_RULES_AUTO_PICK:
            return &rules->autoBattlePick;
        case FIELD_RULES_MOVESET:
            return &rules->moveset;
        case FIELD_RULES_OPPONENT_MOVESET:
            return &rules->opponentMoveset;
        case FIELD_RULES_TRADEBACK:
            return &rules->tradeback;
        case FIELD_RULES_LEGENDARIES:
            return &rules->legendaries;
        case FIELD_RULES_FINAL_EVOS:
            return &rules->finalEvos;
        default:
            return &rules->monoType;
    }
}

// How many values a "Choose your rules" option goes through (on/off for the rest)
static s32 Randomizer_RulesCount(s32 field) {
    switch (field) {
        case FIELD_CUP:
            return FRONTIER_CUP_COUNT;
        case FIELD_RULES_MOVESET:
        case FIELD_RULES_OPPONENT_MOVESET:
            return RANDOMIZER_MOVESET_COUNT;
        case FIELD_RULES_MONO_TYPE:
            return FRONTIER_MONO_TYPES + 1;
        default:
            return 2;
    }
}

static const char* Randomizer_RulesValue(s32 field) {
    u8 value;

    if (field == FIELD_RULES_STATS) {
        return (sFrontierKind == FRONTIER_FACTORY) ? "Max" : "Build up";
    }
    value = *Randomizer_RulesField(Randomizer_FrontierRules(sFrontierKind), field);
    switch (field) {
        case FIELD_CUP:
            return sCupNames[value];
        case FIELD_RULES_MOVESET:
        case FIELD_RULES_OPPONENT_MOVESET:
            return sMovesetNames[value];
        case FIELD_RULES_MONO_TYPE:
            return Randomizer_FrontierTypeName(value);
        default:
            return value ? "On" : "Off";
    }
}

static const char* Randomizer_FieldValue(RandomizerState* state, s32 field) {
    s32 on;

    if (field >= FIELD_CUP) {
        return Randomizer_RulesValue(field);
    }
    if (field == FIELD_MOVESET) {
        return sMovesetNames[Randomizer_PanelSettings(state)->moveset];
    }
    if ((field == FIELD_DVS) || (field == FIELD_STAT_EXP)) {
        return sStatSourceNames[*Randomizer_Flag(state, field)];
    }
    on = *Randomizer_Flag(state, field) != 0;
    if ((field == FIELD_NO_TRADEBACK) || (field == FIELD_NO_LEGENDARIES) || (field == FIELD_NO_SHARED_TYPES)) {
        on = !on;
    }
    return on ? "On" : "Off";
}

static void Randomizer_ChangeField(RandomizerState* state, s32 field, s32 step) {
    RandomizerSettings* settings = Randomizer_PanelSettings(state);

    if (field >= FIELD_CUP) {
        u8* value = Randomizer_RulesField(Randomizer_FrontierRules(sFrontierKind), field);
        s32 count = Randomizer_RulesCount(field);

        *value = (*value + count + step) % count;
    } else if (field == FIELD_MOVESET) {
        settings->moveset = (settings->moveset + RANDOMIZER_MOVESET_COUNT + step) % RANDOMIZER_MOVESET_COUNT;
    } else if ((field == FIELD_DVS) || (field == FIELD_STAT_EXP)) {
        u8* source = Randomizer_Flag(state, field);

        *source = (*source + RANDOMIZER_STATS_COUNT + step) % RANDOMIZER_STATS_COUNT;
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

void Randomizer_PanelOpen(s32 tab) {
    sPanel = tab;
    sCursor = 0;
    sEditingSeed = 0;
    Randomizer_Redraw();
}

static void Randomizer_PanelClose(void) {
    sLastTab = sPanel;
    sPanel = PANEL_CLOSED;
    Randomizer_Redraw();
}

/*
 * Called with the screen's input (on the pick screen, at the top of the rental list's input
 * handler, Randomizer_ListInput). Returns 1 if the window took this frame's input: it was
 * open, or C-Up just opened it (on the tab it was last closed on).
 */
s32 Randomizer_PanelInput(Controller* cont) {
    RandomizerState* state = Randomizer_State();
    RandomizerTab* tab;
    u16 dpad;

    if (sPanel == PANEL_CLOSED) {
        if (BTN_IS_PRESSED(cont, PANEL_BUTTON)) {
            Randomizer_PanelOpen(sLastTab);
            func_80048B90(4);
            return 1;
        }
        return 0;
    }
    tab = &sTabs[sPanel];

    if (sEditingSeed) {
        Randomizer_SeedEditInput(state, cont);
        return 1;
    }
    dpad = Randomizer_Repeat(cont, RANDOMIZER_DPAD);
    if (BTN_IS_PRESSED(cont, BTN_B | PANEL_BUTTON)) {
        func_80048B90(3);
        Randomizer_PanelClose();
    } else if (BTN_IS_PRESSED(cont, BTN_L | BTN_R)) {
        sPanel = (sPanel + TAB_COUNT + (BTN_IS_PRESSED(cont, BTN_L) ? -1 : 1)) % TAB_COUNT;
        if (sCursor >= sTabs[sPanel].numRows) {
            sCursor = sTabs[sPanel].numRows - 1;
        }
        Randomizer_Redraw();
        func_80048B90(1);
    } else if (dpad & (BTN_DUP | BTN_DDOWN)) {
        // A press goes round from one end to the other; holding stops at the end
        s32 row = sCursor + ((dpad & BTN_DUP) ? -1 : 1);

        if (BTN_IS_PRESSED(cont, dpad)) {
            sCursor = (row + tab->numRows) % tab->numRows;
        } else if ((row >= 0) && (row < tab->numRows)) {
            sCursor = row;
        } else {
            return 1;
        }
        func_80048B90(1);
    } else if ((Randomizer_Row()->field == FIELD_SEED) && BTN_IS_PRESSED(cont, BTN_A)) {
        // Start from the seed already entered, or else the last team's
        sSeedDraft = state->useEnteredSeed ? state->enteredSeed : state->lastSeed;
        sSeedDigit = 0;
        sEditingSeed = 1;
        func_80048B90(2);
    } else if (dpad & BTN_DLEFT) {
        Randomizer_ChangeField(state, Randomizer_Row()->field, -1);
        func_80048B90(2);
    } else if ((dpad & BTN_DRIGHT) || BTN_IS_PRESSED(cont, BTN_A)) {
        Randomizer_ChangeField(state, Randomizer_Row()->field, 1);
        func_80048B90(2);
    }
    return 1;
}

void Randomizer_FrontierPanelOpen(s32 kind) {
    sFrontierKind = kind;
    Randomizer_PanelOpen(TAB_FRONTIER);
}

/*
 * "Choose your rules"' input, while it's open: Up/Down picks a rule, Left/Right or A changes
 * it (the mode's own can't be), Start begins the run (1) and B goes back (-1); 0 otherwise.
 */
s32 Randomizer_FrontierPanelInput(Controller* cont) {
    s32 numRows = sTabs[TAB_FRONTIER].numRows;
    s32 field = sRulesRows[sCursor].field;
    u16 dpad = Randomizer_Repeat(cont, RANDOMIZER_DPAD);

    if (BTN_IS_PRESSED(cont, BTN_START)) {
        func_80048B90(2);
        sPanel = PANEL_CLOSED;
        return 1;
    }
    if (BTN_IS_PRESSED(cont, BTN_B)) {
        func_80048B90(3);
        sPanel = PANEL_CLOSED;
        return -1;
    }
    if (dpad & (BTN_DUP | BTN_DDOWN)) {
        s32 row = sCursor + ((dpad & BTN_DUP) ? -1 : 1);

        if (BTN_IS_PRESSED(cont, dpad)) {
            sCursor = (row + numRows) % numRows;
        } else if ((row >= 0) && (row < numRows)) {
            sCursor = row;
        } else {
            return 0;
        }
        func_80048B90(1);
    } else if ((dpad & (BTN_DLEFT | BTN_DRIGHT)) || BTN_IS_PRESSED(cont, BTN_A)) {
        if (field == FIELD_RULES_STATS) {
            func_80048B90(8); // the mode's
        } else {
            Randomizer_ChangeField(Randomizer_State(), field, (dpad & BTN_DLEFT) ? -1 : 1);
            func_80048B90(2);
        }
    }
    return 0;
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

// Held for a quarter of a second, then eight times a second (timed by the CPU's counter,
// since screens run at different frame rates)
#define REPEAT_DELAY (OS_CPU_COUNTER / 4)
#define REPEAT_EVERY (OS_CPU_COUNTER / 8)

u16 Randomizer_Repeat(Controller* cont, u16 buttons) {
    static u16 sHeld;
    static u32 sNext;
    u16 pressed = cont->buttonPressed & buttons;
    u32 now = osGetCount();

    if (pressed != 0) {
        sHeld = pressed;
        sNext = now + REPEAT_DELAY;
        return pressed;
    }
    if ((cont->buttonDown & sHeld) != sHeld) {
        sHeld = 0;
        return 0;
    }
    if ((sHeld == 0) || ((s32)(now - sNext) < 0)) {
        return 0;
    }
    sNext = now + REPEAT_EVERY;
    return sHeld;
}

// What the option on the cursor's row does
static const char* Randomizer_Description(RandomizerState* state, s32 field) {
    static char sSeedDescription[DESCRIPTION_LINE_MAX * 2];
    RandomizerSettings* settings = Randomizer_PanelSettings(state);
    RandomizerFrontierRules* rules;
    s32 opponents = sPanel == TAB_OPPONENT;

    if (field >= FIELD_CUP) {
        rules = Randomizer_FrontierRules(sFrontierKind);
        switch (field) {
            case FIELD_CUP:
                return sCupDescriptions[rules->cup];
            case FIELD_RULES_RANDOM_OPPONENTS:
                return "Trainers get random teams, made\nwith the opponents' rules";
            case FIELD_RULES_AUTO_PICK:
                return "Picks a random three by itself\nwhen a battle's team is chosen";
            case FIELD_RULES_MOVESET:
                return sMovesetDescriptions[rules->moveset];
            case FIELD_RULES_OPPONENT_MOVESET:
                return sMovesetDescriptions[rules->opponentMoveset];
            case FIELD_RULES_TRADEBACK:
                return "Gen 1 moves a Pok\xE9mon only learns\nin Gold/Silver, then traded back";
            case FIELD_RULES_STATS:
                return (sFrontierKind == FRONTIER_FACTORY)
                           ? "Factory: every DV at 15, every\nstat's exp at 65535"
                           : "Rogue: the rentals' own; the\ntrainers' grow ball to ball";
            case FIELD_RULES_LEGENDARIES:
                return "Articuno, Zapdos, Moltres, Mewtwo\nand Mew can be picked";
            case FIELD_RULES_FINAL_EVOS:
                return rules->finalEvos ? "Only Pok\xE9mon that don't evolve\nany further"
                                        : "Pok\xE9mon that evolve further and\nsingle-stage ones; no legendaries";
            default:
                if (rules->monoType == 0) {
                    return "Your team of any types";
                }
                sprintf(sSeedDescription, "Your whole team of the %s type,\nas far as the cup allows",
                        Randomizer_FrontierTypeName(rules->monoType));
                return sSeedDescription;
        }
    }

    switch (field) {
        case FIELD_MOVESET:
            return sMovesetDescriptions[settings->moveset];
        case FIELD_NO_TRADEBACK:
            return "Gen 1 moves a Pok\xE9mon only learns\nin Gold/Silver, then traded back";
        case FIELD_DVS:
            return (opponents ? sOpponentDvDescriptions : sTeamDvDescriptions)[settings->dvs];
        case FIELD_STAT_EXP:
            return (opponents ? sOpponentStatExpDescriptions : sTeamStatExpDescriptions)[settings->statExp];
        case FIELD_NO_LEGENDARIES:
            return "Articuno, Zapdos, Moltres, Mewtwo\nand Mew can be picked";
        case FIELD_FINAL_EVOS:
            return "Only Pok\xE9mon that don't evolve\nany further";
        case FIELD_MONO_TYPE:
            return "The whole team shares a type";
        case FIELD_NO_SHARED_TYPES:
            return "Off: no two Pok\xE9mon have a\ntype in common";
        case FIELD_AUTO_BATTLE_PICK:
            return "Picks a random three by itself\nwhen a battle's team is chosen";
        case FIELD_RANDOM_OPPONENTS:
            return "Trainers in the cups and the\nCastle get random teams";
        default:
            if (state->lastSeed != 0) {
                sprintf(sSeedDescription, "A: type a website seed\nLast team's seed: %08X", state->lastSeed);
                return sSeedDescription;
            }
            return "A: type a website seed, for the\nsame team as on the website";
    }
}

// Text with line breaks, a line at a time
static void Randomizer_DrawLines(s32 x, s32 y, const char* text) {
    char line[DESCRIPTION_LINE_MAX];
    s32 length;

    while (*text != '\0') {
        length = 0;
        while ((*text != '\0') && (*text != '\n') && (length < DESCRIPTION_LINE_MAX - 1)) {
            line[length++] = *text++;
        }
        line[length] = '\0';
        func_8001F1E8(x, y, "%s", line);
        y += DESCRIPTION_LINE_HEIGHT;
        if (*text == '\n') {
            text++;
        }
    }
}

// Called at the end of the screen's drawing, every frame
void Randomizer_PanelDraw(void) {
    RandomizerState* state;
    RandomizerTab* tab;
    s32 rules = sPanel == TAB_FRONTIER;
    s32 firstLine = rules ? RULES_FIRST_LINE : FIRST_LINE;
    s32 lineHeight = rules ? RULES_LINE_HEIGHT : LINE_HEIGHT;
    s32 x;
    s32 i;

    if (sPanel == PANEL_CLOSED) {
        return;
    }
    state = Randomizer_State();
    tab = &sTabs[sPanel];

    func_80020460(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, PANEL_COLOR);

    func_8001F3F4();
    func_8001EBE0(0x10, 0);

    func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
    if (rules) {
        // The mode's name at the right, in yellow
        const char* mode = (sFrontierKind == FRONTIER_FACTORY) ? "Factory" : "Rogue";

        func_8001F1E8(TEXT_X, PANEL_Y + 14, "%s", tab->name);
        func_8001F324(0xFF, 0xFF, 0, 0xFF);
        func_8001F1E8(VALUE_RIGHT - func_8001F5B0(0, 0, "%s", mode), PANEL_Y + 14, "%s", mode);
    } else {
        func_8001F1E8(TEXT_X, PANEL_Y + 14, "Randomization options");

        // The tabs, the one shown in yellow, between L and R
        func_8001F324(0xC8, 0xC8, 0xC8, 0xFF);
        func_8001F1E8(TEXT_X, TABS_Y, "L");
        x = TEXT_X + func_8001F5B0(0, 0, "L") + TAB_GAP;
        for (i = 0; i < TAB_COUNT; i++) {
            if (i == sPanel) {
                func_8001F324(0xFF, 0xFF, 0, 0xFF);
            } else {
                func_8001F324(0x9C, 0x9C, 0x9C, 0xFF);
            }
            func_8001F1E8(x, TABS_Y, "%s", sTabs[i].name);
            x += func_8001F5B0(0, 0, "%s", sTabs[i].name) + TAB_GAP;
        }
        func_8001F324(0xC8, 0xC8, 0xC8, 0xFF);
        func_8001F1E8(x, TABS_Y, "R");
    }

    for (i = 0; i < tab->numRows; i++) {
        s32 y = firstLine + (i * lineHeight);
        s32 field = tab->rows[i].field;

        if ((i == sCursor) && !sEditingSeed) {
            func_8001F324(0xFF, 0xFF, 0, 0xFF);
        } else {
            func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
        }
        func_8001F1E8(PANEL_X + 60, y, "%s", tab->rows[i].name);

        if (field == FIELD_SEED) {
            Randomizer_DrawSeedValue(state, y);
        } else {
            const char* value = Randomizer_FieldValue(state, field);

            // The mode's own rule, which can't be changed, in grey
            if (field == FIELD_RULES_STATS) {
                func_8001F324(0xA0, 0xA0, 0xA0, 0xFF);
            }
            func_8001F1E8(VALUE_RIGHT - func_8001F5B0(0, 0, "%s", value), y, "%s", value);
        }
    }

    // What the option does, in light blue
    func_8001F324(0x9C, 0xDC, 0xFF, 0xFF);
    Randomizer_DrawLines(TEXT_X, firstLine + ((rules ? tab->numRows : MAX_ROWS) * lineHeight) + 8,
                         Randomizer_Description(state, tab->rows[sCursor].field));

    func_8001F324(0xC8, 0xC8, 0xC8, 0xFF);
    if (sEditingSeed) {
        func_8001F1E8(TEXT_X, FOOTER_Y, "Up/Down: digit   A: set   B: back");
    } else if (rules) {
        func_8001F1E8(TEXT_X, FOOTER_Y, "Start: begin   B: back");
    } else {
        func_8001F1E8(TEXT_X, FOOTER_Y, "L/R: tab   B: close");
    }

    func_8001F444();

    func_80020928(PANEL_X + 20, firstLine + (sCursor * lineHeight));
}

#endif
