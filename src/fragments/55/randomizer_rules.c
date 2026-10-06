/*
 * The Rules screen's randomizer button (randomizer_rules, see randomizer_rules.h): Z opens
 * the same options window as C-Up on the Pokemon pick screen (and C-Up does here too),
 * each option with what it does under it, and the window's bottom
 * bar shows the button next to the game's own "A More detail". The game's own rules stay
 * as they are. Built only with RANDOMIZER=1; empty otherwise so the default build still
 * matches.
 */
#include "randomizer_rules.h"

#ifdef RANDOMIZER

#include "src/1CF30.h"
#include "src/20470.h"
#include "src/2E110.h"
#include "src/49790.h"
#include "src/6A40.h"
#include "src/controller.h"
#include "src/fragments/61/randomizer_menu.h"
#include "src/fragments/randomizer_z_icon.h"

#define WINDOW_W 0x210
#define BOTTOM_BAR_Y 0x134
#define FONT 0x10
#define ICON_VISIBLE_W 26 // the Z icon's right quarter is empty, like the game's L and R icons
#define ICON_GAP 4        // from the icon to its text, as the A button's in the same bar

static void Randomizer_RulesInput(void);
static void Randomizer_RulesDrawList(s16 arg0, s16 arg1);
static void Randomizer_RulesRedraw(void);
static void Randomizer_RulesLeave(void);

static RandomizerRulesHooks sHooks = {
    Randomizer_RulesInput,
    Randomizer_RulesDrawList,
    Randomizer_RulesLeave,
};

RandomizerRulesHooks* Randomizer_RulesEntry(void) {
    // Fragments aren't cleared when they're loaded
    Randomizer_PanelReset(Randomizer_RulesRedraw);
    return &sHooks;
}

// As the screen ends: the settings saved, if the options window changed them (which takes
// a few seconds, randomizer_pick.c)
static void Randomizer_RulesLeave(void) {
    Randomizer_SaveSettings();
}

// The screen draws its background for a couple of frames only, when something over it
// goes away (func_83001A9C)
static void Randomizer_RulesRedraw(void) {
    D_83003C90 = 2;
}

// func_8300059C, with the randomizer's buttons
static void Randomizer_RulesInput(void) {
    s16 tmp = D_83003CA6;

    if (Randomizer_PanelInput(gPlayer1Controller)) {
        return;
    }
    if (BTN_IS_PRESSED(gPlayer1Controller, BTN_Z)) {
        Randomizer_PanelOpen(RANDOMIZER_TAB_MODE);
        func_80048B90(4);
        return;
    }

    if (BTN_IS_PRESSED(gPlayer1Controller, BTN_DUP)) {
        D_83003CA6--;
        if (D_83003CA6 < 0) {
            D_83003CA6 = D_83003CA4 - 1;
        }
    }

    if (BTN_IS_PRESSED(gPlayer1Controller, BTN_DDOWN)) {
        D_83003CA6++;
        if (D_83003CA6 >= D_83003CA4) {
            D_83003CA6 = 0;
        }
    }

    if (D_83003CA6 != tmp) {
        func_80048B90(1);
    }

    if (BTN_IS_PRESSED(gPlayer1Controller, BTN_A)) {
        D_83003C80 = 2;
        D_83003C82 = 0;

        if (D_83003DE0.unk_00[D_83003CA0[D_83003CA6]] != NULL) {
            func_830025F8(D_83003CA0[D_83003CA6]);
        } else {
            func_830038DC();
            func_80048B90(4);
        }

        func_80048B90(2);
    } else if (BTN_IS_PRESSED(gPlayer1Controller, BTN_B)) {
        D_83003C80 = 3;
        D_83003C82 = 0xA;
        D_83003C90 = -1;
        func_80048B90(3);
    }
}

// func_830015EC, with the randomizer's button in the bottom bar, and its panels over the
// window while they're open
static void Randomizer_RulesDrawList(s16 arg0, s16 arg1) {
    static const char sButton[] = "Randomizer";
    s16 tmp;
    s32 i;
    s32 buttonX;
    char sp48[0x40];

    // The Z button's icon, right-aligned in the bottom bar, outside the text drawing the
    // caller started (func_830017C0)
    func_8001EBE0(FONT, 0);
    buttonX = arg0 + WINDOW_W - 0x18 - func_8001F5B0(FONT, 0, sButton);
    func_8001F444();
    gSPDisplayList(gDisplayListHead++, D_8006F518);
    func_8001CADC(buttonX - ICON_GAP - ICON_VISIBLE_W, arg1 + BOTTOM_BAR_Y, RANDOMIZER_Z_ICON_W, RANDOMIZER_Z_ICON_H,
                  (u8*)sRandomizerZIcon, RANDOMIZER_Z_ICON_W, 0);
    gSPDisplayList(gDisplayListHead++, D_8006F630);
    func_8001F3F4();

    func_8001EBE0(FONT, 0);
    func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);

    if (D_83003C9C != NULL) {
        if (D_83003C9C != D_83003CBC) {
            func_8002D5D4(0x27, D_83003C9C);
            func_8001F1E8(arg0 + 0x10, arg1 + 4, func_8002D7C0(sp48, sizeof(sp48), D_830039C0, 1));
        } else {
            func_8001F1E8(arg0 + 0x10, arg1 + 4, D_83003C9C);
        }
    }

    func_8001F1E8(arg0 + 0x30, arg1 + BOTTOM_BAR_Y, func_8002D7C0(NULL, 0, D_830039C0, 2));
    func_8001F1E8(buttonX, arg1 + BOTTOM_BAR_Y, sButton);
    func_8001EBE0(8, 0);

    for (i = 0; i < D_83003CA4; i++) {
        if (i != D_83003CA6) {
            func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
        } else {
            func_8001F324(0xFF, 0xFF, 0, 0xFF);
        }
        tmp = D_83003CA0[i];
        func_8001F1E8(arg0 + 0x54, arg1 + 0x26 + i * 0x18, D_83003CE0.unk_00[tmp]);
    }

    if (Randomizer_PanelIsOpen()) {
        // The caller ends the text drawing this is in (func_830017C0)
        func_8001F444();
        Randomizer_PanelDraw();
        func_8001F3F4();
    }
}

#endif
