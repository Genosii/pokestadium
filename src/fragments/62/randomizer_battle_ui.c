/*
 * The randomizer's battle UI fragment (randomizer_battleui, see randomizer_battle_ui.h):
 * while R is held in the Pokemon menu, the game shows the party box, and this adds a
 * panel next to it with each Pokemon's moves. Built only with RANDOMIZER=1; empty
 * otherwise so the default build still matches.
 *
 * The battle's setup loads this fragment after fragment62, so its references to
 * fragment62's functions and data are relocated to where fragment62 is.
 */
#include "randomizer_battle_ui.h"

#ifdef RANDOMIZER

#include "src/1CF30.h"
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
static void Randomizer_Party3(unk_D_84390010* arg0, unk_D_800AE540_0004* arg1, s16 arg2, s16 arg3, s32 arg4) {
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

// Run by Randomizer_BattleUiLoad once the fragment is loaded, at every battle's setup
void Randomizer_BattleUiEntry(void) {
    gRandomizerState.battlePartyHook3 = Randomizer_Party3;
    gRandomizerState.battlePartyHook6 = Randomizer_Party6;
}

#endif
