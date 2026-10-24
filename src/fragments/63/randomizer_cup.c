/*
 * The randomizer's cup screen fragment (randomizer_cup, see randomizer_cup.h): what the
 * opponents' "Mode" option adds between the battles of a cup or the Gym Leader Castle.
 * Built only with RANDOMIZER=1; empty otherwise so the default build still matches.
 *
 * Factory: after each win (but the last), a panel shows the three Pokemon the
 * trainer battled with and the player's six. The player can take one of the trainer's,
 * which keeps its level, DVs, stat exp and moves, in place of one of their own, or keep
 * their team. It goes into the team the battle-select screen reads (and a "save and
 * quit" saves), healed and with the player's trainer name and ID. One swap per win: if
 * the player kept their team, the next-battle menu that follows gets a "Swap a Pokemon"
 * line that brings the panel back.
 *
 * Rogue: after each win (but the last), a reward instead: a new Pokemon, one of three made
 * with the player's options (none the player or the trainer has), in place of one of their
 * own; or a move changed, one of a Pokemon's for one of three others it can learn (any
 * move with the Chaos moveset). Built as the rental card's "Randomize" builds them
 * (randomizer_core): the "Stadium" options give Strong moves and top stats. Skipped, the
 * menu's "Pick a reward" line brings it back. A loss ends the run: a panel says so, then
 * the screen quits as its own menu's "Quit" would, so there's no continue or retry.
 * Badges and records aren't touched.
 *
 * The screen's own frames (func_84B01AA0) keep running while a panel is up, and draw it
 * through the draw hook at the end of each one.
 *
 * Factory and Rogue were the options' "Playstyle"; they're the Battle Frontier's now, and its
 * runs (gRandomizerState.frontier) don't have them yet (the next part). A run comes here with
 * the mode of a cup the US version left out (randomizer_stadium.c), which becomes the Prime
 * Cup's (level 100) as the screen starts. Its trainers aren't the cup's: each battle's comes
 * from the trainer files by the run's seed and the battle's number (Randomizer_Foe), from the
 * Prime Cup's balls, Round 1's then Round 2's, as the run goes on; Rogue has a boss every
 * fifth battle (Gym Leaders, the Elite Four, the rival last) with a harder AI, and ends after
 * the number of battles its rules have. The cup's eight rounds go round and round (the lap
 * counted), and after each win the trainers up to the next boss show under "Keep battling?"
 * (Factory's: the next one, and the streak). A loss ends the run, and quitting ends it
 * without saving it to continue.
 */
#include "randomizer_cup.h"

#ifdef RANDOMIZER

#include "src/1CF30.h"
#include "src/20470.h"
#include "src/22630.h"
#include "src/232C0.h"
#include "src/26820.h"
#include "src/29BA0.h"
#include "src/2C1C0.h"
#include "src/2E110.h"
#include "src/3FB0.h"
#include "src/49790.h"
#include "src/controller.h"
#include "src/hal_libc.h"
#include "src/memory.h"

// The screen's coordinates
#define PANEL_X 8
#define PANEL_Y 8
#define PANEL_W 304
#define PANEL_H 224
#define PANEL_COLOR 0x2121     // the blue of the game's menus
#define HIGHLIGHT_COLOR 0x32B1 // the Pokemon under the cursor
#define TAKEN_COLOR 0x2411     // the trainer's Pokemon picked to take

#define TITLE_Y (PANEL_Y + 6)
#define THEIRS_Y (PANEL_Y + 26)
#define YOURS_LABEL_Y (THEIRS_Y + BLOCK_H + 5)
#define YOURS_Y (YOURS_LABEL_Y + 13)
#define FOOTER_Y (PANEL_Y + PANEL_H - 16)

// A Pokemon: name and level, then its moves; three to a row
#define COLUMNS 3
#define COLUMN_X(i) (PANEL_X + 12 + ((i) % COLUMNS) * 96)
#define COLUMN_W 88
#define LINE_H 9
#define BLOCK_H ((5 * LINE_H) + 2)
#define MOVE_INDENT 2

#define NUM_MOVES 4
#define TEAM_SIZE 6
#define NUM_OFFERS 3
#define OFFER_TRIES 8 // teams made to find three new Pokemon
#define REWARD_POKEMON 0
#define REWARD_MOVE 1
#define REWARD_Y 72
#define REWARD_LINE_H 16
#define MAX_PP_UPS (3 << 6)
#define NICKNAME_LENGTH 10

// func_8002D5AC's archive of move names
#define MOVE_NAMES_ARCHIVE 0x25

// The screen's quit flag (the loss menu's "Quit")
#define CUP_QUIT 1
#define CUP_TITLE 0x40 // with CUP_QUIT: to the title screen (func_80029BC0)

#define MAX_ROUND 8
// The modes of the two cups the US version left out, which a Battle Frontier run starts with
#define ROGUE_SLOT_MODE 1
#define FACTORY_SLOT_MODE 2

// The next-battle menus (fragment63_3A1C30.c): 1 in cups, 2 in the Castle, as
// func_84B022A0 opens them, and their line height and layout (func_84B0DE04)
#define MENU_CUP 1
#define MENU_CASTLE 2
#define MENU_LINE_H 0xE
#define MENU_ITEMS_X 0x22
#define MENU_ITEMS_Y 0x18
#define MENU_COUNT_X 0x4A
// func_84B0DE04 shows the cup's continues only at this height; with the swap line
// they move down and are drawn here instead
#define MENU_COUNT_HEIGHT 0x48
#define MENU_COUNT_Y (0x36 + MENU_LINE_H)
#define MENU_OPEN 4 // unk_02 once the window is fully open

