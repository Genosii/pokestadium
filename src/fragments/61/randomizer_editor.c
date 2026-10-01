/*
 * The teambuilder: once a team is complete on the Pokemon pick screen, "Edit Pokemon" in the
 * team's menu (with OK and Reselect, in the cups and in Registration) opens a window to
 * change each Pokemon's moves, level, DVs and stat exp. The team is changed as it's edited,
 * so "OK to Register", or Registration's OK, saves it as it is. Built only with
 * RANDOMIZER=1; empty otherwise so the default build still matches.
 *
 * Moves come from a list of the moves the Pokemon can legally learn (its learnset, as the
 * website and the "Legal" moveset have it, with the moves only Gold and Silver teach left
 * out when "Tradeback moves" is off), so there's no cycling through all 165. Levels stay
 * within the cup's range and at or above the lowest level the Pokemon can legally have.
 * Stat exp goes in the steps that change the stat: a stat gets a quarter of the square root
 * of its stat exp, 0 to 63 points.
 *
 * Controls: Up/Down picks a line; Left/Right changes it; A opens the move list on a move, or
 * sets the level, a DV or a stat exp to its highest (and back to its lowest if it's there);
 * Z sets every DV and stat exp to the highest; L/R go to the previous or next Pokemon; B
 * closes the window. In the move list, Up/Down and Left/Right (a page) move, A picks and B
 * goes back.
 */
#include "randomizer.h"

#ifdef RANDOMIZER

#include "src/1CF30.h"
#include "src/20470.h"
#include "src/22630.h"
#include "src/232C0.h"
#include "src/49790.h"
#include "src/controller.h"

// fragment61_2E2C20.c: per type, the summary's colours and type icon
typedef struct unk_D_842115F0 {
    /* 0x00 */ Color_RGB8 unk_00[4];
    /* 0x0C */ u8* unk_0C;
} unk_D_842115F0; // size = 0x10
extern unk_D_842115F0 D_842115F0[];

// fragment61_2E4040.c: the screen's menus, and the state of the open one
extern unk_D_84211704 D_84211704[];
extern s16 D_84211700;
extern s16 D_8423E580;
extern s16 D_8423E588;
extern s16 D_8423E58A;

// Set to have the pick screen redraw everything (func_84202718)
extern s16 D_84210D40;
extern unk_D_842168A0 D_842168A0;

#define WINDOW_X 40
#define WINDOW_Y 16
#define WINDOW_W 560
#define WINDOW_H 448
#define WINDOW_COLOR 0x2121 // the blue of the pick screen's own menus
#define TEXT_X (WINDOW_X + 56)
#define VALUE_RIGHT (WINDOW_X + 330)
#define SIDE_X (WINDOW_X + 360)
#define SIDE_RIGHT (WINDOW_X + WINDOW_W - 24)
#define FIRST_LINE (WINDOW_Y + 48)
#define LINE_HEIGHT 24
#define FOOTER_Y (WINDOW_Y + WINDOW_H - 70)

#define LIST_X 240
#define LIST_Y 40
#define LIST_W 360
#define LIST_H 400
#define LIST_ROWS 13
#define LIST_TEXT_X (LIST_X + 62)
#define LIST_FIRST_LINE (LIST_Y + 48)
#define LIST_POWER_RIGHT (LIST_X + LIST_W - 84)
#define LIST_ACCURACY_RIGHT (LIST_X + LIST_W - 24)

#define ICON_SIZE 0x14

#define TEAM_SIZE 6
#define NUM_MOVES 4
#define NUM_DVS 4      // Attack, Defense, Speed, Special; HP's follows from them
#define NUM_STAT_EXP 5 // HP, Attack, Defense, Speed, Special
#define MAX_DV 15
#define MAX_POINTS 63 // what stat exp adds to a stat at most
#define MAX_STAT_EXP 0xFFFF
#define MAX_PP_UPS (3 << 6)
#define MAX_LEARNSET 80

// The menus "Edit Pokemon" is added to, and the level-sum message's state (func_84207530)
#define MENU_OK 0
#define MENU_REGISTER 10
#define MENU_LINE_HEIGHT 0x1C
#define MENU_SOUND_CONFIRM 2 // unk_0E: three bits a line, the sound the line makes
#define TEAM_STATE_LEVEL_SUM 15
#define TEAM_STATE_PICK 3 // waiting for a Pokemon to be picked (func_84207BD4)
#define TEAM_STATE_MENU_OK 7
#define TEAM_STATE_MENU_REGISTER 14
// The rental list's state while its card is open (func_8420A0E4 reads the answer)
#define LIST_STATE_CARD 11

// The rental card's prompt (func_8420B40C, func_8420C368): when it asks whether to add the
// Pokemon (CARD_ENTER, CARD_USE; not CARD_EXCHANGE), "Edit" goes between Yes and No. The
// card's answer, unk_02, is 0 for Yes and 1 for No; it's CARD_EDIT while the cursor is on
// Edit, which answers Yes and opens the teambuilder on the Pokemon once it's in the team.
#define CARD_ENTER 1
#define CARD_EXCHANGE 2
#define CARD_USE 3
#define CARD_YES 0
#define CARD_NO 1
#define CARD_EDIT 2
#define CARD_LINES 3
#define CARD_LINE_HEIGHT 0x16
#define CARD_SOUND_YES 0x22
// "Check registered Pokemon", of the modes of the registered teams' viewer (func_84203BBC)
#define VIEWER_CHECK 1

