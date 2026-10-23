/*
 * The Stadium's cup screen with the Battle Frontier's modes (randomizer_stadium, see
 * randomizer_stadium.h): the cups in three columns, Rogue and Factory on the right, in the
 * two places the game has for two more cups, every panel the size of the Poke and Prime
 * Cups' with the same gap between them across and down. Rogue's and Factory's pictures are
 * those of the two cups the US version left out: Factory's its own (Electrode and Jolteon),
 * Rogue's made from that cup's background (picture 7 of the backgrounds archive), which the
 * screen has no smaller one of. The small pictures (128x96) are made 152x114 as the screen
 * starts.
 *
 * Picking Rogue or Factory zooms into its picture as picking a cup does, and goes on to
 * Battle, Rules and Registration; Battle opens "Choose your rules" on the pick screen
 * (randomizer_frontier.c). The run carries the mode of the left-out cup through the next
 * screens (1 or 2), which skip the ball select for it and show its picture, with the Poke
 * Cup's rule set (level 50; Dragonite, which can't be lower, 55); the screen between battles
 * puts the Poke Cup's mode in its place before the first trainer (randomizer_cup.c). Back
 * here from those screens, the run is over before it began.
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
#include "src/util.h"

#define SMALL_W 128 // the small cups' pictures
#define SMALL_H 96
#define PICTURE_W 152 // and the big ones', which every panel has here
#define PICTURE_H 114
#define PICTURE_STRIP 12 // rows drawn at a time (func_84101894)
#define BACKGROUND_W 320
#define BACKGROUND_H 240
#define BACKGROUND_TILE 16 // the backgrounds are 16x16 tiles of RGBA16, a row of 20 at a time
#define ROGUE_BACKGROUND 7 // D_84102430[STADIUM_SLOT_ROGUE]
#define ROGUE_BRIGHTER 13  // tenths: Rogue's picture, darker than the others, made brighter
#define POKE_CUP_RULES 3

#define TEXT_SIZE 0x10

static void Randomizer_StadiumSetup(void);
static void Randomizer_StadiumSelect(void);
static void Randomizer_StadiumCaption(s16 arg0);

static RandomizerStadiumHooks sHooks = {
    Randomizer_StadiumSetup,
    Randomizer_StadiumSelect,
    Randomizer_StadiumCaption,
};

// The pictures made big, for slots 1, 2, 4 and 5 (Rogue, Factory, the Petit and Pika Cups)
#define BIG_PICTURES 4
static u16 sPictures[BIG_PICTURES][PICTURE_W * PICTURE_H];
static u16* sRogueBackground; // the full picture, which picking Rogue zooms into

// Where each cup's panel is, in front of the camera: on the screen x + 324 and 183 - y, its
// top left (see D_84102450 in fragment59)
static const f32 sPanelPositions[STADIUM_SLOTS][2] = {
    { -252.0f, 113.0f }, { 92.0f, 113.0f }, { 92.0f, -21.0f },
    { -252.0f, -21.0f }, { -80.0f, 113.0f }, { -80.0f, -21.0f },
};

// Where a panel's trophy goes, for the cups won (as func_84101A94 puts it on a small panel)
#define TROPHY_X 0x6A
#define TROPHY_Y 0x42
#define TROPHY_W 0x30
#define TROPHY_H 0x2A

static const char* sNames[] = { "Rogue", "Factory" };
static const char* sLines[] = {
    "Battle a set number of trainers with no retries.\nWin rewards to build up your team.",
    "Rent a team and battle on. Swap in your\nopponents' Pok\xE9mon as you win.",
};

RandomizerStadiumHooks* Randomizer_StadiumEntry(void) {
    // Fragments aren't cleared when they're loaded
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

// A pixel of a small picture (rows of 128)
static u16 Randomizer_SmallPixel(u16* picture, s32 x, s32 y) {
    return picture[(y * SMALL_W) + x];
}

/*
 * A picture made 152x114 from a w x h one, each pixel from the four nearest (bilinear, in
 * 256ths), its colours times bright tenths
 */