// A Battle Frontier run's trainers (docs/game-data.md, "Trainer files"): the trainer archive's
// files of 8 trainers, 0x230 bytes each after the file's count, the name first and the
// trainer ID at 0x34 (its portrait the high byte, its AI the low one)
#define TRAINER_ARCHIVE ((u8*)0x898000)
#define TRAINER_SIZE 0x230
#define TRAINER_ID_OFFSET 0x34
#define TRAINER_NAME_LENGTH 11
#define TRAINERS_PER_FILE 8
#define FRONTIER_LEVEL 100
#define BOSS_EVERY 5
// The trainers battled, from the easiest: the Prime Cup's balls, Round 1's then Round 2's
// (0x1F files on). Rogue goes up through them over the run, Factory every 7 wins.
#define TIERS 8
#define FACTORY_TIER_WINS 7
static const u8 sTierFiles[TIERS] = { 2, 3, 4, 5, 2 + 0x1F, 3 + 0x1F, 4 + 0x1F, 5 + 0x1F };
// Rogue's bosses: the Gym Leader Castle's leaders (the fourth of files 12 to 19, Brock to
// Giovanni), the Elite Four (file 20) and the rival (file 21, the last, the Champion's)
#define GYM_FILE 12
#define GYMS 8
#define LEADER_INDEX 3
#define ELITE_FOUR_FILE 20
#define ELITE_FOUR 4
#define RIVAL_FILE 21
#define RIVAL_INDEX 6
// The bosses' AI (the trainer ID's low byte picks one of the battle's 0x48-byte profiles at
// ROM 0x7820E0): the Gym Leaders the Round 2 Castle's (files 43 to 50), harder than Round
// 1's; the Elite Four and the rival "saikyou 6" ("strongest 6"), the final Rival's
static const u8 sLeaderAi[GYMS] = { 0x41, 0x41, 0x44, 0x43, 0x42, 0x43, 0x44, 0x44 };
#define AI_STRONGEST 0x08

typedef struct RandomizerFoe {
    /* 0x0 */ u8 file;
    /* 0x1 */ u8 index;
    /* 0x2 */ u8 ai; // 0 for the trainer's own
    /* 0x3 */ u8 boss;
} RandomizerFoe; // size = 0x4

// The trainers up to the next boss, under "Keep battling?" (the screen's coordinates); faces
// copied out of the trainers' portraits (64x64, the face 56x56 from 4,4), two strips each
#define LINEUP_SIZE BOSS_EVERY
#define LINEUP_Y 148
#define LINEUP_H 88
#define FACE_W 56
#define FACE_H 56
#define PORTRAIT_W 64
#define FACE_BORDER 4
#define FACE_STRIP 28
#define FACE_X(i) (PANEL_X + 12 + (i) * 58)
#define FACE_Y (LINEUP_Y + 16)
#define BOSS_COLOR 0xFF, 0x90, 0x30

extern unk_D_84B17550 D_84B17550[];
extern unk_D_84B1A598 D_84B1A598; // unk_B40C: the trainers' portraits
extern unk_D_84B26640 D_84B26640;

// Each menu's height without the swap line
static s16 sMenuHeights[2];
static s32 sMenuSaved;

enum {
    STEP_OFF,
    STEP_THEIRS,   // picking one of the trainer's Pokemon (or Rogue's new ones)
    STEP_YOURS,    // picking one of the player's to give up for it
    STEP_RUN_OVER, // Rogue's loss
    STEP_REWARD,   // Rogue: a new Pokemon or a move
    STEP_WHOSE,    // whose move to change,
    STEP_WHICH,    // which of its moves,
    STEP_LEARN,    // and which new one
    STEP_LINEUP    // a Battle Frontier run's trainers to come, under "Keep battling?"
};

static s32 sStep;
static s32 sSwapped; // this win's swap is done
static s32 sCursor;
static s32 sTaken; // index into sTheirs
static s32 sTheirs[TEAM_SIZE];
static s32 sNumTheirs;
static char** sMoveNames;
static s32 sRogue; // the Pokemon on offer are sOffers, not the trainer's
static unk_func_80026268_arg0 sOffers[NUM_OFFERS];
static s32 sMon;  // the player's Pokemon whose move changes,
static s32 sMove; // which one,
static u8 sLearn[NUM_OFFERS]; // and the ones it can learn instead
static s32 sNumLearn;
static s32 sUnpacked;
static s32 sBeaten;  // trainers a Battle Frontier run has beaten, for its end
static s32 sRunKind; // and which it was
static s32 sCleared; // Rogue's: all its battles won
static s32 sWins;    // the run's wins, as the line-up shows
static s32 sLineupFirst; // the first battle in the line-up
static s32 sLineupCount;
static char sLineupNames[LINEUP_SIZE][TRAINER_NAME_LENGTH + 1];
static u8 sLineupBoss[LINEUP_SIZE];
static u8 sLineupFace[LINEUP_SIZE]; // its face was loaded
static u16 sFaces[LINEUP_SIZE][FACE_W * FACE_H];
static u32 sRandom;

static unk_D_800AE540_0004* Randomizer_Player(void) {
    return D_800AE540.unk_1194[0].unk_08[0];
}

static unk_D_800AE540_0004* Randomizer_Trainer(void) {
    return D_800AE540.unk_1194[1].unk_08[0];
}

static s32 Randomizer_Mode(void) {
    return RANDOMIZER_STATE_VALID() ? gRandomizerState.mode : RANDOMIZER_MODE_NORMAL;
}

// The player's team as the battle-select screen reads it (and a save keeps it)
static unk_func_80026268_arg0* Randomizer_Yours(s32* count) {
    unk_D_800AE540_0874* team = Randomizer_Player()->unk_214;

    *count = team->unk_002;
    return team->unk_028;
}

static void Randomizer_DrawMon(unk_func_80026268_arg0* mon, s32 x, s32 y) {
    s32 i;

    func_8001F324(0xFF, 0xFF, 0x64, 0xFF);
    func_8001F1E8(x, y, "%s", mon->unk_30);
    func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
    func_8001F1E8(x + COLUMN_W - func_8001F5B0(1, 0, "L%d", mon->unk_24), y, "L%d", mon->unk_24);

    if (sMoveNames == NULL) {
        return;
    }
    for (i = 0; (i < NUM_MOVES) && (mon->unk_09[i] != 0); i++) {
        func_8001F1E8(x + MOVE_INDENT, y + ((i + 1) * LINE_H), "%s",
                      func_8002D7C0(NULL, 0, sMoveNames, mon->unk_09[i] - 1));
    }
}

// One of the Pokemon on offer: the trainer's, or Rogue's new ones
static unk_func_80026268_arg0* Randomizer_Offer(s32 i) {
    return sRogue ? &sOffers[i] : &Randomizer_Trainer()->unk_01C[sTheirs[i]];
}

// A box behind a Pokemon, for the cursor or the one picked
static void Randomizer_DrawBox(s32 i, s32 y, u16 color) {
    func_80020460(COLUMN_X(i) - 5, y - 4, COLUMN_W + 10, BLOCK_H + 5, color);
}

