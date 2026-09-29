/*
 * The randomizer's battle UI fragment (randomizer_battleui, see randomizer_battle_ui.h).
 * Built only with RANDOMIZER=1; empty otherwise so the default build still matches.
 *
 * The game hides the moves (fight menu) and the party (Pokemon menu) until R is held,
 * so that a second player can't see them. Against the computer there's no one to hide
 * them from: they show as soon as the menu opens, with an "L Cancel" bar in place of the
 * "L Cancel / R Check" one, and the party is a single window with each Pokemon's HP,
 * moves and stats. Between two players, R still shows them, and the party box gets a
 * panel with each Pokemon's moves.
 *
 * The battle's setup loads this fragment after fragment62, so its references to
 * fragment62's functions and data are relocated to where fragment62 is.
 */
#include "randomizer_battle_ui.h"

#ifdef RANDOMIZER

#include "src/1CF30.h"
#include "src/20470.h"
#include "src/2E110.h"
#include "src/randomizer_state.h"

// The battle UI's coordinates
#define SCREEN_W 320
#define SCREEN_H 240
#define SCREEN_MARGIN 3

// The party boxes' height (func_8431524C, func_84315550)
#define PARTY_BOX_H 0x66

// One column per Pokemon, three to a row, in the party box's order
#define COLUMNS 3
#define COLUMN_W 72   // twelve characters
#define MOVE_INDENT 2 // leaves a gap after a twelve-letter move
#define LINE_H 9
#define BLOCK_H ((5 * LINE_H) + 3) // name and four moves
#define PADDING 4
#define GAP 6

#define NUM_MOVES 4

// The hint bar's texts (func_843133B4)
#define TEXT_CANCEL 0xA9

// The party window, right of the player's HP box and clear of the opponent's: a column
// for each Pokemon with its name, switch button (as in the game's party box), level,
// HP, moves and stats
#define PARTY_TOP_Y 4
#define PARTY_BOTTOM_Y 236 // where it ends for the player at the bottom
#define PARTY_HEADER_H 14
#define PARTY_LINES 11
#define PARTY_H (PARTY_HEADER_H + (PARTY_LINES * LINE_H) + 3)
#define PARTY_MAX 3 // the switch buttons are C buttons, three of them
#define BUTTON_W 0x10
#define BUTTON_H 0xE
#define STATUS_W 0x14
#define STATUS_H 9

// The switch buttons (func_84314F60): texture and palette for each Pokemon
static u8* sButtonTextures[PARTY_MAX][2] = {
    { D_3006A70, D_3006C30 },
    { D_3006290, D_3006450 },
    { D_3006530, D_30066F0 },
};

// The status icons (D_84385838), for func_800219A0's statuses and fainting
static u8* sStatusIcons[] = {
    D_3007A78, D_3007EB0, D_3007910, D_3008018, D_3007BE0, D_3007D48,
};

// Where the game puts the menus for the player at the top and at the bottom
// (func_843172A0, func_84317558)
#define MOVES_TOP_X 0x60
#define MOVES_TOP_Y 0xF
#define MOVES_BOTTOM_X 0x19
#define MOVES_BOTTOM_Y 0xA6
#define MOVES_H 0x38
#define CANCEL_W 0x40
#define CANCEL_H 0x10

// The fight menu's R-held box colours for each player (D_84385860, D_84385870)
static Color_RGB8 sPanelTop[] = {
    { 0x00, 0x00, 0x8B },
    { 0x00, 0x32, 0x00 },
    { 0x1E, 0x00, 0x00 },
    { 0x65, 0x49, 0x11 },
};
static Color_RGB8 sPanelBottom[] = {
    { 0x00, 0x00, 0xEE },
    { 0x00, 0x79, 0x00 },
    { 0x82, 0x00, 0x00 },
    { 0x97, 0x89, 0x13 },
};

