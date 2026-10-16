/*
 * The Options screen's "Battle camera" line (randomizer_options, see randomizer_options.h):
 * Custom, the battle's own camera with the randomizer's shots, or Original, the battle's
 * own only; switched with A and saved at once. Then the camera tests, to see each of the
 * randomizer's shots in a battle (T.): "Menu camera", "Attack camera" and "Hit camera", each
 * Auto (picked at random, as usual) or one shot, every attack filmed when the attack's or the
 * hit's is set; A goes to the next, Z and A back. Kept until the console is switched off, not
 * saved. (The randomizer's options are set on the Pokemon pick screen.) Built only with
 * RANDOMIZER=1; empty otherwise so the default build still matches.
 */
#include "randomizer_options.h"

#ifdef RANDOMIZER

#include "src/1CF30.h"
#include "src/20470.h"
#include "src/2E110.h"
#include "src/49790.h"
#include "src/6A40.h"
#include "src/controller.h"
#include "src/fragments/61/randomizer_menu.h"

// What func_82C00658 has, with the randomizer's lines
#define WINDOW_H (0xBA + (RANDOMIZER_OPTIONS_LINES * LINE_H))
#define WINDOW_Y (0xE5 - ((RANDOMIZER_OPTIONS_LINES + 2) * LINE_H / 2))
#define LINE_H 0x20
#define LINE_TEXT_X 0xBB
#define VALUE_BOX_X 0x168
#define VALUE_CENTER_X 0x1A4

static void Randomizer_OptionsDraw(s16 arg0, s32 arg1);
static void Randomizer_OptionsRun(void);
static void Randomizer_OptionsLeave(void);

static RandomizerOptionsHooks sHooks = {
    Randomizer_OptionsDraw,
    Randomizer_OptionsRun,
    Randomizer_OptionsLeave,
};

RandomizerOptionsHooks* Randomizer_OptionsEntry(void) {
    return &sHooks;
}

// The camera tests' values, short enough for the value box
static const char* sMenuShots[CAMERA_MENUS + 1] = {
    "Auto",     "Pan",      "Orbit",    "Low side", "Chase", "Overhead",
    "Close-up", "Fly-over", "Shoulder", "Split",    "POV",   "Behind",
};
static const char* sAttackShots[CAMERA_ATTACKS + 1] = {
    "Auto", "Low", "Push", "Behind", "Face", "Track", "Game Boy", "Aim",
};
static const char* sHitShots[CAMERA_HITS + 1] = { "Auto", "Close", "Low", "Swing" };

// The text of a line: the game's (D_82C01660), and the randomizer's
static char* Randomizer_OptionsLine(s32 line) {
    static s32 sGameLines[RANDOMIZER_OPTIONS_COUNT] = { 5, 6, -1, -1, -1, -1, 7, 8 };
    static char* sLines[RANDOMIZER_OPTIONS_LINES] = { "Battle camera", "Menu camera", "Attack camera", "Hit camera" };

    if ((line >= RANDOMIZER_OPTIONS_CAMERA) && (line < RANDOMIZER_OPTIONS_CAMERA + RANDOMIZER_OPTIONS_LINES)) {
        return sLines[line - RANDOMIZER_OPTIONS_CAMERA];
    }
    return func_8002D7C0(NULL, 0, D_82C01660, sGameLines[line]);
}

// Its value, for the lines with one
static const char* Randomizer_OptionsValue(s32 line) {
    switch (line) {
        case 0:
            return func_8002D7C0(NULL, 0, D_82C01660, (D_82C01666 & 1) != 0);
        case 1:
            return func_8002D7C0(NULL, 0, D_82C01660, ((D_82C01666 & 2) != 0) + 2);
        case RANDOMIZER_OPTIONS_CAMERA:
            return Randomizer_State()->originalCamera ? "Original" : "Custom";
        case RANDOMIZER_OPTIONS_CAMERA + 1:
            return sMenuShots[Randomizer_State()->menuCamera % (CAMERA_MENUS + 1)];
        case RANDOMIZER_OPTIONS_CAMERA + 2:
            return sAttackShots[(Randomizer_State()->attackCamera & 0xF) % (CAMERA_ATTACKS + 1)];
        case RANDOMIZER_OPTIONS_CAMERA + 3:
            return sHitShots[(Randomizer_State()->attackCamera >> 4) % (CAMERA_HITS + 1)];
        default:
            return NULL;
    }
}

