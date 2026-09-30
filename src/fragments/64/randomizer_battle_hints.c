/*
 * The battle-select screen's footer with the randomizer's Z button added:
 * "* [L] Button to cancel  * [R] Button to check  * [Z] Random pick", drawn the way the
 * game draws its two items (func_84800020) and centred as one line. Part of the
 * randomizer's battle-select fragment (randomizer_battle.h). Built only with
 * RANDOMIZER=1; empty otherwise so the default build still matches.
 */
#include "randomizer_battle.h"

#ifdef RANDOMIZER

#include "src/1CF30.h"
#include "src/20470.h"
#include "src/2E110.h"
#include "src/fragments/randomizer_z_icon.h"

#define SCREEN_W 640
#define FOOTER_Y 0x1A8
#define FONT 8
#define ICON_W 0x20
#define ICON_H 0x18
#define BULLET_TO_ICON 0x17
#define ICON_TO_TEXT 0x20
#define ITEM_GAP 20

// The game's texts for its two items, in the screen's text file
#define TEXT_CANCEL 2
#define TEXT_CHECK 3

#define NUM_ITEMS 3

// texts: the screen's text file (D_84803790)
void Randomizer_DrawFooter(char** texts) {
    u8* icons[NUM_ITEMS];
    char* labels[NUM_ITEMS];
    s32 widths[NUM_ITEMS];
    s32 total = 0;
    s32 x;
    s32 i;

    icons[0] = D_3002220;
    icons[1] = D_3002820;
    icons[2] = (u8*)sRandomizerZIcon;
    labels[0] = func_8002D7C0(NULL, 0, texts, TEXT_CANCEL);
    labels[1] = func_8002D7C0(NULL, 0, texts, TEXT_CHECK);
    labels[2] = "Random pick";

    for (i = 0; i < NUM_ITEMS; i++) {
        widths[i] = BULLET_TO_ICON + ICON_TO_TEXT + func_8001F5B0(FONT, 0, "%s", labels[i]);
        total += widths[i] + ((i != 0) ? ITEM_GAP : 0);
    }

    // The game's L and R icons as the game draws them (copy mode); the Z icon, IA8, which
    // copy mode can't draw, blended
    gSPDisplayList(gDisplayListHead++, D_8006F4E0);
    x = (SCREEN_W - total) / 2;
    for (i = 0; i < NUM_ITEMS; i++) {
        if (icons[i] != (u8*)sRandomizerZIcon) {
            func_8001C6AC(x + BULLET_TO_ICON, FOOTER_Y, ICON_W, ICON_H, icons[i], ICON_W, 0x200000);
        } else {
            gSPDisplayList(gDisplayListHead++, D_8006F518);
            func_8001CADC(x + BULLET_TO_ICON, FOOTER_Y, ICON_W, ICON_H, icons[i], ICON_W, 0);
            gSPDisplayList(gDisplayListHead++, D_8006F4E0);
        }
        x += widths[i] + ITEM_GAP;
    }
    gSPDisplayList(gDisplayListHead++, D_8006F630);

    func_8001F3F4();
    func_8001EBE0(FONT, 0);
    x = (SCREEN_W - total) / 2;
    for (i = 0; i < NUM_ITEMS; i++) {
        func_8001F1E8(x, FOOTER_Y + 2, "*");
        func_8001F1E8(x + BULLET_TO_ICON + ICON_TO_TEXT, FOOTER_Y, "%s", labels[i]);
        x += widths[i] + ITEM_GAP;
    }
    func_8001F444();
}

#endif