// The rest of the battle UI is drawn after this, so the panel keeps clear of the other
// player's corner: the player whose box is at the top gets it at the bottom left, the
// one at the bottom at the top right
static void Randomizer_DrawMoves(unk_func_80026268_arg0* party, s32 count, s32 boxY, s32 player) {
    s32 columns = (count < COLUMNS) ? count : COLUMNS;
    s32 rows = (count + COLUMNS - 1) / COLUMNS;
    s32 w = (columns * COLUMN_W) + (2 * PADDING);
    s32 h = (rows * BLOCK_H) + (2 * PADDING);
    s32 x;
    s32 y;
    unk_func_80026268_arg0* mon;
    s32 i;
    s32 j;
    s32 cx;
    s32 cy;

    if (count <= 0) {
        return;
    }
    if (boxY < (SCREEN_H / 2)) {
        x = SCREEN_MARGIN;
        y = boxY + PARTY_BOX_H + GAP;
    } else {
        x = SCREEN_W - SCREEN_MARGIN - w;
        y = boxY - h - GAP;
    }
    player &= 3;

    func_84310334(x + 1, y + 1, w, h, &sPanelTop[player], &sPanelBottom[player]);
    func_84311428(x, y, w, h);

    func_8001F3F4();
    func_8001EBE0(1, 0);
    for (i = 0; i < count; i++) {
        mon = &party[i];
        cx = x + PADDING + ((i % COLUMNS) * COLUMN_W);
        cy = y + PADDING + ((i / COLUMNS) * BLOCK_H);

        func_8001F324(0xFF, 0xFF, 0x64, 0xFF);
        func_8001F1E8(cx, cy, "%s", mon->unk_30);

        func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
        for (j = 0; (j < NUM_MOVES) && (mon->unk_09[j] != 0); j++) {
            func_8001F1E8(cx + MOVE_INDENT, cy + ((j + 1) * LINE_H), "%s",
                          func_8002D7C0(NULL, 0, D_843900B8, mon->unk_09[j] - 1));
        }
    }
    func_8001F444();
}

// func_8431524C's box, for up to three Pokemon, then their moves
static s32 Randomizer_VsComputer(unk_D_84390010* arg0);
static void Randomizer_DrawParty(unk_D_84390010* arg0, unk_D_800AE540_0004* trainer, s32 top, s32 player, s32 cancel);

static void Randomizer_Party3(unk_D_84390010* arg0, unk_D_800AE540_0004* arg1, s16 arg2, s16 arg3, s32 arg4) {
    // Against the computer, the same window as without R
    if (Randomizer_VsComputer(arg0) && (arg1->unk_002 <= PARTY_MAX)) {
        Randomizer_DrawParty(arg0, arg1, arg3 < (SCREEN_H / 2), arg4, 1);
        return;
    }

    func_8431524C(arg0, arg1, arg2, arg3, arg4);

    // Two trainers on a side get two boxes side by side, which leaves no room
    if (arg0->unk_720->unk_01 != 2) {
        Randomizer_DrawMoves(arg1->unk_01C, arg1->unk_002, arg3, arg4);
    }
}

// func_84315550's box, for six Pokemon, then their moves
static void Randomizer_Party6(unk_D_84390010* arg0, s16 arg1, s16 arg2, s32 arg3) {
    func_84315550(arg0, arg1, arg2, arg3);

    Randomizer_DrawMoves(arg0->unk_724->unk_01C, arg0->unk_720->unk_08[arg0->unk_728.unk_16C]->unk_002, arg2, arg3);
}

// Against the computer, the menus show everything without R
static s32 Randomizer_VsComputer(unk_D_84390010* arg0) {
    s32 side;
    s32 i;

    // Two trainers on a side have boxes side by side, which the party window can't be
    if (arg0->unk_720->unk_01 != 1) {
        return 0;
    }
    for (side = 0; side < 2; side++) {
        for (i = 0; i < D_800AE540.unk_1194[side].unk_01; i++) {
            unk_D_800AE540_0004* trainer = D_800AE540.unk_1194[side].unk_08[i];

            if ((trainer != NULL) && (trainer->unk_000 & 2)) {
                return 1;
            }
        }
    }
    return 0;
}

