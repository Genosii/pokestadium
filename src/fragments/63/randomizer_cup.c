/*
 * The randomizer's cup screen fragment (randomizer_cup, see randomizer_cup.h): what the
 * opponents' "Mode" option adds between the battles of a cup or the Gym Leader Castle.
 * Built only with RANDOMIZER=1; empty otherwise so the default build still matches.
 *
 * Factory and Rogue: after each win (but the last), a panel shows the three Pokemon the
 * trainer battled with and the player's six. The player can take one of the trainer's,
 * which keeps its level, DVs, stat exp and moves, in place of one of their own, or keep
 * their team. It goes into the team the battle-select screen reads (and a "save and
 * quit" saves), healed and with the player's trainer name and ID. One swap per win: if
 * the player kept their team, the next-battle menu that follows gets a "Swap a Pokemon"
 * line that brings the panel back.
 *
 * Rogue: a loss ends the run. A panel says so, then the screen quits as its own menu's
 * "Quit" would, so there's no continue or retry. Badges and records aren't touched.
 *
 * The screen's own frames (func_84B01AA0) keep running while a panel is up, and draw it
 * through the draw hook at the end of each one.
 */
#include "randomizer_cup.h"

#ifdef RANDOMIZER

#include "src/1CF30.h"
#include "src/20470.h"
#include "src/22630.h"
#include "src/232C0.h"
#include "src/29BA0.h"
#include "src/2E110.h"
#include "src/49790.h"
#include "src/controller.h"
#include "src/hal_libc.h"

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
#define NICKNAME_LENGTH 10

// func_8002D5AC's archive of move names
#define MOVE_NAMES_ARCHIVE 0x25

// The screen's quit flag (the loss menu's "Quit")
#define CUP_QUIT 1

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

extern unk_D_84B17550 D_84B17550[];
extern unk_D_84B26640 D_84B26640;

// Each menu's height without the swap line
static s16 sMenuHeights[2];
static s32 sMenuSaved;

enum {
    STEP_OFF,
    STEP_THEIRS,  // picking one of the trainer's Pokemon
    STEP_YOURS,   // picking one of the player's to give up for it
    STEP_RUN_OVER // Rogue's loss
};

static s32 sStep;
static s32 sSwapped; // this win's swap is done
static s32 sCursor;
static s32 sTaken; // index into sTheirs
static s32 sTheirs[TEAM_SIZE];
static s32 sNumTheirs;
static char** sMoveNames;

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

// A box behind a Pokemon, for the cursor or the one picked
static void Randomizer_DrawBox(s32 i, s32 y, u16 color) {
    func_80020460(COLUMN_X(i) - 5, y - 4, COLUMN_W + 10, BLOCK_H + 5, color);
}

static void Randomizer_DrawSwap(void) {
    unk_D_800AE540_0004* trainer = Randomizer_Trainer();
    unk_func_80026268_arg0* yours;
    s32 count;
    s32 i;

    yours = Randomizer_Yours(&count);

    func_80020460(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, PANEL_COLOR);
    if (sStep == STEP_THEIRS) {
        Randomizer_DrawBox(sCursor, THEIRS_Y, HIGHLIGHT_COLOR);
    } else {
        Randomizer_DrawBox(sTaken, THEIRS_Y, TAKEN_COLOR);
        Randomizer_DrawBox(sCursor, YOURS_Y + ((sCursor / COLUMNS) * BLOCK_H), HIGHLIGHT_COLOR);
    }

    func_8001F3F4();
    func_8001EBE0(2, 0);
    func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
    if (sStep == STEP_THEIRS) {
        func_8001F1E8(PANEL_X + 8, TITLE_Y, "Take one of %s's?", trainer->unk_008);
    } else {
        func_8001F1E8(PANEL_X + 8, TITLE_Y, "Swap %s for?", trainer->unk_01C[sTheirs[sTaken]].unk_30);
    }

    func_8001EBE0(1, 0);
    for (i = 0; i < sNumTheirs; i++) {
        Randomizer_DrawMon(&trainer->unk_01C[sTheirs[i]], COLUMN_X(i), THEIRS_Y);
    }

    func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
    func_8001F1E8(PANEL_X + 8, YOURS_LABEL_Y, "Your team");
    for (i = 0; i < count; i++) {
        Randomizer_DrawMon(&yours[i], COLUMN_X(i), YOURS_Y + ((i / COLUMNS) * BLOCK_H));
    }

    func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
    if (sStep == STEP_THEIRS) {
        func_8001F1E8(PANEL_X + 8, FOOTER_Y, "A: take it   B: keep your team");
    } else {
        func_8001F1E8(PANEL_X + 8, FOOTER_Y, "A: swap   B: back");
    }
    func_8001F444();
}