static void Randomizer_Scale(u16* out, u16* picture, s32 w, s32 h, u16 (*pixel)(u16*, s32, s32), s32 bright) {
    s32 x;
    s32 y;
    s32 i;

    for (y = 0; y < PICTURE_H; y++) {
        s32 fy = (y * (h - 1) * 256) / (PICTURE_H - 1);
        s32 sy = fy >> 8;
        s32 dy = fy & 0xFF;

        for (x = 0; x < PICTURE_W; x++) {
            s32 fx = (x * (w - 1) * 256) / (PICTURE_W - 1);
            s32 sx = fx >> 8;
            s32 dx = fx & 0xFF;
            s32 rgb[3] = { 0, 0, 0 };

            for (i = 0; i < 4; i++) {
                s32 px = sx + (i & 1);
                s32 py = sy + (i >> 1);
                s32 weight = ((i & 1) ? dx : 256 - dx) * ((i >> 1) ? dy : 256 - dy);
                u16 value;

                if ((weight == 0) || (px >= w) || (py >= h)) {
                    continue;
                }
                value = pixel(picture, px, py);
                rgb[0] += ((value >> 11) & 0x1F) * weight;
                rgb[1] += ((value >> 6) & 0x1F) * weight;
                rgb[2] += ((value >> 1) & 0x1F) * weight;
            }
            for (i = 0; i < 3; i++) {
                rgb[i] = ((rgb[i] >> 16) * bright) / 10;
                if (rgb[i] > 0x1F) {
                    rgb[i] = 0x1F;
                }
            }
            out[(y * PICTURE_W) + x] = (rgb[0] << 11) | (rgb[1] << 6) | (rgb[2] << 1) | 1;
        }
    }
    osWritebackDCache(out, PICTURE_W * PICTURE_H * sizeof(u16));
}

// The pictures made big: Rogue's from its background, the others from their small ones
static void Randomizer_Pictures(void) {
    sRogueBackground = func_8000484C(D_84103444, ROGUE_BACKGROUND);
    Randomizer_Scale(sPictures[0], sRogueBackground, BACKGROUND_W, BACKGROUND_H, Randomizer_BackgroundPixel,
                     ROGUE_BRIGHTER);
    // The small pictures are in segment 3 (stadium_select_ui), which the CPU reads through
    // where it's loaded
    Randomizer_Scale(sPictures[1], (u16*)Util_ConvertAddrToVirtAddr((uintptr_t)D_3006000), SMALL_W, SMALL_H,
                     Randomizer_SmallPixel, 10);
    Randomizer_Scale(sPictures[2], (u16*)Util_ConvertAddrToVirtAddr((uintptr_t)D_3014760), SMALL_W, SMALL_H,
                     Randomizer_SmallPixel, 10);
    Randomizer_Scale(sPictures[3], (u16*)Util_ConvertAddrToVirtAddr((uintptr_t)D_301A760), SMALL_W, SMALL_H,
                     Randomizer_SmallPixel, 10);
}

/*
 * A big panel's texture with a picture made big (func_84101894, without the Poke and Prime
 * Cups' balls), and the small cups' trophy once won (func_84101A94) for rule set ruleSet
 */
static unk_D_80068BB0* Randomizer_BigPanel(u16* picture, s32 ruleSet) {
    unk_D_80068BB0* panel = func_80006314(0, 2, 0xA4, 0x7E, 1);
    s32 i;

    func_8410048C(panel, 0xA0, 0x7A);
    gSPDisplayList(gDisplayListHead++, D_8006F518);
    for (i = 0; i < PICTURE_H; i += PICTURE_STRIP) {
        s32 rows = ((PICTURE_H - i) < PICTURE_STRIP) ? (PICTURE_H - i) : PICTURE_STRIP;

        func_8001C6AC(4, i + 4, PICTURE_W, rows, (u8*)(picture + (i * PICTURE_W)), PICTURE_W, 0);
    }
    if ((ruleSet != 0) && (D_84103CF0.unk_00 & (1 << ruleSet))) {
        func_8001C6AC(TROPHY_X, TROPHY_Y, TROPHY_W, TROPHY_H, D_84102534[(ruleSet == 4) ? 5 : 4], TROPHY_W, 0);
    }
    gSPDisplayList(gDisplayListHead++, D_8006F630);
    return panel;
}