// func_82C00658 with the randomizer's line
static void Randomizer_OptionsDraw(s16 arg0, s32 arg1) {
    s16 height = (arg0 * (WINDOW_H - 0xA)) / 8 + 0xA;
    s16 y = ((WINDOW_H - height) / 2) + WINDOW_Y;
    char* title;
    s32 i;

    if (arg0 <= 0) {
        return;
    }
    func_80020754(0x89, y, 0x16E, height);
    if (arg0 != 8) {
        return;
    }

    func_82C0025C(0x90, y + 7, 0x160, 0x20, 0x3C, 0x3C, 0xA0, 0xFF);
    func_82C0025C(0x90, y + 0x27, 0x160, 0x8C + (RANDOMIZER_OPTIONS_LINES * LINE_H), 0x1E, 0x1E, 0x64, 0xFF);

    if (arg1 != 0) {
        func_80020928(0x95, y + (D_82C01664 * LINE_H) + 0x31);
    }

    gSPDisplayList(gDisplayListHead++, D_8006F518);
    for (i = 0; i < RANDOMIZER_OPTIONS_COUNT; i++) {
        if (Randomizer_OptionsValue(i) != NULL) {
            func_82C00574(VALUE_BOX_X, y + 0x30 + (i * LINE_H));
        }
    }
    gSPDisplayList(gDisplayListHead++, D_8006F630);

    func_8001F3F4();
    func_8001EBE0(0x10, 0);
    title = func_8002D7C0(NULL, 0, D_82C01660, 4);
    func_8001F1E8(0x140 - (func_8001F5B0(0x10, 0, title) / 2), y + 0xA, title);

    for (i = 0; i < RANDOMIZER_OPTIONS_COUNT; i++) {
        const char* value = Randomizer_OptionsValue(i);
        s32 lineY = y + 0x31 + (i * LINE_H);

        func_8001F324(0xFF, 0xFF, (D_82C01664 == i) ? 0 : 0xFF, 0xFF);
        func_8001F1E8(LINE_TEXT_X, lineY, Randomizer_OptionsLine(i));
        if (value != NULL) {
            func_8001F1E8(VALUE_CENTER_X - (func_8001F5B0(0x10, 0, value) / 2), lineY, value);
        }
    }
    func_8001F444();
}

// The next value of n out of count, or the one before with Z held
static s32 Randomizer_OptionsStep(s32 n, s32 count) {
    return (gPlayer1Controller->buttonDown & BTN_Z) ? ((n + count - 1) % count) : ((n + 1) % count);
}

// One of the randomizer's lines picked: the camera switched, or a camera test's shot changed
static void Randomizer_OptionsRun(void) {
    RandomizerState* state = Randomizer_State();
    s32 hit = state->attackCamera >> 4;
    s32 attack = state->attackCamera & 0xF;

    func_80048B90(2);
    switch (D_82C01664 - RANDOMIZER_OPTIONS_CAMERA) {
        case 0:
            state->originalCamera ^= 1;
            break;
        case 1:
            state->menuCamera = Randomizer_OptionsStep(state->menuCamera, CAMERA_MENUS + 1);
            break;
        case 2:
            attack = Randomizer_OptionsStep(attack, CAMERA_ATTACKS + 1);
            break;
        case 3:
            hit = Randomizer_OptionsStep(hit, CAMERA_HITS + 1);
            break;
    }
    state->attackCamera = (hit << 4) | attack;
}

// As the screen ends: saved, if it changed (which takes a few seconds, randomizer_pick.c)
static void Randomizer_OptionsLeave(void) {
    Randomizer_SaveSettings();
}

#endif
