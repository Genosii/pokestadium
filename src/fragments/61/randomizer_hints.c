/*
 * Control hints for the randomizer on the Pokemon pick screen: a line under the Pokemon
 * list, in the style of the battle-select screen's "* [L] Button to cancel" footer, so
 * the randomizer's buttons can be found without a guide. Built only with RANDOMIZER=1;
 * empty otherwise so the default build still matches.
 */
#include "randomizer.h"

#ifdef RANDOMIZER

#include "src/1CF30.h"
#include "src/20470.h"
#include "src/fragments/randomizer_z_icon.h"

#define HINTS_Y 444
#define SCREEN_W 640
#define FONT 8
#define BULLET_W 23 // from the bullet to the button, as on the battle-select screen
#define TEXT_GAP 6  // from the button to its text
#define ITEM_GAP 16

// The summary screen's C button icons (fragment61_2E2C20.c)
#define C_ICON_W 0x1C
#define C_ICON_H 0x1A

typedef struct RandomizerHint {
    /* 0x00 */ u8* icon;
    /* 0x04 */ s16 w;
    /* 0x06 */ s16 h;
    /* 0x08 */ const char* text;
} RandomizerHint; // size = 0xC

static RandomizerHint sHints[] = {
    { (u8*)sRandomizerZIcon, RANDOMIZER_Z_ICON_W, RANDOMIZER_Z_ICON_H, "Random team" },
    { D_2014F00, C_ICON_W, C_ICON_H, "Options" },
    { D_2015A60, C_ICON_W, C_ICON_H, "Moves" },
};

// The Z icon's right quarter is empty, like the game's L and R icons
#define ICON_VISIBLE_W(hint) (((hint)->icon == (u8*)sRandomizerZIcon) ? 26 : (hint)->w)

// Called at the end of the pick screen's drawing, every frame
void Randomizer_HintsDraw(void) {
    s32 widths[ARRAY_COUNT(sHints)];
    s32 total = 0;
    s32 x;
    s32 i;

    // Z edits a registered team in "Check registered Pokemon" (randomizer_editor.c)
    sHints[0].text = Randomizer_EditorInCheck() ? "Edit team" : "Random team";

    for (i = 0; i < ARRAY_COUNT(sHints); i++) {
        widths[i] = BULLET_W + ICON_VISIBLE_W(&sHints[i]) + TEXT_GAP + func_8001F5B0(FONT, 0, "%s", sHints[i].text);
        total += widths[i] + ((i != 0) ? ITEM_GAP : 0);
    }

    gSPDisplayList(gDisplayListHead++, D_8006F518);
    x = (SCREEN_W - total) / 2;
    for (i = 0; i < ARRAY_COUNT(sHints); i++) {
        s32 y = HINTS_Y + ((RANDOMIZER_Z_ICON_H - sHints[i].h) / 2);

        // The Z icon is IA8, the game's C button icons RGBA16
        if (sHints[i].icon == (u8*)sRandomizerZIcon) {
            func_8001CADC(x + BULLET_W, y, sHints[i].w, sHints[i].h, sHints[i].icon, sHints[i].w, 0);
        } else {
            func_8001C6AC(x + BULLET_W, y, sHints[i].w, sHints[i].h, sHints[i].icon, sHints[i].w, 0);
        }
        x += widths[i] + ITEM_GAP;
    }
    gSPDisplayList(gDisplayListHead++, D_8006F630);

    func_8001F3F4();
    func_8001EBE0(FONT, 0);
    func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
    x = (SCREEN_W - total) / 2;
    for (i = 0; i < ARRAY_COUNT(sHints); i++) {
        func_8001F1E8(x, HINTS_Y + 2, "*");
        func_8001F1E8(x + BULLET_W + ICON_VISIBLE_W(&sHints[i]) + TEXT_GAP, HINTS_Y, "%s", sHints[i].text);
        x += widths[i] + ITEM_GAP;
    }
    func_8001F444();
}

#endif
