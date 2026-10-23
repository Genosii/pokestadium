/*
 * The Stadium's cup screen with the Battle Frontier's modes (randomizer_stadium, see
 * randomizer_stadium.h): the cups in three columns, Rogue and Factory on the right, in the
 * two places the game has for two more cups. Their pictures are those of the two cups the US
 * version left out: Factory's its own (Electrode and Jolteon), Rogue's made from that cup's
 * background (picture 7 of the backgrounds archive), which the screen has no smaller one of.
 *
 * Picking Rogue or Factory opens "Choose your rules" (randomizer_panel.c) over the screen;
 * Start begins a run with them, as the cup screen would start a cup. The run carries the
 * mode of the left-out cup through the next screens (1 or 2), which skip the ball select for
 * it and show its picture, with the rule set of the cup chosen; the screen between battles
 * puts that cup's mode in its place before the first trainer (randomizer_cup.c). Back here
 * with B during a run, the rules open again; B there ends it.
 *
 * Built only with RANDOMIZER=1; empty otherwise so the default build still matches.
 */
#include "randomizer_stadium.h"

#ifdef RANDOMIZER

#include "src/12D80.h"
#include "src/1C720.h"
#include "src/1CF30.h"
#include "src/20470.h"
#include "src/232C0.h"
#include "src/26820.h"
#include "src/29BA0.h"
#include "src/2C1C0.h"
#include "src/2E110.h"
#include "src/3FB0.h"
#include "src/49790.h"
#include "src/DDC0.h"
#include "src/F420.h"
#include "src/controller.h"
#include "src/geo_layout.h"
#include "src/math_util.h"
#include "src/memory.h"

#define PICTURE_W 128 // a small cup's picture
#define PICTURE_H 96
#define BACKGROUND_W 320
#define BACKGROUND_TILE 16 // the backgrounds are 16x16 tiles of RGBA16, a row of 20 at a time
#define ROGUE_BACKGROUND 7 // D_84102430[STADIUM_SLOT_ROGUE]

#define TEXT_SIZE 0x10

static void Randomizer_StadiumSetup(void);
static void Randomizer_StadiumSelect(void);
static void Randomizer_StadiumCaption(s16 arg0);
static void Randomizer_StadiumLeave(void);

static RandomizerStadiumHooks sHooks = {
    Randomizer_StadiumSetup,
    Randomizer_StadiumSelect,
    Randomizer_StadiumCaption,
    Randomizer_StadiumLeave,
};

static u16 sRoguePicture[PICTURE_W * PICTURE_H];
static u16* sRogueBackground; // the full picture, which picking Rogue zooms into

// Where each cup's panel is, in front of the camera (see D_84102450 in fragment59 for where
// that puts them on the screen)
static const f32 sPanelPositions[STADIUM_SLOTS][2] = {
    { -232.0f, 113.0f }, { 96.0f, 113.0f },  { 96.0f, -29.0f },
    { -232.0f, -29.0f }, { -56.0f, 113.0f }, { -56.0f, -29.0f },
};

static const char* sNames[] = { "Rogue", "Factory" };
static const char* sLines[] = {
    "Battle a set number of trainers with no retries.\nWin rewards to build up your team.",
    "Rent a team and battle on. Swap in your\nopponents' Pok\xE9mon as you win.",
};

RandomizerStadiumHooks* Randomizer_StadiumEntry(void) {
    // Fragments aren't cleared when they're loaded
    Randomizer_PanelReset(NULL);
    Randomizer_FrontierReset();
    sRogueBackground = NULL;
    return &sHooks;
}

static s32 Randomizer_SlotKind(s32 slot) {
    return (slot == STADIUM_SLOT_ROGUE) ? FRONTIER_ROGUE : FRONTIER_FACTORY;
}

static s32 Randomizer_KindSlot(s32 kind) {
    return (kind == FRONTIER_ROGUE) ? STADIUM_SLOT_ROGUE : STADIUM_SLOT_FACTORY;
}

static s32 Randomizer_IsFrontierSlot(s32 slot) {
    return (slot == STADIUM_SLOT_ROGUE) || (slot == STADIUM_SLOT_FACTORY);
}

// A pixel of a background (16x16 tiles, a row of 20 at a time)
static u16 Randomizer_BackgroundPixel(u16* picture, s32 x, s32 y) {
    s32 tile = ((y / BACKGROUND_TILE) * (BACKGROUND_W / BACKGROUND_TILE)) + (x / BACKGROUND_TILE);

    return picture[(tile * BACKGROUND_TILE * BACKGROUND_TILE) + ((y % BACKGROUND_TILE) * BACKGROUND_TILE) +
                   (x % BACKGROUND_TILE)];
}