enum {
    ROW_MOVE_1,
    ROW_MOVE_4 = ROW_MOVE_1 + NUM_MOVES - 1,
    ROW_LEVEL,
    ROW_DV_1,
    ROW_DV_4 = ROW_DV_1 + NUM_DVS - 1,
    ROW_STAT_EXP_1,
    ROW_STAT_EXP_5 = ROW_STAT_EXP_1 + NUM_STAT_EXP - 1,
    ROW_COUNT
};

static const char* sRowNames[ROW_COUNT] = {
    "Move 1",          "Move 2",           "Move 3",         "Move 4",           "Level",
    "Attack DV",       "Defense DV",       "Speed DV",       "Special DV",       "HP stat exp",
    "Attack stat exp", "Defense stat exp", "Speed stat exp", "Special stat exp",
};

static const char* sStatNames[NUM_STAT_EXP] = { "HP", "Attack", "Defense", "Speed", "Special" };

// What's being edited: the team being entered, or a registered team seen in "Check
// registered Pokemon" (sSet, of sViewer)
static s32 sOpen;
static unk_D_838067F0_0168_0000* sSlots;
static s32 sCount;
static s32 sController;
static unk_D_84211B50* sTeam;
static s32 sReturnState; // the team's state to go back to
static unk_D_84229EB0* sViewer;
static unk_D_84229EB0_00024* sSet;
static s32 sInCheck; // "Check registered Pokemon" is taking input, for the control hints
static s32 sMon;
static s32 sRow;
static s32 sListOpen;
static s32 sListCount;
static s32 sListTop;
static s32 sListCursor;
static u8 sList[MAX_LEARNSET + 1];          // 0: no move
static u8 sListTradeback[MAX_LEARNSET + 1]; // only learnable through Gold and Silver
static s32 sEditSlot; // the team slot "Edit" in the rental card adds to, until it's there, or -1
static s32 sJustClosed; // the button that closed it is still this frame's, for the rental list

// The card's answers, top to bottom, when it has "Edit"
static u8 sCardLines[CARD_LINES] = { CARD_YES, CARD_EDIT, CARD_NO };

// When the pick screen starts: adds "Edit Pokemon" to the team's menus, which the screen
// loads afresh each time
void Randomizer_EditorReset(void) {
    unk_D_84211704* menu = &D_84211704[MENU_OK];

    sOpen = 0;
    sTeam = NULL;
    sViewer = NULL;
    sInCheck = 0;
    sListOpen = 0;
    sEditSlot = -1;
    sJustClosed = 0;

    menu->unk_0A = RANDOMIZER_EDIT_LINE_MENU_0;
    menu->unk_06 += MENU_LINE_HEIGHT;
    menu->unk_0E |= MENU_SOUND_CONFIRM << ((RANDOMIZER_EDIT_LINE_MENU_0 - 1) * 3);

    // Registration's already has the room
    menu = &D_84211704[MENU_REGISTER];
    menu->unk_0A = RANDOMIZER_EDIT_LINE_MENU_10;
    menu->unk_0E |= MENU_SOUND_CONFIRM << ((RANDOMIZER_EDIT_LINE_MENU_10 - 1) * 3);
}

s32 Randomizer_EditorIsOpen(void) {
    return sOpen;
}

// For the rental list, which takes input after the team does: 1 while the teambuilder is
// open, and in the frame it closed in, so the B that closed it doesn't also take a
// Pokemon back out of the team
s32 Randomizer_EditorTakesListInput(void) {
    s32 closed = sJustClosed;

    sJustClosed = 0;
    return sOpen || closed;
}

static unk_func_80026268_arg0* Randomizer_EditorMon(void) {
    return &sSlots[sMon].unk_004;
}

static s32 Randomizer_NumMoves(unk_func_80026268_arg0* mon) {
    s32 i;

    for (i = 0; (i < NUM_MOVES) && (mon->unk_09[i] != 0); i++) {}
    return i;
}

// The DV the row shows: Attack, Defense, Speed, Special from the top bits down
static s32 Randomizer_Dv(unk_func_80026268_arg0* mon, s32 i) {
    return (mon->unk_1E >> ((NUM_DVS - 1 - i) * 4)) & 0xF;
}

static void Randomizer_SetDv(unk_func_80026268_arg0* mon, s32 i, s32 value) {
    s32 shift = (NUM_DVS - 1 - i) * 4;

    mon->unk_1E = (mon->unk_1E & ~(0xF << shift)) | (value << shift);
}

static s32 Randomizer_HpDv(unk_func_80026268_arg0* mon) {
    s32 hp = 0;
    s32 i;

    for (i = 0; i < NUM_DVS; i++) {
        hp = (hp << 1) | (Randomizer_Dv(mon, i) & 1);
    }
    return hp;
}

static u16* Randomizer_StatExp(unk_func_80026268_arg0* mon, s32 i) {
    return &mon->unk_14 + i;
}

// What stat exp adds to a stat: a quarter of its square root, rounded up, at most 255
static s32 Randomizer_Points(u32 statExp) {
    u32 root = 0;

    while ((root * root) < statExp) {
        root++;
    }
    return ((root > 255) ? 255 : root) / 4;
}

