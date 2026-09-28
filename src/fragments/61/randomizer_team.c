/*
 * Team moves view for the in-game randomizer: the moves of every Pokemon in the entry
 * box, drawn like the summary screen draws them (type colour and icon). It shows by
 * itself with the OK / Reselect menu once the team is complete, which otherwise only
 * shows moves one Pokemon at a time through "Reselect some Pokemon", and while C-Down
 * is held anywhere on the pick screen. It sits above that menu, over the entry box.
 * Built only with RANDOMIZER=1; empty otherwise so the default build still matches.
 */
#include "randomizer.h"

#ifdef RANDOMIZER

#include "src/1CF30.h"
#include "src/20470.h"
#include "src/controller.h"

// fragment61_2E2C20.c: per type, the summary's colours and type icon
typedef struct unk_D_842115F0 {
    /* 0x00 */ Color_RGB8 unk_00[4];
    /* 0x0C */ u8* unk_0C;
} unk_D_842115F0; // size = 0x10
extern unk_D_842115F0 D_842115F0[];

extern unk_D_842168A0 D_842168A0;
// Set to have the pick screen redraw everything (func_84202718)
extern s16 D_84210D40;

// Above the OK / Reselect menu, which starts at y = 305
#define PANEL_X 40
#define PANEL_Y 24
#define PANEL_W 560
#define PANEL_H 272
#define PANEL_COLOR 0x2121 // the blue of the pick screen's own menus

// Three columns and two rows, in the order of the entry box above
#define COLUMNS 3
#define COLUMN_X(i) (PANEL_X + 20 + ((i) % COLUMNS) * 180)
#define ROW_Y(i) (PANEL_Y + 40 + ((i) / COLUMNS) * 112)
#define COLUMN_W 168
#define NAME_HEIGHT 28
#define MOVE_HEIGHT 21
#define ICON_SIZE 0x14
#define MOVE_TEXT_X (ICON_SIZE + 6)

#define NUM_MOVES 4
#define TEAM_SIZE 6

// The team's states for the OK / Reselect menu (func_8420776C): 7, or 14 when
// team->unk_0000 is set (func_84207190)
#define TEAM_STATE_OK_MENU 7
#define TEAM_STATE_OK_MENU_2 14

static s32 sShown;

// The move's type, as an index into D_842115F0
#define MOVE_TYPE(move) (D_80072338[(move) - 1].unk_01)

void Randomizer_TeamReset(void) {
    sShown = 0;
}

static unk_func_80026268_arg0* Randomizer_TeamMon(unk_D_84211B50* team, s32 i) {
    unk_func_80026268_arg0* mon = &team->unk_0030[i].unk_004;

    return (mon->unk_00.unk_00 != 0) ? mon : NULL;
}

// Called at the end of the pick screen's drawing, every frame
void Randomizer_TeamDraw(void) {
    unk_D_84211B50* team = D_842168A0.unk_13608;
    Controller* cont = &gControllers[D_842168A0.unk_00003];
    unk_func_80026268_arg0* mon;
    Color_RGB8* color;
    s32 i;
    s32 j;
    s32 x;
    s32 y;

    if ((team == NULL) || Randomizer_PanelIsOpen() ||
        !(BTN_IS_DOWN(cont, BTN_CDOWN) || (team->unk_0001 == TEAM_STATE_OK_MENU) ||
          (team->unk_0001 == TEAM_STATE_OK_MENU_2))) {
        if (sShown) {
            // Draw the screen under it again
            sShown = 0;
            D_84210D40 = 2;
        }
        return;
    }
    sShown = 1;

    func_80020460(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, PANEL_COLOR);

    func_8001F3F4();
    func_8001EBE0(0x10, 0);
    func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
    func_8001F1E8(PANEL_X + 24, PANEL_Y + 12, "Your team's moves");

    func_8001EBE0(8, 0);
    for (i = 0; i < TEAM_SIZE; i++) {
        mon = Randomizer_TeamMon(team, i);
        if (mon == NULL) {
            continue;
        }
        x = COLUMN_X(i);
        y = ROW_Y(i);

        func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
        func_8001F1E8(x, y, "%s", mon->unk_30);
        // As the entry box writes the level
        func_8001F1E8(x + COLUMN_W - func_8001F5B0(8, 0, "%s %d", func_84200160(0x15), mon->unk_24), y, "%s %d",
                      func_84200160(0x15), mon->unk_24);

        for (j = 0; (j < NUM_MOVES) && (mon->unk_09[j] != 0); j++) {
            color = &D_842115F0[MOVE_TYPE(mon->unk_09[j])].unk_00[2];
            func_8001F324(color->r, color->g, color->b, 0xFF);
            func_8001F1E8(x + MOVE_TEXT_X, y + NAME_HEIGHT + (j * MOVE_HEIGHT), "%s", func_842000C0(mon->unk_09[j]));
        }
    }
    func_8001F444();

    // The type icons, as the summary draws them next to the moves
    gSPDisplayList(gDisplayListHead++, D_8006F518);
    for (i = 0; i < TEAM_SIZE; i++) {
        mon = Randomizer_TeamMon(team, i);
        if (mon == NULL) {
            continue;
        }
        for (j = 0; (j < NUM_MOVES) && (mon->unk_09[j] != 0); j++) {
            func_8001C6AC(COLUMN_X(i), ROW_Y(i) + NAME_HEIGHT + (j * MOVE_HEIGHT) - 2, ICON_SIZE, ICON_SIZE,
                          D_842115F0[MOVE_TYPE(mon->unk_09[j])].unk_0C, ICON_SIZE, 0);
        }
    }
    gSPDisplayList(gDisplayListHead++, D_8006F630);
}

#endif