// func_84101BFC, with the cups where they are now, all as big, and Rogue's and Factory's too
static void Randomizer_StadiumSetup(void) {
    s32 i;

    func_841003F0();
    Randomizer_Pictures();
    D_84103CD0[0] = func_84101894(D_300C000, 3);
    D_84103CD0[1] = Randomizer_BigPanel(sPictures[0], 0);
    D_84103CD0[2] = Randomizer_BigPanel(sPictures[1], 0);
    D_84103CD0[3] = func_84101894(D_3020760, 6);
    D_84103CD0[4] = Randomizer_BigPanel(sPictures[2], 4);
    D_84103CD0[5] = Randomizer_BigPanel(sPictures[3], 5);
    for (i = 0; i < STADIUM_SLOTS; i++) {
        func_8001BC34(&D_84103458[i], 0, 0, D_84103CE8);
        func_8000E88C(&D_84103458[i].unk_024, sPanelPositions[i][0], sPanelPositions[i][1], -579.0f);
    }
}

// func_84101EC8, with Rogue and Factory starting a Battle Frontier run as they're picked
static void Randomizer_StadiumSelect(void) {
    s32 kind = RANDOMIZER_FRONTIER();
    s16 i;
    s16 picked;

    func_8000D1F0(0x15);
    Randomizer_StadiumSetup();
    // Back from the next screens before the run's first battle: it's over, on its mode
    D_84103CC8 = 0;
    if (kind != FRONTIER_NONE) {
        D_84103CC8 = Randomizer_KindSlot(kind);
        Randomizer_FrontierEnd();
    }
    func_80006C6C(0xA);
    while (func_80007604() != 0) {
        func_800290B4();
        func_841014E4(0, 0, 0.0f);
    }

    for (i = 0; i < 10; i++) {
        func_800290B4();
        func_841014E4(i, 0, 0.0f);
    }
    do {
        func_800290B4();
        func_841014E4(0xA, 1, 0.0f);
        picked = func_84101D80();
    } while (picked == -1);

    D_84103CC8 = picked;

    if (D_84103CC8 < 6) {
        if (Randomizer_IsFrontierSlot(D_84103CC8)) {
            // A run of the mode, with the Poke Cup's rule set (its rules are chosen as its
            // first battle is, Randomizer_FrontierBegin)
            D_84102444[D_84103CC8] = POKE_CUP_RULES;
            Randomizer_FrontierSelect(Randomizer_SlotKind(D_84103CC8));
        }
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
// the randomizer's, with the Poke Cup's levels
static void Randomizer_StadiumCaption(s16 arg0) {
    s32 sp28;
    s16 temp_a3;
    s32 temp_v0;
    s32 frontier = Randomizer_IsFrontierSlot(D_84103CC8);
    s32 levels = frontier ? 0 : D_84103CC8; // the Poke Cup's for Rogue and Factory

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
        } else {
            func_8001F1E8(0x48, 0x160, func_8002D7C0(NULL, 0, D_84103440, D_84103CC8));
        }
        func_8001F1E8(0x18C, 0x160, func_8002D7C0(NULL, 0, D_84103440, 0xC));
        func_8001F1E8(func_8001F5B0(TEXT_SIZE, 0, func_8002D7C0(NULL, 0, D_84103440, 0xC)) + 0x190, 0x160, "%d~%d",
                      D_84102528[levels][0], D_84102528[levels][1]);
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
}

#endif