// The moves to learn, in the top row: name, power and accuracy
static void Randomizer_DrawLearn(void) {
    s32 i;

    for (i = 0; i < sNumLearn; i++) {
        RandomizerMove* move = &gRandomizerMoves[sLearn[i]];

        func_8001F324(0xFF, 0xFF, 0x64, 0xFF);
        func_8001F1E8(COLUMN_X(i), THEIRS_Y, "%s", func_8002D7C0(NULL, 0, sMoveNames, sLearn[i] - 1));
        func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
        if (move->power > 1) {
            func_8001F1E8(COLUMN_X(i) + MOVE_INDENT, THEIRS_Y + LINE_H, "Power %d", move->power);
        } else {
            func_8001F1E8(COLUMN_X(i) + MOVE_INDENT, THEIRS_Y + LINE_H, "Power -");
        }
        func_8001F1E8(COLUMN_X(i) + MOVE_INDENT, THEIRS_Y + (2 * LINE_H), "Accuracy %d", move->accuracy);
    }
}

static void Randomizer_DrawSwap(void) {
    unk_D_800AE540_0004* trainer = Randomizer_Trainer();
    unk_func_80026268_arg0* yours;
    s32 count;
    s32 i;

    yours = Randomizer_Yours(&count);

    func_80020460(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, PANEL_COLOR);
    switch (sStep) {
        case STEP_THEIRS:
        case STEP_LEARN:
            Randomizer_DrawBox(sCursor, THEIRS_Y, HIGHLIGHT_COLOR);
            break;
        case STEP_YOURS:
            Randomizer_DrawBox(sTaken, THEIRS_Y, TAKEN_COLOR);
            /* fallthrough */
        case STEP_WHOSE:
            Randomizer_DrawBox(sCursor, YOURS_Y + ((sCursor / COLUMNS) * BLOCK_H), HIGHLIGHT_COLOR);
            break;
        case STEP_WHICH:
            Randomizer_DrawBox(sMon, YOURS_Y + ((sMon / COLUMNS) * BLOCK_H), TAKEN_COLOR);
            func_80020460(COLUMN_X(sMon) - 2, YOURS_Y + ((sMon / COLUMNS) * BLOCK_H) + ((sCursor + 1) * LINE_H) - 1,
                          COLUMN_W + 4, LINE_H, HIGHLIGHT_COLOR);
            break;
    }

    func_8001F3F4();
    func_8001EBE0(2, 0);
    func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
    switch (sStep) {
        case STEP_THEIRS:
            if (sRogue) {
                func_8001F1E8(PANEL_X + 8, TITLE_Y, "Take a new Pok\xE9mon?");
            } else {
                func_8001F1E8(PANEL_X + 8, TITLE_Y, "Take one of %s's?", trainer->unk_008);
            }
            break;
        case STEP_YOURS:
            func_8001F1E8(PANEL_X + 8, TITLE_Y, "Swap %s for?", Randomizer_Offer(sTaken)->unk_30);
            break;
        case STEP_WHOSE:
            func_8001F1E8(PANEL_X + 8, TITLE_Y, "Change whose move?");
            break;
        case STEP_WHICH:
            func_8001F1E8(PANEL_X + 8, TITLE_Y, "Change which move?");
            break;
        case STEP_LEARN:
            func_8001F1E8(PANEL_X + 8, TITLE_Y, "Learn which move?");
            break;
    }

    func_8001EBE0(1, 0);
    if (sStep == STEP_LEARN) {
        Randomizer_DrawLearn();
    } else if ((sStep == STEP_THEIRS) || (sStep == STEP_YOURS)) {
        for (i = 0; i < sNumTheirs; i++) {
            Randomizer_DrawMon(Randomizer_Offer(i), COLUMN_X(i), THEIRS_Y);
        }
    }

    func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
    func_8001F1E8(PANEL_X + 8, YOURS_LABEL_Y, "Your team");
    for (i = 0; i < count; i++) {
        Randomizer_DrawMon(&yours[i], COLUMN_X(i), YOURS_Y + ((i / COLUMNS) * BLOCK_H));
    }

    func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
    switch (sStep) {
        case STEP_THEIRS:
            func_8001F1E8(PANEL_X + 8, FOOTER_Y, sRogue ? "A: take it   B: back" : "A: take it   B: keep your team");
            break;
        case STEP_YOURS:
            func_8001F1E8(PANEL_X + 8, FOOTER_Y, "A: swap   B: back");
            break;
        case STEP_LEARN:
            func_8001F1E8(PANEL_X + 8, FOOTER_Y, "A: learn it   B: back");
            break;
        default:
            func_8001F1E8(PANEL_X + 8, FOOTER_Y, "A: choose   B: back");
            break;
    }
    func_8001F444();
}

// Rogue's choice of reward
static void Randomizer_DrawReward(void) {
    func_80020460(PANEL_X + 40, REWARD_Y, PANEL_W - 80, 88, PANEL_COLOR);
    func_80020460(PANEL_X + 50, REWARD_Y + 28 + (sCursor * REWARD_LINE_H) - 3, PANEL_W - 100, REWARD_LINE_H - 2,
                  HIGHLIGHT_COLOR);

    func_8001F3F4();
    func_8001EBE0(2, 0);
    func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
    func_8001F1E8(PANEL_X + 56, REWARD_Y + 8, "Pick a reward");
    func_8001EBE0(1, 0);
    func_8001F1E8(PANEL_X + 60, REWARD_Y + 28, "A new Pok\xE9mon");
    func_8001F1E8(PANEL_X + 60, REWARD_Y + 28 + REWARD_LINE_H, "Change a move");
    func_8001F1E8(PANEL_X + 56, REWARD_Y + 68, "A: choose   B: not now");
    func_8001F444();
}

static void Randomizer_DrawRunOver(void) {
    s32 frontier = sRunKind;

    func_80020460(PANEL_X + 40, 72, PANEL_W - 80, 88, PANEL_COLOR);

    func_8001F3F4();
    func_8001EBE0(2, 0);
    func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
    func_8001F1E8(PANEL_X + 56, 84, sCleared ? "Rogue cleared!" : "Your run is over");
    func_8001EBE0(1, 0);
    if (sCleared) {
        func_8001F1E8(PANEL_X + 56, 104, "You beat all %d trainers.", sBeaten);
    } else if (frontier == FRONTIER_NONE) {
        func_8001F1E8(PANEL_X + 56, 108, "Rogue mode: no retries.");
    } else {
        func_8001F1E8(PANEL_X + 56, 104, "%s: a loss ends the run.",
                      (frontier == FRONTIER_ROGUE) ? "Rogue" : "Factory");
        func_8001F1E8(PANEL_X + 56, 118, "Trainers beaten: %d", sBeaten);
    }
    func_8001F1E8(PANEL_X + 56, 136, "A: back to the menu");
    func_8001F444();
}