// The least stat exp that adds that much, and the most for the top
static u16 Randomizer_StatExpFor(s32 points) {
    if (points <= 0) {
        return 0;
    }
    if (points >= MAX_POINTS) {
        return MAX_STAT_EXP;
    }
    return (((points * 4) - 1) * ((points * 4) - 1)) + 1;
}

// The levels the cup allows the Pokemon, and at least its lowest legal one
static void Randomizer_LevelRange(unk_func_80026268_arg0* mon, s32* min, s32* max) {
    RandomizerRules rules;
    s32 legal = gRandomizerSpecies[mon->unk_00.unk_00].minLevel;

    Randomizer_GetRules(D_800AE540.unk_0001, mon->unk_24, &rules);
    if (rules.levelMax != 0) {
        *min = rules.levelMin;
        *max = rules.levelMax;
    } else {
        *min = 1;
        *max = 100;
    }
    if (*min < legal) {
        *min = legal;
    }
    if (*min > *max) {
        *min = *max = mon->unk_24;
    }
}

/*
 * func_84206A68, moved here from fragment61 to make room there, over any six Pokemon:
 * whether the three lowest levels add up to more than the cup allows (Poke Cup 155, Petit
 * Cup 80, Pika Cup 50).
 */
static s32 Randomizer_LevelSumOver(unk_D_838067F0_0168_0000* slots, s32 count) {
    s32 level;
    s16 i;
    s16 lowest[3];

    for (i = 0; i < 3; i++) {
        lowest[i] = 999;
    }

    for (i = 0; i < count; i++) {
        level = slots[i].unk_004.unk_24;
        if (lowest[2] >= level) {
            lowest[2] = level;
        }
        if (lowest[1] >= level) {
            lowest[2] = lowest[1];
            lowest[1] = level;
        }
        if (lowest[0] >= level) {
            lowest[1] = lowest[0];
            lowest[0] = level;
        }
    }

    level = lowest[0] + lowest[1] + lowest[2];
    switch (D_800AE540.unk_0001) {
        case 3:
            return level >= 156;
        case 4:
            return level >= 81;
        case 5:
            return level >= 51;
        default:
            return 0;
    }
}

s32 Randomizer_LevelSumTooHigh(unk_D_84211B50* team) {
    return Randomizer_LevelSumOver(team->unk_0030, team->unk_0006);
}

// For the warning: the whole team's levels, once it's complete (a Pokemon edited from the
// rental card is one of a team still being picked), or the registered team's
static s32 Randomizer_EditorLevelSumOver(void) {
    if (sTeam == NULL) {
        return Randomizer_LevelSumOver(sSlots, sCount);
    }
    if (sReturnState == TEAM_STATE_PICK) {
        return 0;
    }
    return Randomizer_LevelSumTooHigh(sTeam);
}

// The level, stats, HP and PP from everything else, as the game works them out
static void Randomizer_Recalculate(unk_func_80026268_arg0* mon) {
    func_80022734(mon);
}

static void Randomizer_SetLevel(unk_func_80026268_arg0* mon, s32 level) {
    mon->unk_10 = func_800224B8(mon->unk_00.unk_00, level);
    Randomizer_Recalculate(mon);
}

static s32 Randomizer_Knows(unk_func_80026268_arg0* mon, s32 move, s32 exceptSlot) {
    s32 i;

    for (i = 0; i < NUM_MOVES; i++) {
        if ((i != exceptSlot) && (mon->unk_09[i] == move)) {
            return 1;
        }
    }
    return 0;
}

// Puts move in a slot, keeping the moves together at the front; 0 takes the slot's away
static void Randomizer_SetMove(unk_func_80026268_arg0* mon, s32 slot, s32 move) {
    s32 count = Randomizer_NumMoves(mon);
    s32 i;

    if (slot > count) {
        slot = count;
    }
    if (move != 0) {
        mon->unk_09[slot] = move;
        mon->unk_20[slot] = MAX_PP_UPS;
    } else if (slot < count) {
        for (i = slot; i < NUM_MOVES - 1; i++) {
            mon->unk_09[i] = mon->unk_09[i + 1];
            mon->unk_20[i] = mon->unk_20[i + 1];
        }
        mon->unk_09[NUM_MOVES - 1] = 0;
        mon->unk_20[NUM_MOVES - 1] = 0;
    }
    Randomizer_Recalculate(mon);
}

static s32 Randomizer_NameBefore(const char* a, const char* b) {
    while ((*a != '\0') && (*a == *b)) {
        a++;
        b++;
    }
    return (u8)*a < (u8)*b;
}