/*
 * Rogue's picture: its cup's background, 320x240, made 128x96 (two and a half times smaller,
 * each pixel the average of four), as the other cups' small pictures are their backgrounds
 */
static void Randomizer_RoguePicture(void) {
    u16* background = func_8000484C(D_84103444, ROGUE_BACKGROUND);
    s32 x;
    s32 y;
    s32 i;

    sRogueBackground = background;
    for (y = 0; y < PICTURE_H; y++) {
        for (x = 0; x < PICTURE_W; x++) {
            s32 sx = (x * 5) / 2;
            s32 sy = (y * 5) / 2;
            u32 r = 0;
            u32 g = 0;
            u32 b = 0;

            for (i = 0; i < 4; i++) {
                u16 pixel = Randomizer_BackgroundPixel(background, sx + (i & 1), sy + (i >> 1));

                r += (pixel >> 11) & 0x1F;
                g += (pixel >> 6) & 0x1F;
                b += (pixel >> 1) & 0x1F;
            }
            sRoguePicture[(y * PICTURE_W) + x] = ((r / 4) << 11) | ((g / 4) << 6) | ((b / 4) << 1) | 1;
        }
    }
    osWritebackDCache(sRoguePicture, sizeof(sRoguePicture));
}

// func_84101BFC, with the cups where they are now, and Rogue's and Factory's too
static void Randomizer_StadiumSetup(void) {
    s32 i;

    func_841003F0();
    Randomizer_RoguePicture();
    D_84103CD0[0] = func_84101894(D_300C000, 3);
    D_84103CD0[1] = func_84101A94((u8*)sRoguePicture, 1);
    D_84103CD0[2] = func_84101A94(D_3006000, 2);
    D_84103CD0[3] = func_84101894(D_3020760, 6);
    D_84103CD0[4] = func_84101A94(D_3014760, 4);
    D_84103CD0[5] = func_84101A94(D_301A760, 5);
    for (i = 0; i < STADIUM_SLOTS; i++) {
        // The big panels for the Poke and Prime Cups, the small ones for the rest
        func_8001BC34(&D_84103458[i], 0, 0, ((i == 0) || (i == 3)) ? D_84103CE8 : D_84103CEC);
        func_8000E88C(&D_84103458[i].unk_024, sPanelPositions[i][0], sPanelPositions[i][1], -579.0f);
    }
}

// Start in "Choose your rules": the run with them begins as a cup would
static void Randomizer_BeginRun(s32 slot) {
    s32 kind = Randomizer_SlotKind(slot);

    // The mode of the cup left out stays (the next screens skip the ball select for it and
    // show its picture), with the rule set of the cup chosen
    D_84102444[slot] = Randomizer_FrontierRuleSet(Randomizer_FrontierRules(kind)->cup);
    Randomizer_FrontierBegin(kind);
}

// func_84101EC8, with "Choose your rules" for Rogue and Factory
static void Randomizer_StadiumSelect(void) {
    s32 kind = RANDOMIZER_FRONTIER();
    s16 i;
    s16 picked;
    s32 rules;

    func_8000D1F0(0x15);
    Randomizer_StadiumSetup();
    // Back from the next screen during a run: on its mode, its rules open again
    D_84103CC8 = (kind != FRONTIER_NONE) ? Randomizer_KindSlot(kind) : 0;
    func_80006C6C(0xA);
    while (func_80007604() != 0) {
        func_800290B4();
        func_841014E4(0, 0, 0.0f);
    }

    for (i = 0; i < 10; i++) {
        func_800290B4();
        func_841014E4(i, 0, 0.0f);
    }
    if (kind != FRONTIER_NONE) {
        Randomizer_FrontierPanelOpen(kind);
    }
    do {
        func_800290B4();
        func_841014E4(0xA, 1, 0.0f);
        if (Randomizer_PanelIsOpen()) {
            picked = -1;
            rules = Randomizer_FrontierPanelInput(gPlayer1Controller);
            if (rules > 0) {
                Randomizer_BeginRun(D_84103CC8);
                picked = D_84103CC8;
            } else if ((rules < 0) && (RANDOMIZER_FRONTIER() != FRONTIER_NONE)) {
                // Out of the run: the player's own options back
                Randomizer_FrontierEnd();
            }
        } else {
            picked = func_84101D80();
            if ((picked >= 0) && Randomizer_IsFrontierSlot(picked)) {
                Randomizer_FrontierPanelOpen(Randomizer_SlotKind(picked));
                picked = -1;
            }
        }
    } while (picked == -1);

    D_84103CC8 = picked;

    if (D_84103CC8 < 6) {
        if ((D_84103CC8 == STADIUM_SLOT_ROGUE) && (sRogueBackground != NULL)) {
            D_8410344C = sRogueBackground; // already loaded, for its picture
        } else {
            D_8410344C = func_8000484C(D_84103444, D_84102430[D_84103CC8]);
        }

        for (i = 11; i < 21; i++) {
            func_800290B4();
            func_841014E4(i, 0, 0.0f);
        }

        for (i = 0; i < 6; i++) {
            func_8001BCF0(&D_84103458[i]);
        }

        for (i = 1; i < 10; i++) {
            func_800290B4();
            func_841014E4(0, 0, i / 10.0f);
        }

        func_841014E4(0, 0, 1.0f);
        func_841014E4(0, 0, 1.0f);
    } else {
        func_80007990(0xFFFF);
        func_8000D278(0x14);
        func_80006CB4(0xA);
        while (func_80007604() != 1) {
            func_800290B4();
            func_841014E4(0xA, 0, 0.0f);
        }
        func_800077B4(2);
    }
}