// The trainers up to the next boss (Factory's: the next one), the beaten ones grey (their
// faces made so as they're copied)
static void Randomizer_DrawLineup(void) {
    s32 next = sWins + 1;
    s32 battle;
    s32 i;

    func_80020460(PANEL_X, LINEUP_Y, PANEL_W, LINEUP_H, PANEL_COLOR);
    for (i = 0; i < sLineupCount; i++) {
        if ((sLineupFirst + i) == next) {
            func_80020460(FACE_X(i) - 3, FACE_Y - 3, FACE_W + 6, FACE_H + 6, HIGHLIGHT_COLOR);
        }
    }

    gSPDisplayList(gDisplayListHead++, D_8006F518);
    for (i = 0; i < sLineupCount; i++) {
        if (sLineupFace[i]) {
            func_8001C6AC(FACE_X(i), FACE_Y, FACE_W, FACE_STRIP, (u8*)sFaces[i], FACE_W, 0);
            func_8001C6AC(FACE_X(i), FACE_Y + FACE_STRIP, FACE_W, FACE_STRIP, (u8*)(sFaces[i] + FACE_STRIP * FACE_W),
                          FACE_W, 0);
        }
    }
    gSPDisplayList(gDisplayListHead++, D_8006F630);

    func_8001F3F4();
    func_8001EBE0(1, 0);
    func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
    if (sRunKind == FRONTIER_ROGUE) {
        func_8001F1E8(PANEL_X + 12, LINEUP_Y + 6, "Rogue: battle %d of %d", next, FRONTIER_BATTLES(&gRandomizerState));
    } else {
        func_8001F1E8(PANEL_X + 12, LINEUP_Y + 6, "Factory: %d win%s in a row. Next:", sWins, (sWins == 1) ? "" : "s");
    }
    for (i = 0; i < sLineupCount; i++) {
        battle = sLineupFirst + i;
        if (sLineupBoss[i]) {
            func_8001F324(BOSS_COLOR, 0xFF);
        } else if (battle < next) {
            func_8001F324(0x9C, 0x9C, 0x9C, 0xFF);
        } else {
            func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
        }
        func_8001F1E8(FACE_X(i), FACE_Y + FACE_H + 1, "%s", sLineupNames[i]);
    }
    func_8001F444();
}

// The "Swap a Pokemon" line, and the cup's continues below it, on the open menu
static void Randomizer_DrawMenuLine(void) {
    unk_D_84B2665C* menu = &D_84B26640.unk_1C;
    unk_D_84B17550* def;

    if ((menu->unk_00 == 0) || ((menu->unk_01 != MENU_CUP) && (menu->unk_01 != MENU_CASTLE)) ||
        (menu->unk_02 < MENU_OPEN)) {
        return;
    }
    def = &D_84B17550[menu->unk_01 - 1];
    if (def->unk_08 <= RANDOMIZER_CUP_SWAP_ITEM) {
        return;
    }

    func_8001F3F4();
    func_8001EBE0(2, 0);
    if (menu->unk_04 == RANDOMIZER_CUP_SWAP_ITEM) {
        func_8001F324(0xFF, 0xFF, 0, 0xFF);
    } else {
        func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
    }
    func_8001F1E8(def->unk_00 + MENU_ITEMS_X, def->unk_02 + MENU_ITEMS_Y + (RANDOMIZER_CUP_SWAP_ITEM * MENU_LINE_H),
                  (Randomizer_Mode() == RANDOMIZER_MODE_ROGUE) ? "Pick a reward" : "Swap a Pok\xE9mon");
    if (menu->unk_01 == MENU_CUP) {
        func_8001EBE0(1, 0);
        func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
        func_8001F1E8(def->unk_00 + MENU_COUNT_X, def->unk_02 + MENU_COUNT_Y, "%s%d", func_84B0037C(0x2C),
                      D_800AE540.unk_11F3);
    }
    func_8001F444();
}

// Gives the next-battle menus the swap line, or takes it away
static void Randomizer_SetMenuLine(s32 on) {
    s32 i;

    for (i = 0; i < 2; i++) {
        unk_D_84B17550* def = &D_84B17550[i];

        if (!sMenuSaved) {
            sMenuHeights[i] = def->unk_06;
        }
        def->unk_08 = on ? (RANDOMIZER_CUP_SWAP_ITEM + 1) : RANDOMIZER_CUP_SWAP_ITEM;
        // Taller by a line, and never MENU_COUNT_HEIGHT, which would draw the
        // continues where the line is
        def->unk_06 = sMenuHeights[i] + (on ? (MENU_LINE_H + 2) : 0);
    }
    sMenuSaved = 1;
}

// At the end of every frame's drawing (func_84B014DC)
static void Randomizer_CupDraw(void) {
    Randomizer_DrawMenuLine();

    switch (sStep) {
        case STEP_THEIRS:
        case STEP_YOURS:
        case STEP_WHOSE:
        case STEP_WHICH:
        case STEP_LEARN:
            Randomizer_DrawSwap();
            break;

        case STEP_REWARD:
            Randomizer_DrawReward();
            break;

        case STEP_RUN_OVER:
            Randomizer_DrawRunOver();
            break;

        case STEP_LINEUP:
            Randomizer_DrawLineup();
            break;
    }
}

// The trainer's Pokemon becomes the player's, in slot
static void Randomizer_Swap(unk_func_80026268_arg0* theirs, unk_func_80026268_arg0* slot) {
    unk_func_80026268_arg0 mon = *theirs;
    char name[0x20];
    s32 i;

    // The player's trainer name and ID, and the species' name, as rentals have
    mon.unk_0E = slot->unk_0E;
    _bcopy(slot->unk_3B, mon.unk_3B, sizeof(mon.unk_3B));
    // Where it came from, as the team's other Pokemon
    mon.unk_52 = slot->unk_52;
    mon.unk_53 = slot->unk_53;
    bzero(mon.unk_30, sizeof(mon.unk_30));
    func_80021CA4(name, mon.unk_00.unk_00);
    for (i = 0; (i < NICKNAME_LENGTH) && (name[i] != '\0'); i++) {
        mon.unk_30[i] = name[i];
    }
    // Stats worked out again, with full HP and PP and no status after its battle
    func_80022734(&mon);
    func_800228B0(&mon);
    *slot = mon;
}

// The same LCG as the team generator's, for the moves on offer
static s32 Randomizer_CupBelow(s32 n) {
    sRandom = (sRandom * 0x19660D) + 0x3C6EF35F;
    return ((u64)sRandom * (u32)n) >> 32;
}

static void Randomizer_Unpack(void) {
    if (!sUnpacked) {
        // The generator's tables, in randomizer_core, loaded with this fragment
        Randomizer_UnpackData();
        sUnpacked = 1;
    }
}