// The moves the Pokemon can learn, by name, with "no move" first for any but the first slot
static void Randomizer_BuildList(void) {
    unk_func_80026268_arg0* mon = Randomizer_EditorMon();
    const RandomizerSpecies* species = &gRandomizerSpecies[mon->unk_00.unk_00];
    s32 noTradeback = Randomizer_State()->settings.noTradeback;
    s32 first = 0;
    s32 current = mon->unk_09[sRow - ROW_MOVE_1];
    s32 i;
    s32 j;

    sListCount = 0;
    if (sRow != ROW_MOVE_1) {
        sList[sListCount] = 0;
        sListTradeback[sListCount] = 0;
        sListCount++;
        first = 1;
    }
    for (i = 0; (i < species->learnsetCount) && (sListCount < ARRAY_COUNT(sList)); i++) {
        const RandomizerLearnsetMove* learn = &gRandomizerLearnsets[species->learnsetStart + i];

        if (noTradeback && !learn->gen1) {
            continue;
        }
        // Insertion by name
        for (j = sListCount;
             (j > first) && Randomizer_NameBefore(func_842000C0(learn->move), func_842000C0(sList[j - 1])); j--) {
            sList[j] = sList[j - 1];
            sListTradeback[j] = sListTradeback[j - 1];
        }
        sList[j] = learn->move;
        sListTradeback[j] = !learn->gen1;
        sListCount++;
    }

    sListCursor = 0;
    for (i = 0; i < sListCount; i++) {
        if (sList[i] == current) {
            sListCursor = i;
        }
    }
    sListTop = sListCursor - (LIST_ROWS / 2);
    if (sListTop > sListCount - LIST_ROWS) {
        sListTop = sListCount - LIST_ROWS;
    }
    if (sListTop < 0) {
        sListTop = 0;
    }
}

static void Randomizer_EditorStart(unk_D_838067F0_0168_0000* slots, s32 count, s32 controller) {
    sOpen = 1;
    sSlots = slots;
    sCount = count;
    sController = controller;
    sMon = 0;
    sRow = ROW_MOVE_1;
    sListOpen = 0;
    func_80048B90(4);
}

// "Edit Pokemon" picked in the team's menu
void Randomizer_EditorOpen(unk_D_84211B50* team) {
    if (sOpen) {
        return;
    }
    sTeam = team;
    sViewer = NULL;
    sReturnState = team->unk_0001;
    team->unk_0001 = RANDOMIZER_TEAM_STATE_EDIT;
    Randomizer_EditorStart(team->unk_0030, team->unk_0006, team->unk_0002);
}

/*
 * A registered team, edited: written back over itself, the way the game registers a team
 * (func_84206990) but at its own place, with its own name and ID, then read back for the
 * screen as the game reads it (func_8420F204).
 */
static void Randomizer_SaveSet(void) {
    unk_func_80022C28_ret* save = func_80022CC0(0x10, 0, sSet->unk_4D1E, 0, sSet->unk_4D12, sSet->unk_4D10);
    s32 i;

    if (save != NULL) {
        for (i = 0; i < sSet->unk_4D20; i++) {
            func_80022F24(&sSet->unk_0000[i].unk_004.unk_00.unk_00, 1, save);
        }
        func_80022D8C(save);
        func_800286D8();
    }
    func_8420F204(sSet, sSet->unk_4D1E);
}

static void Randomizer_EditorClose(void) {
    if (sTeam != NULL) {
        // What the game does once the last Pokemon is picked: the cup's level-sum rule
        // first (func_84207190). Not while the team is still being picked.
        if ((sReturnState != TEAM_STATE_PICK) && Randomizer_LevelSumTooHigh(sTeam)) {
            sTeam->unk_0001 = TEAM_STATE_LEVEL_SUM;
        } else {
            sTeam->unk_0001 = sReturnState;
        }
    } else {
        Randomizer_SaveSet();
    }
    sOpen = 0;
    sJustClosed = 1;
    sTeam = NULL;
    sViewer = NULL;
    D_84210D40 = 2;
    func_80048B90(3);
}

static void Randomizer_MoveListInput(Controller* cont) {
    unk_func_80026268_arg0* mon = Randomizer_EditorMon();
    s32 slot = sRow - ROW_MOVE_1;
    s32 step = 0;
    u16 dpad;

    if (BTN_IS_PRESSED(cont, BTN_A)) {
        s32 move = sList[sListCursor];

        if ((move != 0) && Randomizer_Knows(mon, move, slot)) {
            func_80048B90(8);
            return;
        }
        Randomizer_SetMove(mon, slot, move);
        sListOpen = 0;
        D_84210D40 = 2;
        func_80048B90(2);
        return;
    }
    if (BTN_IS_PRESSED(cont, BTN_B)) {
        sListOpen = 0;
        D_84210D40 = 2;
        func_80048B90(3);
        return;
    }
    dpad = Randomizer_Repeat(cont, RANDOMIZER_DPAD);
    if (dpad & BTN_DUP) {
        step = -1;
    } else if (dpad & BTN_DDOWN) {
        step = 1;
    } else if (dpad & BTN_DLEFT) {
        step = -LIST_ROWS;
    } else if (dpad & BTN_DRIGHT) {
        step = LIST_ROWS;
    }
    if (step == 0) {
        return;
    }
    if (((step == 1) || (step == -1)) && BTN_IS_PRESSED(cont, dpad)) {
        // A press goes round from one end to the other; holding stops at the end
        sListCursor = (sListCursor + sListCount + step) % sListCount;
    } else {
        sListCursor += step;
        if (sListCursor < 0) {
            sListCursor = 0;
        } else if (sListCursor >= sListCount) {
            sListCursor = sListCount - 1;
        }
    }
    if (sListCursor < sListTop) {
        sListTop = sListCursor;
    } else if (sListCursor >= sListTop + LIST_ROWS) {
        sListTop = sListCursor - LIST_ROWS + 1;
    }
    func_80048B90(1);
}