static void Randomizer_DrawRunOver(void) {
    func_80020460(PANEL_X + 40, 72, PANEL_W - 80, 88, PANEL_COLOR);

    func_8001F3F4();
    func_8001EBE0(2, 0);
    func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
    func_8001F1E8(PANEL_X + 56, 84, "Your run is over");
    func_8001EBE0(1, 0);
    func_8001F1E8(PANEL_X + 56, 108, "Rogue mode: no retries.");
    func_8001F1E8(PANEL_X + 56, 136, "A: back to the menu");
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
                  "Swap a Pok\xE9mon");
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
            Randomizer_DrawSwap();
            break;

        case STEP_RUN_OVER:
            Randomizer_DrawRunOver();
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

static void Randomizer_SwapInput(Controller* cont) {
    unk_func_80026268_arg0* yours;
    s32 count;
    s32 max;

    yours = Randomizer_Yours(&count);
    max = (sStep == STEP_THEIRS) ? sNumTheirs : count;

    if (BTN_IS_PRESSED(cont, BTN_DLEFT)) {
        sCursor = (sCursor + max - 1) % max;
        func_80048B90(1);
    } else if (BTN_IS_PRESSED(cont, BTN_DRIGHT)) {
        sCursor = (sCursor + 1) % max;
        func_80048B90(1);
    } else if ((sStep == STEP_YOURS) && BTN_IS_PRESSED(cont, BTN_DUP | BTN_DDOWN)) {
        if (count > COLUMNS) {
            sCursor = (sCursor + COLUMNS) % (COLUMNS * 2);
            if (sCursor >= count) {
                sCursor = count - 1;
            }
            func_80048B90(1);
        }
    } else if (BTN_IS_PRESSED(cont, BTN_A)) {
        if (sStep == STEP_THEIRS) {
            sTaken = sCursor;
            sCursor = 0;
            sStep = STEP_YOURS;
        } else {
            Randomizer_Swap(&Randomizer_Trainer()->unk_01C[sTheirs[sTaken]], &yours[sCursor]);
            sSwapped = 1;
            sStep = STEP_OFF;
        }
        func_80048B90(2);
    } else if (BTN_IS_PRESSED(cont, BTN_B)) {
        if (sStep == STEP_THEIRS) {
            sStep = STEP_OFF;
        } else {
            sCursor = sTaken;
            sStep = STEP_THEIRS;
        }
        func_80048B90(3);
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

    // Freed with the rest of this part of the screen
    sMoveNames = func_8002D5AC(MOVE_NAMES_ARCHIVE);
    sCursor = 0;
    sStep = STEP_THEIRS;
    func_80048B90(4);

    while (sStep != STEP_OFF) {
        func_84B01AA0();
        Randomizer_SwapInput(gPlayer1Controller);
    }
    sMoveNames = NULL;

    // Kept their team: the menu offers the swap again
    Randomizer_SetMenuLine(!sSwapped);
}

// After a loss, before the menu for what to do next
static s32 Randomizer_RunOver(void) {
    if (Randomizer_Mode() != RANDOMIZER_MODE_ROGUE) {
        return 0;
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

static RandomizerCupHooks sHooks = {
    Randomizer_CupDraw,
    Randomizer_AfterWin,
    Randomizer_RunOver,
    Randomizer_PikachuCheck,
};

// Run by the cup screen once it has loaded this fragment (func_84B03194)
void Randomizer_CupEntry(void) {
    sStep = STEP_OFF;
    sSwapped = 0;
    sMenuSaved = 0;
    sMoveNames = NULL;
    // Even before the pick screen has set the rest up (a cup resumed after power-on):
    // it's only this, and the hooks check the rest is valid
    gRandomizerState.cupHooks = &sHooks;
}

#endif