// Whether the player, the trainer or another of the offers has the species
static s32 Randomizer_Seen(s32 species) {
    unk_D_800AE540_0004* trainer = Randomizer_Trainer();
    unk_func_80026268_arg0* yours;
    s32 count;
    s32 i;

    yours = Randomizer_Yours(&count);
    for (i = 0; i < count; i++) {
        if (yours[i].unk_00.unk_00 == species) {
            return 1;
        }
    }
    for (i = 0; i < trainer->unk_002; i++) {
        if (trainer->unk_01C[i].unk_00.unk_00 == species) {
            return 1;
        }
    }
    for (i = 0; i < sNumTheirs; i++) {
        if (sOffers[i].unk_00.unk_00 == species) {
            return 1;
        }
    }
    return 0;
}

/*
 * Rogue's three new Pokemon: teams made with the player's options and the cup's rules (no
 * rentals: the "Stadium" options give Strong moves and top stats, as the rental card's
 * "Randomize" does), the first three not seen yet taken
 */
static void Randomizer_MakeOffers(void) {
    unk_func_80026268_arg0* yours;
    RandomizerRules rules;
    RandomizerMon team[RANDOMIZER_TEAM_SIZE];
    s32 count;
    s32 tries;
    s32 i;

    Randomizer_Unpack();
    yours = Randomizer_Yours(&count);
    Randomizer_GetRules(D_800AE540.unk_0001, yours[0].unk_24, &rules);
    rules.rental = NULL;
    Randomizer_Seed(osGetCount());

    sNumTheirs = 0;
    for (tries = 0; (tries < OFFER_TRIES) && (sNumTheirs < NUM_OFFERS); tries++) {
        if (!Randomizer_GenerateTeam(&gRandomizerState.settings, &rules, team)) {
            continue;
        }
        for (i = 0; (i < RANDOMIZER_TEAM_SIZE) && (sNumTheirs < NUM_OFFERS); i++) {
            if (!Randomizer_Seen(team[i].species)) {
                Randomizer_BuildPokemon(&sOffers[sNumTheirs], &team[i], &yours[0]);
                sNumTheirs++;
            }
        }
    }
}

/*
 * Three moves the Pokemon can learn and doesn't know: from its learnset (Gen 1's own moves
 * only with "No tradebacks"), or any with the Chaos moveset
 */
static void Randomizer_MakeLearn(unk_func_80026268_arg0* mon) {
    RandomizerSettings* settings = &gRandomizerState.settings;
    RandomizerSpecies* species;
    u8 moves[RANDOMIZER_NUM_MOVES];
    s32 count = 0;
    s32 known;
    s32 move;
    s32 i;
    s32 j;

    Randomizer_Unpack();
    species = &gRandomizerSpecies[mon->unk_00.unk_00];
    for (i = 0; i < ((settings->moveset == RANDOMIZER_MOVESET_CHAOS) ? RANDOMIZER_NUM_CHAOS_MOVES
                                                                     : species->learnsetCount);
         i++) {
        if (settings->moveset == RANDOMIZER_MOVESET_CHAOS) {
            move = gRandomizerChaosMoves[i];
        } else {
            RandomizerLearnsetMove* learn = &gRandomizerLearnsets[species->learnsetStart + i];

            if (settings->noTradeback && !learn->gen1) {
                continue;
            }
            move = learn->move;
        }
        known = (move == 0);
        for (j = 0; j < NUM_MOVES; j++) {
            known |= (mon->unk_09[j] == move);
        }
        for (j = 0; j < count; j++) {
            known |= (moves[j] == move);
        }
        if (!known && (count < RANDOMIZER_NUM_MOVES)) {
            moves[count++] = move;
        }
    }

    // Three of them, each picked from those not picked yet
    sRandom = osGetCount();
    for (sNumLearn = 0; (sNumLearn < NUM_OFFERS) && (count > 0); sNumLearn++) {
        i = Randomizer_CupBelow(count);
        sLearn[sNumLearn] = moves[i];
        moves[i] = moves[--count];
    }
}

// Stats worked out again, with full HP and PP and no status after its battle
static void Randomizer_Heal(unk_func_80026268_arg0* mon) {
    func_80022734(mon);
    func_800228B0(mon);
}

// How many lines the cursor goes over in a step
static s32 Randomizer_StepLines(void) {
    unk_func_80026268_arg0* yours;
    s32 count;
    s32 i;

    yours = Randomizer_Yours(&count);
    switch (sStep) {
        case STEP_REWARD:
            return 2;
        case STEP_THEIRS:
            return sNumTheirs;
        case STEP_LEARN:
            return sNumLearn;
        case STEP_WHICH:
            // Its moves, and the first empty slot if it has one
            for (i = 0; (i < NUM_MOVES) && (yours[sMon].unk_09[i] != 0); i++) {}
            return MIN(i + 1, NUM_MOVES);
        default:
            return count;
    }
}

static void Randomizer_PanelA(void) {
    unk_func_80026268_arg0* yours;
    unk_func_80026268_arg0* mon;
    s32 count;

    yours = Randomizer_Yours(&count);
    switch (sStep) {
        case STEP_REWARD:
            if (sCursor == REWARD_POKEMON) {
                if (sNumTheirs == 0) {
                    Randomizer_MakeOffers();
                }
                if (sNumTheirs == 0) {
                    return;
                }
                sStep = STEP_THEIRS;
            } else {
                sStep = STEP_WHOSE;
            }
            sCursor = 0;
            break;

        case STEP_THEIRS:
            sTaken = sCursor;
            sCursor = 0;
            sStep = STEP_YOURS;
            break;

        case STEP_YOURS:
            Randomizer_Swap(Randomizer_Offer(sTaken), &yours[sCursor]);
            sSwapped = 1;
            sStep = STEP_OFF;
            break;

        case STEP_WHOSE:
            if (sMon != sCursor) {
                sNumLearn = 0;
            }
            sMon = sCursor;
            sCursor = 0;
            sStep = STEP_WHICH;
            break;

        case STEP_WHICH:
            // The moves on offer stay the same for the Pokemon, whichever is replaced
            if (sNumLearn == 0) {
                Randomizer_MakeLearn(&yours[sMon]);
            }
            if (sNumLearn == 0) {
                return;
            }
            sMove = sCursor;
            sCursor = 0;
            sStep = STEP_LEARN;
            break;

        case STEP_LEARN:
            mon = &yours[sMon];
            mon->unk_09[sMove] = sLearn[sCursor];
            mon->unk_20[sMove] = MAX_PP_UPS;
            Randomizer_Heal(mon);
            sSwapped = 1;
            sStep = STEP_OFF;
            break;
    }
    func_80048B90(2);
}