// Left/Right (step -1 or 1), or A (step 0: to the highest, or back to the lowest)
static void Randomizer_ChangeRow(unk_func_80026268_arg0* mon, s32 step) {
    s32 min;
    s32 max;
    s32 value;

    if (sRow == ROW_LEVEL) {
        Randomizer_LevelRange(mon, &min, &max);
        value = (step == 0) ? ((mon->unk_24 == max) ? min : max) : (mon->unk_24 + step);
        if ((value < min) || (value > max) || (value == mon->unk_24)) {
            func_80048B90(8);
            return;
        }
        Randomizer_SetLevel(mon, value);
    } else if (sRow <= ROW_DV_4) {
        s32 dv = sRow - ROW_DV_1;

        value = Randomizer_Dv(mon, dv);
        value = (step == 0) ? ((value == MAX_DV) ? 0 : MAX_DV) : ((value + step + MAX_DV + 1) % (MAX_DV + 1));
        Randomizer_SetDv(mon, dv, value);
        Randomizer_Recalculate(mon);
    } else {
        u16* statExp = Randomizer_StatExp(mon, sRow - ROW_STAT_EXP_1);
        s32 points = Randomizer_Points(*statExp);

        if (step == 0) {
            *statExp = (*statExp == MAX_STAT_EXP) ? 0 : MAX_STAT_EXP;
        } else {
            points += step;
            if ((points < 0) || (points > MAX_POINTS)) {
                func_80048B90(8);
                return;
            }
            *statExp = Randomizer_StatExpFor(points);
        }
        Randomizer_Recalculate(mon);
    }
    func_80048B90(1);
}

static void Randomizer_EditorKeys(void) {
    Controller* cont = &gControllers[sController];
    unk_func_80026268_arg0* mon;
    u16 dpad;
    s32 i;

    if (sListOpen) {
        Randomizer_MoveListInput(cont);
        return;
    }
    mon = Randomizer_EditorMon();
    dpad = Randomizer_Repeat(cont, RANDOMIZER_DPAD);

    if (BTN_IS_PRESSED(cont, BTN_B)) {
        Randomizer_EditorClose();
    } else if (BTN_IS_PRESSED(cont, BTN_L | BTN_R)) {
        sMon = (sMon + sCount + (BTN_IS_PRESSED(cont, BTN_L) ? -1 : 1)) % sCount;
        func_80048B90(1);
    } else if (dpad & (BTN_DUP | BTN_DDOWN)) {
        // A press goes round from one end to the other; holding stops at the end
        s32 row = sRow + ((dpad & BTN_DUP) ? -1 : 1);

        if (BTN_IS_PRESSED(cont, dpad)) {
            sRow = (row + ROW_COUNT) % ROW_COUNT;
        } else if ((row >= 0) && (row < ROW_COUNT)) {
            sRow = row;
        } else {
            return;
        }
        func_80048B90(1);
    } else if (BTN_IS_PRESSED(cont, BTN_Z)) {
        for (i = 0; i < NUM_DVS; i++) {
            Randomizer_SetDv(mon, i, MAX_DV);
        }
        for (i = 0; i < NUM_STAT_EXP; i++) {
            *Randomizer_StatExp(mon, i) = MAX_STAT_EXP;
        }
        Randomizer_Recalculate(mon);
        func_80048B90(2);
    } else if (BTN_IS_PRESSED(cont, BTN_A)) {
        if (sRow <= ROW_MOVE_4) {
            Randomizer_BuildList();
            if (sListCount == 0) {
                func_80048B90(8);
                return;
            }
            sListOpen = 1;
            func_80048B90(4);
        } else {
            Randomizer_ChangeRow(mon, 0);
        }
    } else if ((sRow > ROW_MOVE_4) && (dpad & (BTN_DLEFT | BTN_DRIGHT))) {
        Randomizer_ChangeRow(mon, (dpad & BTN_DLEFT) ? -1 : 1);
    }
}

// The team's input while it's edited (func_8420776C, in the team's edit state)
void Randomizer_EditorInput(unk_D_84211B50* team) {
    team->unk_0018 = team->unk_0014;
    team->unk_001A = team->unk_0016;
    if (sOpen && (team == sTeam)) {
        Randomizer_EditorKeys();
    }
}

/*
 * Called by "Check registered Pokemon" as it takes input on the teams (func_8420F86C), with
 * the one highlighted: Z edits it, and while it's edited, the editor takes the input.
 * Returns 1 if it took this frame's input.
 */
s32 Randomizer_EditorCheckInput(unk_D_84229EB0* viewer, unk_D_84229EB0_00024* set) {
    Controller* cont = &gControllers[viewer->unk_00003];

    if (sOpen) {
        if (viewer == sViewer) {
            Randomizer_EditorKeys();
        }
        return 1;
    }
    if (viewer->unk_00000 != VIEWER_CHECK) {
        return 0;
    }
    sInCheck = 1;
    if (!BTN_IS_PRESSED(cont, BTN_Z)) {
        return 0;
    }
    if (set->unk_4D20 <= 0) {
        func_80048B90(8);
        return 1;
    }
    sTeam = NULL;
    sViewer = viewer;
    sSet = set;
    Randomizer_EditorStart(set->unk_0000, set->unk_4D20, viewer->unk_00003);
    return 1;
}

static void Randomizer_MoveColor(s32 move, s32 dim) {
    Color_RGB8* color = &D_842115F0[D_80072338[move - 1].unk_01].unk_00[2];

    if (dim) {
        func_8001F324(color->r / 2, color->g / 2, color->b / 2, 0xFF);
    } else {
        func_8001F324(color->r, color->g, color->b, 0xFF);
    }
}