// The L button and "Cancel", as func_843133B4 draws them
static void Randomizer_DrawCancelLabel(s32 x, s32 y) {
    gSPDisplayList(gDisplayListHead++, D_8006F5A0);
    func_8001D560(x + 7, y + 3, 0x10, 0xC, D_3008780, D_3008900, 0x10, 0x100000);
    gSPDisplayList(gDisplayListHead++, D_8006F630);

    func_8001F3F4();
    func_8001EBE0(1, 0);
    func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
    func_8001F1E8(x + 0x18, y + 3, func_8002D7C0(NULL, 0, D_843900B0, TEXT_CANCEL));
    func_8001F444();
}

// func_843133B4's bar with only "L Cancel"
static void Randomizer_DrawCancelBar(s32 x, s32 y, s32 player) {
    func_84310334(x, y, CANCEL_W, CANCEL_H, &sPanelTop[player & 3], &sPanelBottom[player & 3]);
    func_84311428(x, y, CANCEL_W, CANCEL_H);
    Randomizer_DrawCancelLabel(x, y);
    func_8430FC28(x - 2, y - 2, CANCEL_W + 6, CANCEL_H + 6, 0);
}

// One Pokemon's column of the party window
static void Randomizer_DrawPartyColumn(unk_func_80026268_arg0* mon, s32 index, s32 x, s32 y) {
    static const char* sStatNames[] = { "ATK", "DEF", "SPD", "SPC" };
    u16 stats[4];
    s32 status;
    s32 i;

    stats[0] = mon->unk_28;
    stats[1] = mon->unk_2A;
    stats[2] = mon->unk_2C;
    stats[3] = mon->unk_2E;
    status = (mon->unk_02 == 0) ? 1 : func_800219A0(mon->unk_05);

    // The button that switches to it, beside its level and HP
    gSPDisplayList(gDisplayListHead++, D_8006F5A0);
    func_8001D560(x, y + LINE_H + 1, BUTTON_W, BUTTON_H, sButtonTextures[index][0], sButtonTextures[index][1], 0x10,
                  0x100000);
    gSPDisplayList(gDisplayListHead++, D_8006F630);
    if (status != 0) {
        gSPDisplayList(gDisplayListHead++, D_8006F518);
        func_8001C6AC(x + BUTTON_W + 32, y + LINE_H, STATUS_W, STATUS_H, sStatusIcons[status - 1], STATUS_W, 0);
        gSPDisplayList(gDisplayListHead++, D_8006F630);
    }

    func_8001F3F4();
    func_8001EBE0(1, 0);
    func_8001F324(0xFF, 0xFF, 0x64, 0xFF);
    func_8001F1E8(x, y, "%s", mon->unk_30);

    func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
    func_8001F1E8(x + BUTTON_W + 2, y + LINE_H, "L%d", mon->unk_24);
    func_8001F1E8(x + BUTTON_W + 2, y + (2 * LINE_H), "%d/%d", mon->unk_02, mon->unk_26);

    for (i = 0; (i < NUM_MOVES) && (mon->unk_09[i] != 0); i++) {
        func_8001F1E8(x + MOVE_INDENT, y + ((3 + i) * LINE_H), "%s",
                      func_8002D7C0(NULL, 0, D_843900B8, mon->unk_09[i] - 1));
    }

    for (i = 0; i < 4; i++) {
        s32 sy = y + ((3 + NUM_MOVES + i) * LINE_H);

        func_8001F324(0xA0, 0xC8, 0xFF, 0xFF);
        func_8001F1E8(x + MOVE_INDENT, sy, "%s", sStatNames[i]);
        func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
        func_8001F1E8(x + MOVE_INDENT + func_8001F5B0(1, 0, "%s ", sStatNames[i]), sy, "%d", stats[i]);
    }
    func_8001F444();
}