static void Randomizer_PanelB(void) {
    switch (sStep) {
        case STEP_THEIRS:
            if (sRogue) {
                sCursor = REWARD_POKEMON;
                sStep = STEP_REWARD;
            } else {
                sStep = STEP_OFF;
            }
            break;
        case STEP_YOURS:
            sCursor = sTaken;
            sStep = STEP_THEIRS;
            break;
        case STEP_WHOSE:
            sCursor = REWARD_MOVE;
            sStep = STEP_REWARD;
            break;
        case STEP_WHICH:
            sCursor = sMon;
            sStep = STEP_WHOSE;
            break;
        case STEP_LEARN:
            sCursor = sMove;
            sStep = STEP_WHICH;
            break;
        default:
            sStep = STEP_OFF;
            break;
    }
    func_80048B90(3);
}

static void Randomizer_PanelInput(Controller* cont) {
    s32 max = Randomizer_StepLines();
    s32 grid = (sStep == STEP_YOURS) || (sStep == STEP_WHOSE);

    if (BTN_IS_PRESSED(cont, BTN_DLEFT) || (!grid && BTN_IS_PRESSED(cont, BTN_DUP))) {
        sCursor = (sCursor + max - 1) % max;
        func_80048B90(1);
    } else if (BTN_IS_PRESSED(cont, BTN_DRIGHT) || (!grid && BTN_IS_PRESSED(cont, BTN_DDOWN))) {
        sCursor = (sCursor + 1) % max;
        func_80048B90(1);
    } else if (grid && BTN_IS_PRESSED(cont, BTN_DUP | BTN_DDOWN)) {
        if (max > COLUMNS) {
            sCursor = (sCursor + COLUMNS) % (COLUMNS * 2);
            if (sCursor >= max) {
                sCursor = max - 1;
            }
            func_80048B90(1);
        }
    } else if (BTN_IS_PRESSED(cont, BTN_A)) {
        Randomizer_PanelA();
    } else if (BTN_IS_PRESSED(cont, BTN_B)) {
        Randomizer_PanelB();
    }
}

// After a win, before the menu for the next battle, and again when that menu's swap
// line is picked (func_84B022A0)
static void Randomizer_AfterWin(void) {
    unk_D_800AE540_0004* trainer = Randomizer_Trainer();
    s32 i;

    if ((Randomizer_Mode() == RANDOMIZER_MODE_NORMAL) || (trainer == NULL) || !(trainer->unk_000 & 2) || sSwapped) {
        Randomizer_SetMenuLine(0);
        return;
    }

    sRogue = (Randomizer_Mode() == RANDOMIZER_MODE_ROGUE);
    if (sRogue) {
        // A reward: the Pokemon and the moves on offer are made the first time they're
        // looked at, and stay the same for this win
        sStep = STEP_REWARD;
    } else {
        // The three it battled with: having lost, they're the ones that fainted. The rest
        // weren't sent out.
        sNumTheirs = 0;
        for (i = 0; i < trainer->unk_002; i++) {
            if (trainer->unk_01C[i].unk_02 == 0) {
                sTheirs[sNumTheirs++] = i;
            }
        }
        if (sNumTheirs == 0) {
            Randomizer_SetMenuLine(0);
            return;
        }
        sStep = STEP_THEIRS;
    }

    // Freed with the rest of this part of the screen
    sMoveNames = func_8002D5AC(MOVE_NAMES_ARCHIVE);
    sCursor = 0;
    func_80048B90(4);

    while (sStep != STEP_OFF) {
        func_84B01AA0();
        Randomizer_PanelInput(gPlayer1Controller);
    }
    sMoveNames = NULL;

    // Kept their team: the menu offers the swap again
    Randomizer_SetMenuLine(!sSwapped);
}

// After a loss, before the menu for what to do next
static s32 Randomizer_RunOver(void) {
    s32 frontier = RANDOMIZER_FRONTIER();

    if ((Randomizer_Mode() != RANDOMIZER_MODE_ROGUE) && (frontier == FRONTIER_NONE)) {
        return 0;
    }
    sRunKind = frontier;
    if (frontier != FRONTIER_NONE) {
        sBeaten = (FRONTIER_LAP(gRandomizerState.frontier) * MAX_ROUND) + D_800AE540.unk_0003 - 1;
        // Over: the player's own options back as the next screen with them starts
        gRandomizerState.frontier = FRONTIER_RESTORE;
    }

    sStep = STEP_RUN_OVER;
    do {
        func_84B01AA0();
    } while (!BTN_IS_PRESSED(gPlayer1Controller, BTN_A));
    sStep = STEP_OFF;
    func_80048B90(2);

    D_800AE540.unk_11F6 |= CUP_QUIT;
    return 1;
}

// func_84B01994, unchanged: after the Prime Cup's Master Ball in Round 2, a Pikachu on the
// team that knows Surf (and came from a Game Boy game) unlocks Surfing Pikachu
static void Randomizer_PikachuCheck(void) {
    s32 i;
    s32 j;
    unk_D_800AE540_0004* var_a1;
    unk_D_800AE540_0874* temp_v1;

    if (!(D_800AE540.unk_11F5 & 2) && (D_800AE540.unk_11F2 == 1) && (D_800AE540.unk_0000 == 6) &&
        (D_800AE540.unk_0002 == 3)) {
        var_a1 = D_800AE540.unk_1194[0].unk_08[0];
        temp_v1 = var_a1->unk_214;

        for (i = 0; i < temp_v1->unk_002; i++) {
            s32 tmp = (temp_v1->unk_028[i].unk_52 & 0x70) >> 4;
            s32 tmp2 = temp_v1->unk_028[i].unk_52 & 0xF;

            if ((tmp >= 4) || (tmp2 >= 13)) {
                return;
            }
        }

        for (i = 0; i < var_a1->unk_002; i++) {
            unk_func_80026268_arg0* ptr = &var_a1->unk_01C[i];

            if (ptr->unk_00.unk_00 == 0x19) {
                for (j = 0; j < 4; j++) {
                    if (ptr->unk_09[j] == 0) {
                        break;
                    } else if (ptr->unk_09[j] == 0x39) {
                        return;
                    }
                }
                D_800AE540.unk_11F6 |= 0x1000;
                break;
            }
        }
    }
}