static void Randomizer_RowColor(s32 selected) {
    func_8001F324(0xFF, 0xFF, selected ? 0 : 0xFF, 0xFF);
}

static void Randomizer_Right(s32 right, s32 y, const char* text) {
    func_8001F1E8(right - func_8001F5B0(0, 0, "%s", text), y, "%s", text);
}

static void Randomizer_DrawMoveInfo(s32 move, s32 y) {
    const unk_D_80072B00* data = &D_80072B00[move - 1];
    char text[16];

    func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
    func_8001F1E8(SIDE_X, y, "Power");
    if (data->unk_02 > 1) {
        sprintf(text, "%d", data->unk_02);
    } else {
        sprintf(text, "-");
    }
    Randomizer_Right(SIDE_RIGHT, y, text);
    func_8001F1E8(SIDE_X, y + LINE_HEIGHT, "Accuracy");
    sprintf(text, "%d%%", ((data->unk_04 * 100) + 127) / 255);
    Randomizer_Right(SIDE_RIGHT, y + LINE_HEIGHT, text);
    func_8001F1E8(SIDE_X, y + (LINE_HEIGHT * 2), "PP");
    sprintf(text, "%d", data->unk_05);
    Randomizer_Right(SIDE_RIGHT, y + (LINE_HEIGHT * 2), text);
}

static void Randomizer_DrawList(void) {
    unk_func_80026268_arg0* mon = Randomizer_EditorMon();
    s32 slot = sRow - ROW_MOVE_1;
    char text[16];
    s32 i;

    func_80020460(LIST_X, LIST_Y, LIST_W, LIST_H, WINDOW_COLOR);

    func_8001F3F4();
    func_8001EBE0(0x10, 0);
    func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
    func_8001F1E8(LIST_X + 24, LIST_Y + 14, "Moves");
    func_8001EBE0(8, 0);
    Randomizer_Right(LIST_POWER_RIGHT, LIST_Y + 18, "Pow");
    Randomizer_Right(LIST_ACCURACY_RIGHT, LIST_Y + 18, "Acc");

    func_8001EBE0(0x10, 0);
    for (i = sListTop; (i < sListCount) && (i < sListTop + LIST_ROWS); i++) {
        s32 y = LIST_FIRST_LINE + ((i - sListTop) * LINE_HEIGHT);
        s32 move = sList[i];

        if (move == 0) {
            func_8001F324(0xC8, 0xC8, 0xC8, 0xFF);
            func_8001F1E8(LIST_TEXT_X, y, "(no move)");
            continue;
        }
        Randomizer_MoveColor(move, Randomizer_Knows(mon, move, slot));
        func_8001F1E8(LIST_TEXT_X, y, "%s%s", func_842000C0(move), sListTradeback[i] ? "*" : "");
        func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
        if (D_80072B00[move - 1].unk_02 > 1) {
            sprintf(text, "%d", D_80072B00[move - 1].unk_02);
        } else {
            sprintf(text, "-");
        }
        Randomizer_Right(LIST_POWER_RIGHT, y, text);
        sprintf(text, "%d", ((D_80072B00[move - 1].unk_04 * 100) + 127) / 255);
        Randomizer_Right(LIST_ACCURACY_RIGHT, y, text);
    }

    func_8001EBE0(8, 0);
    func_8001F324(0xC8, 0xC8, 0xC8, 0xFF);
    func_8001F1E8(LIST_X + 24, LIST_Y + LIST_H - 30, "%d/%d  * trade from Gold/Silver", sListCursor + 1, sListCount);
    func_8001F444();

    // Type icons, as the summary draws them next to moves
    gSPDisplayList(gDisplayListHead++, D_8006F518);
    for (i = sListTop; (i < sListCount) && (i < sListTop + LIST_ROWS); i++) {
        if (sList[i] != 0) {
            func_8001C6AC(LIST_TEXT_X - ICON_SIZE - 6, LIST_FIRST_LINE + ((i - sListTop) * LINE_HEIGHT) - 2, ICON_SIZE,
                          ICON_SIZE, D_842115F0[D_80072338[sList[i] - 1].unk_01].unk_0C, ICON_SIZE, 0);
        }
    }
    gSPDisplayList(gDisplayListHead++, D_8006F630);

    func_80020928(LIST_X + 4, LIST_FIRST_LINE + ((sListCursor - sListTop) * LINE_HEIGHT) - 4);
}

// "Edit Pokemon" in the team's menus, which draw their own lines only (func_8420C844,
// func_8420CA00)
static void Randomizer_DrawMenuLine(void) {
    unk_D_84211704* menu;
    s32 line;

    if ((D_84211700 == 0) || (D_8423E588 < 4)) {
        return;
    }
    if (D_8423E580 == MENU_OK) {
        line = RANDOMIZER_EDIT_LINE_MENU_0 - 1;
    } else if (D_8423E580 == MENU_REGISTER) {
        line = RANDOMIZER_EDIT_LINE_MENU_10 - 1;
    } else {
        return;
    }
    menu = &D_84211704[D_8423E580];
    func_8001F3F4();
    func_8001EBE0(0x10, 0);
    func_8420C7B0(menu->unk_00, menu->unk_02, line, "Edit Pok\xE9mon");
    func_8001F444();
}