// func_84101210: the cup's name, levels and lines under the cups; Rogue's and Factory's are
// the randomizer's. Then "Choose your rules", if it's open.
static void Randomizer_StadiumCaption(s16 arg0) {
    s32 sp28;
    s16 temp_a3;
    s32 temp_v0;
    s32 frontier = Randomizer_IsFrontierSlot(D_84103CC8);
    const char* levels = "Level: by cup rules";

    if (arg0 < 0xB) {
        sp28 = arg0 * 9;
        temp_a3 = (sp28 + 2);
    } else {
        sp28 = arg0 * 9;
        temp_a3 = (0xB6 - sp28);
    }

    if (temp_a3 >= 0xA) {
        func_84100704(0x38, ((0x5C - temp_a3) / 2) + 0x15C, 0x210, temp_a3, 0x1E, 0x1E, 0x82, 0x96);
    }

    if ((sp28 == 0x5A) && (D_84103CC8 < 6)) {
        func_8001F3F4();
        func_8001EBE0(TEXT_SIZE, 0);
        func_8001F324(0xFF, 0xFF, 0x64, 0xFF);
        if (frontier) {
            func_8001F1E8(0x48, 0x160, "%s", sNames[D_84103CC8 - STADIUM_SLOT_ROGUE]);
            // Right-aligned where the cups' levels end
            func_8001F1E8(0x234 - func_8001F5B0(TEXT_SIZE, 0, "%s", levels), 0x160, "%s", levels);
        } else {
            func_8001F1E8(0x48, 0x160, func_8002D7C0(NULL, 0, D_84103440, D_84103CC8));
            func_8001F1E8(0x18C, 0x160, func_8002D7C0(NULL, 0, D_84103440, 0xC));
            func_8001F1E8(func_8001F5B0(TEXT_SIZE, 0, func_8002D7C0(NULL, 0, D_84103440, 0xC)) + 0x190, 0x160, "%d~%d",
                          D_84102528[D_84103CC8][0], D_84102528[D_84103CC8][1]);
        }
        func_8001EBE0(8, 0);
        func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
        func_8001F3B4(0x18);
        if (frontier) {
            func_8001F1E8(0x48, 0x182, "%s", sLines[D_84103CC8 - STADIUM_SLOT_ROGUE]);
        } else {
            func_8001F1E8(0x48, 0x182, func_8002D7C0(NULL, 0, D_84103440, D_84103CC8 + 6));
        }
        func_8001F444();

        if ((D_800AE540.unk_11F2 == 1) && !frontier) {
            temp_v0 = func_8001F5B0(TEXT_SIZE, 0, func_8002D7C0(NULL, 0, D_84103440, D_84103CC8));

            gSPDisplayList(gDisplayListHead++, D_8006F518);

            func_8001C6AC(temp_v0 + 0x4C, 0x160, 0x1C, 0x1C, D_302E840, 0x1C, 0);

            gSPDisplayList(gDisplayListHead++, D_8006F630);
        }
    }

    Randomizer_PanelDraw();
}

// As the screen ends: the rules saved, if "Choose your rules" changed them (it takes a few
// seconds, which the screen changing covers)
static void Randomizer_StadiumLeave(void) {
    Randomizer_FrontierSave();
}

#endif