// A 32-bit finalizer, so nearby battles give unrelated trainers
static u32 Randomizer_Mix(u32 x) {
    x ^= x >> 16;
    x *= 0x85EBCA6B;
    x ^= x >> 13;
    x *= 0xC2B2AE35;
    x ^= x >> 16;
    return x;
}

// A Battle Frontier run's battle number: the one coming before a battle, the one just won
// after it (the round is counted up after "Keep battling?")
static s32 Randomizer_Battle(void) {
    return (FRONTIER_LAP(gRandomizerState.frontier) * MAX_ROUND) + D_800AE540.unk_0003;
}

static s32 Randomizer_IsBoss(s32 kind, s32 battle) {
    return (kind == FRONTIER_ROGUE) && ((battle % BOSS_EVERY) == 0);
}

/*
 * A run's trainer for a battle, the same every time for the run's seed: a boss every fifth
 * of Rogue's (its last the rival; before it up to four of the Elite Four, spread from
 * Lorelei to Lance, Lance always; the earlier ones Gym Leaders spread from Brock to
 * Giovanni, Giovanni always, as many as the Elite Four or one more), or a trainer of the
 * tier the battle has reached, not the one battled just before.
 */
static void Randomizer_Foe(s32 kind, s32 battle, RandomizerFoe* foe) {
    s32 battles = FRONTIER_BATTLES(&gRandomizerState);
    s32 bosses = battles / BOSS_EVERY;
    s32 boss = battle / BOSS_EVERY;
    s32 eliteFour;
    s32 leaders;
    s32 tier;
    s32 last;

    foe->ai = 0;
    foe->boss = 0;
    if (Randomizer_IsBoss(kind, battle)) {
        foe->boss = 1;
        foe->ai = AI_STRONGEST;
        eliteFour = (bosses - 1) / 2;
        if (eliteFour > ELITE_FOUR) {
            eliteFour = ELITE_FOUR;
        }
        leaders = bosses - 1 - eliteFour;
        if (boss >= bosses) {
            foe->file = RIVAL_FILE;
            foe->index = RIVAL_INDEX;
        } else if (boss <= leaders) {
            foe->file = GYM_FILE + ((boss * GYMS) / leaders) - 1;
            foe->index = LEADER_INDEX;
            foe->ai = sLeaderAi[foe->file - GYM_FILE];
        } else {
            foe->file = ELITE_FOUR_FILE;
            foe->index = (((boss - leaders) * ELITE_FOUR) / eliteFour) - 1;
        }
        return;
    }

    if (kind == FRONTIER_ROGUE) {
        tier = ((battle - 1) * TIERS) / battles;
    } else {
        tier = (battle - 1) / FACTORY_TIER_WINS;
    }
    if (tier >= TIERS) {
        tier = TIERS - 1;
    }
    foe->file = sTierFiles[tier];
    foe->index = Randomizer_Mix(gRandomizerState.opponentSeed ^ (battle * 0x9E3779B9)) % TRAINERS_PER_FILE;
    if ((battle > 1) && !Randomizer_IsBoss(kind, battle - 1)) {
        last = Randomizer_Mix(gRandomizerState.opponentSeed ^ ((battle - 1) * 0x9E3779B9)) % TRAINERS_PER_FILE;
        if (last == foe->index) {
            foe->index = (foe->index + 1) % TRAINERS_PER_FILE;
        }
    }
}

// A trainer in the trainer archive (loaded with func_800044F4)
static u8* Randomizer_FoeEntry(BinArchive* archive, RandomizerFoe* foe) {
    return (u8*)func_8000484C(archive, foe->file) + sizeof(u32) + (foe->index * TRAINER_SIZE);
}

/*
 * func_8002C128, as each battle comes; during a run, its trainer loaded in the cup's in the
 * same way (func_8002BFC4, with the Round 2 flag cleared so the file is the one asked for),
 * its Pokemon made level 100 (the Castle's are 50 to 55; func_8002BEC8, as the Castle's
 * rival is raised to the player's level) and, for a boss, its AI.
 */
static void Randomizer_LoadTrainer(void) {
    s32 kind = RANDOMIZER_FRONTIER();
    unk_D_800AE540_0004* trainer;
    RandomizerFoe foe;
    u8 round2;

    func_8002C128();
    if (kind == FRONTIER_NONE) {
        return;
    }
    Randomizer_Foe(kind, Randomizer_Battle(), &foe);

    main_pool_push_state('RNDT');
    D_800AF738 = func_800044F4(TRAINER_ARCHIVE, NULL, 1, 0);
    round2 = D_800AE540.unk_11F2;
    D_800AE540.unk_11F2 = 0;
    func_8002BFC4(1, foe.file, foe.index, 0);
    D_800AE540.unk_11F2 = round2;
    func_8002BEC8(FRONTIER_LEVEL);
    if (foe.ai != 0) {
        trainer = Randomizer_Trainer();
        trainer->unk_018 = (trainer->unk_018 & ~0xFF) | foe.ai;
    }
    main_pool_pop_state('RNDT');
}

/*
 * The line-up shown under "Keep battling?": Rogue's five battles up to the next boss,
 * Factory's next one. The names and faces are copied out, as the archives they're in are
 * freed straight after (the portraits' archive loads a file again each time it's read).
 */
static void Randomizer_LineupLoad(s32 kind) {
    BinArchive* archive;
    RandomizerFoe foe;
    u8* entry;
    u16* portrait;
    s32 next = sWins + 1;
    s32 i;
    s32 row;

    sLineupFirst = (kind == FRONTIER_ROGUE) ? (((next - 1) / BOSS_EVERY) * BOSS_EVERY) + 1 : next;
    sLineupCount = (kind == FRONTIER_ROGUE) ? LINEUP_SIZE : 1;

    main_pool_push_state('LINE');
    archive = func_800044F4(TRAINER_ARCHIVE, NULL, 1, 0);
    for (i = 0; i < sLineupCount; i++) {
        Randomizer_Foe(kind, sLineupFirst + i, &foe);
        entry = Randomizer_FoeEntry(archive, &foe);
        bcopy(entry, sLineupNames[i], TRAINER_NAME_LENGTH);
        sLineupNames[i][TRAINER_NAME_LENGTH] = '\0';
        sLineupBoss[i] = foe.boss;

        portrait = func_8000484C(D_84B1A598.unk_B40C, entry[TRAINER_ID_OFFSET]);
        sLineupFace[i] = (portrait != NULL);
        for (row = 0; (row < FACE_H) && (portrait != NULL); row++) {
            bcopy(portrait + ((row + FACE_BORDER) * PORTRAIT_W) + FACE_BORDER, sFaces[i] + (row * FACE_W),
                  FACE_W * sizeof(u16));
        }
        // Beaten: grey and darker (RGBA 5551, the alpha bit kept)
        for (row = 0; (row < (FACE_W * FACE_H)) && (portrait != NULL) && ((sLineupFirst + i) < next); row++) {
            u16 pixel = sFaces[i][row];
            s32 grey = ((((pixel >> 11) & 0x1F) * 77) + (((pixel >> 6) & 0x1F) * 150) + (((pixel >> 1) & 0x1F) * 29)) >> 9;

            sFaces[i][row] = (grey << 11) | (grey << 6) | (grey << 1) | (pixel & 1);
        }
    }
    main_pool_pop_state('LINE');
    // Out of the CPU's cache, where the RDP doesn't look
    osWritebackDCache(sFaces, sizeof(sFaces));
}