// With "Edit Pokemon", the OK menu reaches the bottom of the screen, over the control hints
s32 Randomizer_EditorMenuCoversHints(void) {
    return (D_84211700 != 0) && (D_8423E580 == MENU_OK);
}

// Whether "Check registered Pokemon" took input this frame, where Z edits the team; asked
// once a frame by the control hints
s32 Randomizer_EditorInCheck(void) {
    s32 inCheck = sInCheck;

    sInCheck = 0;
    return inCheck;
}

// At the end of the pick screen's drawing, every frame
void Randomizer_EditorDraw(void) {
    unk_func_80026268_arg0* mon;
    s32 nameWidths[NUM_MOVES];
    char text[32];
    s32 i;

    if (!sOpen) {
        Randomizer_DrawMenuLine();
        return;
    }
    mon = Randomizer_EditorMon();

    func_80020460(WINDOW_X, WINDOW_Y, WINDOW_W, WINDOW_H, WINDOW_COLOR);

    func_8001F3F4();
    func_8001EBE0(0x10, 0);
    func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
    func_8001F1E8(WINDOW_X + 24, WINDOW_Y + 14, "%s  L%d", mon->unk_30, mon->unk_24);
    sprintf(text, "L/R  %d/%d", sMon + 1, sCount);
    Randomizer_Right(SIDE_RIGHT, WINDOW_Y + 14, text);

    for (i = 0; i < ROW_COUNT; i++) {
        s32 y = FIRST_LINE + (i * LINE_HEIGHT);

        Randomizer_RowColor((i == sRow) && !sListOpen);
        func_8001F1E8(TEXT_X, y, "%s", sRowNames[i]);

        if (i <= ROW_MOVE_4) {
            s32 move = mon->unk_09[i - ROW_MOVE_1];

            if (move != 0) {
                Randomizer_MoveColor(move, 0);
                nameWidths[i] = func_8001F5B0(0, 0, "%s", func_842000C0(move));
                Randomizer_Right(VALUE_RIGHT, y, func_842000C0(move));
            } else {
                Randomizer_Right(VALUE_RIGHT, y, "-");
            }
            continue;
        }
        if (i == ROW_LEVEL) {
            sprintf(text, "%d", mon->unk_24);
        } else if (i <= ROW_DV_4) {
            sprintf(text, "%d", Randomizer_Dv(mon, i - ROW_DV_1));
        } else {
            sprintf(text, "%d", *Randomizer_StatExp(mon, i - ROW_STAT_EXP_1));
        }
        Randomizer_Right(VALUE_RIGHT, y, text);
    }

    // The stats as they come out, and the move on the cursor's line
    func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
    func_8001F1E8(SIDE_X, FIRST_LINE, "Stats");
    for (i = 0; i < NUM_STAT_EXP; i++) {
        s32 y = FIRST_LINE + ((i + 1) * LINE_HEIGHT);

        func_8001F1E8(SIDE_X, y, "%s", sStatNames[i]);
        sprintf(text, "%d", (&mon->unk_26)[i]);
        Randomizer_Right(SIDE_RIGHT, y, text);
    }
    func_8001F1E8(SIDE_X, FIRST_LINE + (6 * LINE_HEIGHT), "HP DV");
    sprintf(text, "%d", Randomizer_HpDv(mon));
    Randomizer_Right(SIDE_RIGHT, FIRST_LINE + (6 * LINE_HEIGHT), text);
    if ((sRow <= ROW_MOVE_4) && (mon->unk_09[sRow - ROW_MOVE_1] != 0)) {
        Randomizer_DrawMoveInfo(mon->unk_09[sRow - ROW_MOVE_1], FIRST_LINE + (8 * LINE_HEIGHT));
    }

    func_8001EBE0(8, 0);
    if (Randomizer_EditorLevelSumOver()) {
        func_8001F324(0xFF, 0x60, 0x60, 0xFF);
        func_8001F1E8(WINDOW_X + 24, FOOTER_Y, "The three lowest levels add up to more than the cup allows");
    }
    func_8001F324(0xC8, 0xC8, 0xC8, 0xFF);
    if (sRow <= ROW_MOVE_4) {
        func_8001F1E8(WINDOW_X + 24, FOOTER_Y + 22, "A: pick a move   L/R: other Pok\xE9mon   B: done");
    } else {
        func_8001F1E8(WINDOW_X + 24, FOOTER_Y + 22, "Left/Right: change   A: highest or lowest   B: done");
    }
    func_8001F1E8(WINDOW_X + 24, FOOTER_Y + 40, "Z: every DV and stat exp to the highest");
    func_8001F444();

    // Type icons next to the moves
    gSPDisplayList(gDisplayListHead++, D_8006F518);
    for (i = 0; i < NUM_MOVES; i++) {
        s32 move = mon->unk_09[i];

        if (move != 0) {
            func_8001C6AC(VALUE_RIGHT - nameWidths[i] - ICON_SIZE - 6, FIRST_LINE + (i * LINE_HEIGHT) - 2, ICON_SIZE,
                          ICON_SIZE, D_842115F0[D_80072338[move - 1].unk_01].unk_0C, ICON_SIZE, 0);
        }
    }
    gSPDisplayList(gDisplayListHead++, D_8006F630);

    if (!sListOpen) {
        func_80020928(WINDOW_X + 20, FIRST_LINE + (sRow * LINE_HEIGHT) - 4);
    } else {
        Randomizer_DrawList();
    }
}