// The party as one window, a column per Pokemon
static void Randomizer_DrawParty(unk_D_84390010* arg0, unk_D_800AE540_0004* trainer, s32 top, s32 player, s32 cancel) {
    s32 count = trainer->unk_002;
    s32 w = (count * COLUMN_W) + (2 * PADDING);
    s32 x;
    s32 y = top ? PARTY_TOP_Y : (PARTY_BOTTOM_Y - PARTY_H);
    s32 i;

    if (w < (COLUMN_W * 2)) {
        w = COLUMN_W * 2; // room for the header
    }
    x = SCREEN_W - SCREEN_MARGIN - w;
    player &= 3;
    func_84310334(x + 1, y + 1, w, PARTY_H, &sPanelTop[player], &sPanelBottom[player]);
    func_84311428(x, y, w, PARTY_H);

    func_8001F3F4();
    func_8001EBE0(1, 0);
    func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
    func_8001F1E8(x + PADDING + 2, y + 3, "%s", trainer->unk_008);
    func_8001F444();
    if (cancel) {
        Randomizer_DrawCancelLabel(x + w - CANCEL_W, y - 1);
    }

    for (i = 0; i < count; i++) {
        Randomizer_DrawPartyColumn(&trainer->unk_01C[i], i, x + PADDING + (i * COLUMN_W), y + PARTY_HEADER_H);
    }

    func_8430FC28(x - 2, y - 2, w + 6, PARTY_H + 4, 0);
}

// The trainer whose menus these are
static unk_D_800AE540_0004* Randomizer_MenuTrainer(unk_D_84390010* arg0) {
    return arg0->unk_720->unk_08[arg0->unk_728.unk_16C];
}

// In place of func_843133B4, the "L Cancel / R Check" bar of the fight and Pokemon menus
static void Randomizer_Hint(unk_D_84390010* arg0, s16 x, s16 y, s32 player) {
    s32 top = y < (SCREEN_H / 2);
    unk_D_800AE540_0004* trainer = Randomizer_MenuTrainer(arg0);

    if (!Randomizer_VsComputer(arg0)) {
        func_843133B4(arg0, x, y, player);
    } else if (arg0->unk_654.unk_10 == 1) {
        // The fight menu: the moves, as when R is held, with the cancel bar next to them
        if (top) {
            func_84313A74(arg0, MOVES_TOP_X, MOVES_TOP_Y, player);
            Randomizer_DrawCancelBar(MOVES_TOP_X, MOVES_TOP_Y + MOVES_H + 3, player);
        } else {
            func_84313A74(arg0, MOVES_BOTTOM_X, MOVES_BOTTOM_Y, player);
            Randomizer_DrawCancelBar(MOVES_BOTTOM_X, MOVES_BOTTOM_Y - CANCEL_H - 3, player);
        }
    } else if (trainer->unk_002 <= PARTY_MAX) {
        Randomizer_DrawParty(arg0, trainer, top, player, 1);
    } else {
        func_843133B4(arg0, x, y, player);
    }
}

// In place of func_843135B8, the "R Check" bar of a switch that can't be cancelled
static void Randomizer_HintForced(unk_D_84390010* arg0, s16 x, s16 y, s32 player) {
    unk_D_800AE540_0004* trainer = Randomizer_MenuTrainer(arg0);

    if (Randomizer_VsComputer(arg0) && (trainer->unk_002 <= PARTY_MAX)) {
        Randomizer_DrawParty(arg0, trainer, y < (SCREEN_H / 2), player, 0);
    } else {
        func_843135B8(arg0, x, y, player);
    }
}

// Run by Randomizer_BattleUiLoad once the fragment is loaded, at every battle's setup
void Randomizer_BattleUiEntry(void) {
    gRandomizerState.battlePartyHook3 = Randomizer_Party3;
    gRandomizerState.battlePartyHook6 = Randomizer_Party6;
    gRandomizerState.battleHintHook = Randomizer_Hint;
    gRandomizerState.battleHintForcedHook = Randomizer_HintForced;
}

#endif