/*
 * func_84B022A0, the "Keep battling?" menu after a win: 0 to battle on, 1 to quit (saving the
 * cup to continue). Factory and Rogue first swap one of the team for one the trainer battled
 * with, and again when the menu's "Swap a Pokemon" line is picked; a Battle Frontier run
 * isn't saved to continue: quitting ends it.
 */
static s32 Randomizer_KeepBattling(void) {
    s32 kind = RANDOMIZER_FRONTIER();
    s32 var_s2;
    s32 var_v0_2;

    if (kind != FRONTIER_NONE) {
        sRunKind = kind;
        sWins = Randomizer_Battle();
        // Rogue's last battle won: the run's over, as "Keep battling?" answered no would end
        // it (Randomizer_Quit), after a panel that says so
        if ((kind == FRONTIER_ROGUE) && (sWins >= FRONTIER_BATTLES(&gRandomizerState))) {
            sBeaten = sWins;
            sCleared = 1;
            sStep = STEP_RUN_OVER;
            do {
                func_84B01AA0();
            } while (!BTN_IS_PRESSED(gPlayer1Controller, BTN_A));
            sStep = STEP_OFF;
            func_80048B90(2);
            return 1;
        }
        Randomizer_LineupLoad(kind);
    }

    do {
        Randomizer_AfterWin();
        if (kind != FRONTIER_NONE) {
            sStep = STEP_LINEUP;
        }

        if (D_800AE540.unk_0000 == 7) {
            func_84B0F950(&D_84B26640.unk_1C, 2);
        } else {
            func_84B0F950(&D_84B26640.unk_1C, 1);
        }

        var_s2 = func_84B0FADC(&D_84B26640.unk_1C);
        while (var_s2 == -1) {
            func_84B01AA0();
            var_s2 = func_84B0FADC(&D_84B26640.unk_1C);
        }
    } while (var_s2 == RANDOMIZER_CUP_SWAP_ITEM);
    if (sStep == STEP_LINEUP) {
        sStep = STEP_OFF;
    }

    if ((var_s2 == 1) && (RANDOMIZER_FRONTIER() == FRONTIER_NONE)) {
        if (func_8002847C() != 0) {
            func_84B0F950(&D_84B26640.unk_1C, 0xA);

            var_v0_2 = func_84B0FADC(&D_84B26640.unk_1C);
            while (var_v0_2 == -1) {
                func_84B01AA0();
                var_v0_2 = func_84B0FADC(&D_84B26640.unk_1C);
            }

            if (var_v0_2 == 1) {
                var_s2 = 0;
            }
        }
        if (var_s2 == 1) {
            D_800AE540.unk_0003 = D_800AE540.unk_0003 + 1;
            func_8002C538();
            func_800284B4(2);
        }
    }
    return var_s2;
}

// "Keep battling?" answered no: the run saved to continue and the title screen, as the game
// does; a Battle Frontier run, which isn't saved, ends and goes back to the menu
static u16 Randomizer_Quit(void) {
    if (RANDOMIZER_FRONTIER() == FRONTIER_NONE) {
        return CUP_QUIT | CUP_TITLE;
    }
    gRandomizerState.frontier = FRONTIER_RESTORE;
    return CUP_QUIT;
}

// After a win, the round counted up: a Battle Frontier run's goes back to 1 after the
// eighth trainer (func_8002BFC4 keeps which trainer each round had in 8 bytes, by round),
// the lap counted. The trainers' teams take the lap into their seeds (fragment64's
// Randomizer_TrainerSeed), so they don't repeat.
static void Randomizer_NextRound(void) {
    RandomizerState* state = &gRandomizerState;
    s32 lap;

    if ((RANDOMIZER_FRONTIER() == FRONTIER_NONE) || (D_800AE540.unk_0003 <= MAX_ROUND)) {
        return;
    }
    D_800AE540.unk_0003 = 1;

    lap = FRONTIER_LAP(state->frontier);
    if (lap < (FRONTIER_LAP_MASK >> FRONTIER_LAP_SHIFT)) {
        lap++;
    }
    state->frontier = (state->frontier & ~FRONTIER_LAP_MASK) | (lap << FRONTIER_LAP_SHIFT);
}

static RandomizerCupHooks sHooks = {
    Randomizer_CupDraw,      Randomizer_AfterWin, Randomizer_RunOver,   Randomizer_PikachuCheck,
    Randomizer_KeepBattling, Randomizer_Quit,     Randomizer_NextRound, Randomizer_LoadTrainer,
};

// Run by the cup screen once it has loaded this fragment (func_84B03194)
void Randomizer_CupEntry(void) {
    sStep = STEP_OFF;
    sSwapped = 0;
    sMenuSaved = 0;
    sMoveNames = NULL;
    sRogue = 0;
    sNumTheirs = 0;
    sNumLearn = 0;
    sMon = -1;
    sUnpacked = 0;
    sBeaten = 0;
    sRunKind = FRONTIER_NONE;
    sCleared = 0;
    sWins = 0;
    sLineupCount = 0;
    // Even before the pick screen has set the rest up (a cup resumed after power-on):
    // it's only this, and the hooks check the rest is valid
    gRandomizerState.cupHooks = &sHooks;

    // A Battle Frontier run, its first time here: the mode of the cup left out it came with
    // (which skipped the ball select and showed Rogue's or Factory's picture) becomes the
    // Prime Cup's, whose rules it has (for the cups the mode is the rule set); its trainers
    // are its own (Randomizer_LoadTrainer)
    if ((RANDOMIZER_FRONTIER() != FRONTIER_NONE) &&
        ((D_800AE540.unk_0000 == ROGUE_SLOT_MODE) || (D_800AE540.unk_0000 == FACTORY_SLOT_MODE))) {
        D_800AE540.unk_0000 = D_800AE540.unk_0001;
    }
}

#endif