// Whether the card's prompt has "Edit", and the line its answer is on
static s32 Randomizer_CardHasEdit(s32 mode) {
    return (mode == CARD_ENTER) || (mode == CARD_USE);
}

static s32 Randomizer_CardLine(s32 answer) {
    s32 i;

    for (i = 0; i < CARD_LINES - 1; i++) {
        if (sCardLines[i] == answer) {
            break;
        }
    }
    return i;
}

// func_8420B40C: the card's prompt, with "Edit" when it has it
void Randomizer_CardPrompt(s16 x, s16 y, s16 mode, s16 answer) {
    s32 line;

    if (mode == 0) {
        return;
    }
    line = Randomizer_CardHasEdit(mode) ? Randomizer_CardLine(answer) : answer;

    func_80020928(x + 0x19C, y + (line * CARD_LINE_HEIGHT) + 0x3A);
    func_8001F3F4();
    func_8001EBE0(8, 0);
    func_8001F3B4(0x16);

    switch (mode) {
        case CARD_ENTER:
            func_8001F1E8(x + 0x19C, y + 0x20, func_84200130(0));
            break;

        case CARD_EXCHANGE:
            func_8001F1E8(x + 0x19C, y + 0x20, func_84200130(1));
            break;

        case CARD_USE:
            func_8001F1E8(x + 0x19C, y + 0xA, func_84200130(2));
            break;
    }

    func_8001EBE0(8, 0);
    func_8420B390(x + 0x1CE, y + 0x3A, 0, line, func_84200130(3));
    if (Randomizer_CardHasEdit(mode)) {
        func_8420B390(x + 0x1CE, y + 0x3A + CARD_LINE_HEIGHT, 1, line, "Edit");
        func_8420B390(x + 0x1CE, y + 0x3A + (2 * CARD_LINE_HEIGHT), 2, line, func_84200130(4));
    } else {
        func_8420B390(x + 0x1CE, y + 0x3A + CARD_LINE_HEIGHT, 1, line, func_84200130(4));
    }
    func_8001F444();
}

// func_8420C368: the card's input while it's open
void Randomizer_CardInput(unk_D_8423D3A8* card) {
    Controller* cont = &gControllers[card->unk_03];

    if (BTN_IS_PRESSED(cont, BTN_A)) {
        if ((card->unk_01 == 0) || (card->unk_02 == CARD_NO)) {
            func_80048B90(3);
        } else {
            func_80048B90(CARD_SOUND_YES);
        }
        if (card->unk_02 == CARD_EDIT) {
            unk_D_84211B50* team = D_842168A0.unk_13608;

            // The slot func_84207BD4 puts it in
            sEditSlot = team->unk_0010 + (team->unk_0012 * 3);
            card->unk_02 = CARD_YES;
        }
        card->unk_04 = 0;
        card->unk_00 = 3;
    } else if (BTN_IS_PRESSED(cont, BTN_B)) {
        func_80048B90(3);
        card->unk_04 = 0;
        card->unk_02 = CARD_NO;
        card->unk_00 = 3;
    } else if (BTN_IS_PRESSED(cont, BTN_DUP | BTN_DDOWN) && (card->unk_01 != 0)) {
        func_80048B90(1);
        if (Randomizer_CardHasEdit(card->unk_01)) {
            s32 line = Randomizer_CardLine(card->unk_02) + (BTN_IS_PRESSED(cont, BTN_DUP) ? -1 : 1);

            card->unk_02 = sCardLines[(line + CARD_LINES) % CARD_LINES];
        } else {
            card->unk_02 ^= 1;
        }
    }

    card->unk_06 = card->unk_0E.x1;
    card->unk_08 = card->unk_0E.y2;
    card->unk_0A = 0x228;
    card->unk_0C = 0xCC;
    card->unk_20 = func_8001B9D4(card->unk_28);
}

/*
 * Every frame (from the pick screen's drawing): after "Edit" in the rental card, once the
 * list has taken the answer and the team has settled with the Pokemon in it (it moves its
 * cursor to the next slot first), the teambuilder on it alone. Closing it goes back to
 * where the team was: picking, or its menu once it's complete. Not if the team broke the
 * cup's level-sum rule: the game says so first.
 */
void Randomizer_EditorAfterCard(void) {
    unk_D_84211B50* team = D_842168A0.unk_13608;
    s32 state;

    if ((sEditSlot < 0) || sOpen || (D_842168A0.unk_00001 == LIST_STATE_CARD)) {
        return;
    }
    state = team->unk_0001;
    if ((team->unk_0030[sEditSlot].unk_004.unk_00.unk_00 == 0) || (state == TEAM_STATE_LEVEL_SUM)) {
        sEditSlot = -1;
    } else if ((state == TEAM_STATE_PICK) || (state == TEAM_STATE_MENU_OK) || (state == TEAM_STATE_MENU_REGISTER)) {
        sTeam = team;
        sViewer = NULL;
        sReturnState = state;
        team->unk_0001 = RANDOMIZER_TEAM_STATE_EDIT;
        Randomizer_EditorStart(&team->unk_0030[sEditSlot], 1, team->unk_0002);
        sEditSlot = -1;
    }
}

#endif
