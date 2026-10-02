/*
 * The title screen's face-off (randomizer_title, see randomizer_title.h): two Pokemon, picked
 * at random every time the title starts, as live 3D models facing each other in the lower
 * half of the title picture and taking turns attacking.
 *
 * Each is drawn the way the pick screen's rental card draws a Pokemon, with a model widget
 * of the game's (src/1AB70.c): a camera and a model, which func_8001B9D4 renders with its
 * animation into a picture every frame. Here both widgets render into the same picture, the
 * canvas, made the way func_8001B1FC makes a widget's: the one drawn first paints the patch
 * of the title picture behind the canvas into it, the other draws on top of that, and the
 * canvas is then drawn onto the screen. The canvas is 256 pixels wide, as many as the
 * widget's picture loader takes in a row, so the attacks can reach across the middle.
 *
 * The animations are the ones the battles play. Every species has a table in the ROM (at
 * _70D3A0_ROM_START plus D_80075BD0's offset for it, 0xBC0 bytes, as func_84302658 in
 * fragment62 loads it) of 0x10-byte entries whose first byte is the model's animation: one per
 * move, at the move's id - 1, then the idle stance (165) and the reaction to a hit (168),
 * among others. A turn plays the animation of one of the attacker's damaging moves, then,
 * partway through it, the defender's hit reaction, then both stand in their idle stance for a
 * moment, and the other attacks.
 *
 * Built only with RANDOMIZER=1; empty otherwise so the default build still matches.
 */
#include "randomizer_title.h"

#ifdef RANDOMIZER

#include "src/11BA0.h"
#include "src/12D80.h"
#include "src/17300.h"
#include "src/19840.h"
#include "src/1AB70.h"
#include "src/1C720.h"
#include "src/1CF30.h"
#include "src/20470.h"
#include "src/2E460.h"
#include "src/3FB0.h"
#include "src/6BC0.h"
#include "src/F420.h"
#include "src/memory.h"
#include "src/randomizer_save.h"
#include "src/stage_loader.h"

#define SIDES 2

// The canvas, on the screen; its corner on one of the title picture's 16x16 tiles
#define CANVAS_X 32
#define CANVAS_Y 64
#define CANVAS_W 256
#define CANVAS_H 128
#define CANVAS_ROWS 8 // rows of it drawn at a time (the texture memory holds 2048 pixels)
#define BACKGROUND_W 320
#define TILE 16

// How far each Pokemon stands from the middle, in the models' units (about 7 to a pixel)
#ifndef FIGHTER_X
#define FIGHTER_X 420.0f
#endif
#ifndef FIGHTER_ANGLE
#define FIGHTER_ANGLE 0x2800 // turned from facing the viewer towards the other
#endif

/*
 * The cameras: the rental card's (1250 units away, looking 10 degrees down and 20 to the
 * side), turned to look straight on, a little higher (the models a little lower), and
 * closer with a wider view (the models about 85% of the card's size, for room to move).
 * Closer, because some models have parts that are only drawn within a depth range
 * (func_80014214), and the card's distance is at the edge of it: a little farther, and
 * Pikachu comes out pink.
 */
#ifndef CAMERA_DISTANCE
#define CAMERA_DISTANCE 1100.0f
#endif
#ifndef CAMERA_FOVY
#define CAMERA_FOVY 45.0f
#endif
#ifndef CAMERA_RAISE
#define CAMERA_RAISE 100.0f
#endif

#define NUM_SPECIES 151
#define NUM_MOVES 165

// A species' battle animation table (see above)
#define ANIM_ENTRY_SIZE 0x10
#define ANIM_ENTRIES 188
#define ANIM_ENTRY_IDLE 165
#define ANIM_ENTRY_HIT 168
#define MAX_ATTACKS 8

#define PAUSE_FRAMES 45     // in the idle stance between turns
#define HIT_AT_PERCENT 45   // how far into the attack the defender reacts
#define TURN_MAX_FRAMES 300 // a turn ends by then even if an animation never does

enum {
    PHASE_PAUSE,
    PHASE_ATTACK,
};

typedef struct RandomizerFighter {
    /* 0x00 */ unk_func_8001B1FC* widget;
    /* 0x04 */ s16 lastFrame; // of the attack or hit reaction playing, to see it end
    /* 0x06 */ u8 idle;
    /* 0x07 */ u8 hit;
    /* 0x08 */ u8 busy; // playing an attack or the hit reaction
    /* 0x09 */ u8 numAttacks;
    /* 0x0A */ u8 attacks[MAX_ATTACKS];
} RandomizerFighter; // size = 0x12

// Left and right, turned to face each other (the widget's own angle faces the viewer)
static const f32 sFighterX[SIDES] = { -FIGHTER_X, FIGHTER_X };
static const s16 sFighterAngle[SIDES] = { FIGHTER_ANGLE, -FIGHTER_ANGLE };

static RandomizerFighter sFighters[SIDES];
static unk_D_80068BB0* sCanvas;
static u8 sReady;
static u8 sPhase;
static u8 sAttacker;
static u8 sHitStarted;
static s16 sTimer;
static u32 sRandomState;

// Mixes a number's bits thoroughly (a 32-bit hash finaliser, as in randomizer_intro.c)
static u32 Randomizer_FaceoffMix(u32 x) {
    x ^= x >> 16;
    x *= 0x7FEB352D;
    x ^= x >> 15;
    x *= 0x846CA68B;
    x ^= x >> 16;
    return x;
}

// A number from 0 to n - 1 (the team generator's 32-bit LCG; its fragment isn't loaded here)
static s32 Randomizer_FaceoffBelow(s32 n) {
    sRandomState = (sRandomState * 0x19660D) + 0x3C6EF35F;
    return ((u64)sRandomState * (u32)n) >> 32;
}

/*
 * The seed: the CPU's cycle count, which an emulator repeats from boot to boot if nothing is
 * pressed, so mixed with the intro's count of boots in the save file (src/randomizer_save.h),
 * which the title has loaded. Only read: the intro counts the boots.
 */
static u32 Randomizer_FaceoffSeed(void) {
    u32 boots = 0;

    if (func_80028AFC(RANDOMIZER_SAVE_BANK)) {
        bcopy(RANDOMIZER_SAVE_AT(RANDOMIZER_SAVE_BOOT_COUNT + 4), &boots, sizeof(boots));
    }
    return Randomizer_FaceoffMix(osGetCount() ^ Randomizer_FaceoffMix(boots + 0x5EED));
}

static unk_D_86002F58_004_000* Randomizer_FaceoffModel(s32 side) {
    return sFighters[side].widget->unk_24;
}

// How many animations a side's model has
static s32 Randomizer_FaceoffAnimCount(s32 side) {
    return Randomizer_FaceoffModel(side)->unk_000.unk_0C->unk_28(0, 0)->unk_04;
}

// A side's idle stance, hit reaction and attacks, from its species' battle animation table
static void Randomizer_FaceoffAnimations(s32 side, s32 species, u8* table) {
    RandomizerFighter* fighter = &sFighters[side];
    s32 count = Randomizer_FaceoffAnimCount(side);
    u32 start = (u32)_70D3A0_ROM_START + ((u32)D_80075BD0[species - 1] & 0xFFFFFF);
    s32 move;
    s32 i;

    func_80003B30((u32)table, start, start + (ANIM_ENTRIES * ANIM_ENTRY_SIZE), 0);

    fighter->idle = table[ANIM_ENTRY_IDLE * ANIM_ENTRY_SIZE];
    fighter->hit = table[ANIM_ENTRY_HIT * ANIM_ENTRY_SIZE];
    if (fighter->idle >= count) {
        fighter->idle = 1; // what the widget plays (the rental card's stance)
    }
    if (fighter->hit >= count) {
        fighter->hit = fighter->idle;
    }

    // Each different animation of the moves that do damage
    fighter->numAttacks = 0;
    for (move = 1; move <= NUM_MOVES && fighter->numAttacks < MAX_ATTACKS; move++) {
        u8 anim = table[(move - 1) * ANIM_ENTRY_SIZE];

        if (D_80072B00[move - 1].unk_02 == 0 || anim >= count || anim == fighter->idle || anim == fighter->hit) {
            continue;
        }
        for (i = 0; i < fighter->numAttacks; i++) {
            if (fighter->attacks[i] == anim) {
                break;
            }
        }
        if (i == fighter->numAttacks) {
            fighter->attacks[fighter->numAttacks++] = anim;
        }
    }
}

// Starts one of a side's animations from its first frame
static void Randomizer_FaceoffPlay(s32 side, s32 anim, s32 busy) {
    RandomizerFighter* fighter = &sFighters[side];
    unk_D_86002F58_004_000* model = Randomizer_FaceoffModel(side);

    func_8001BD04(model, anim);
    func_80017464(model, 0);
    fighter->busy = busy;
    fighter->lastFrame = -1;
}

// The frame a side's animation is on
static s32 Randomizer_FaceoffFrame(s32 side) {
    s32 frame = Randomizer_FaceoffModel(side)->unk_040.unk_08 >> 16;

    return (frame < 0) ? 0 : frame;
}

// How far through its animation a side is, in percent
static s32 Randomizer_FaceoffPercent(s32 side) {
    s32 frames = Randomizer_FaceoffModel(side)->unk_040.unk_04->unk_0A;

    if (frames <= 1) {
        return 100;
    }
    return (Randomizer_FaceoffFrame(side) * 100) / (frames - 1);
}

/*
 * Whether a side's attack or hit reaction has played through: it's on its last frame
 * (func_80017514), or it has gone back to an earlier one, for an animation that loops instead
 * of holding its last frame. Then it goes back to its idle stance.
 */
static s32 Randomizer_FaceoffDone(s32 side) {
    RandomizerFighter* fighter = &sFighters[side];
    s32 frame;

    if (!fighter->busy) {
        return 1;
    }
    frame = Randomizer_FaceoffFrame(side);
    if (func_80017514(Randomizer_FaceoffModel(side)) || (frame < fighter->lastFrame)) {
        Randomizer_FaceoffPlay(side, fighter->idle, 0);
        return 1;
    }
    fighter->lastFrame = frame;
    return 0;
}

// The turns, every frame
static void Randomizer_FaceoffUpdate(void) {
    s32 defender = sAttacker ^ 1;
    RandomizerFighter* attacker = &sFighters[sAttacker];
    s32 attackerDone;
    s32 defenderDone;

    switch (sPhase) {
        case PHASE_PAUSE:
            if (--sTimer > 0) {
                break;
            }
            if (attacker->numAttacks == 0) {
                // Nothing to attack with: the other's turn
                sAttacker = defender;
                sTimer = PAUSE_FRAMES;
                break;
            }
            Randomizer_FaceoffPlay(sAttacker, attacker->attacks[Randomizer_FaceoffBelow(attacker->numAttacks)], 1);
            sHitStarted = 0;
            sTimer = TURN_MAX_FRAMES;
            sPhase = PHASE_ATTACK;
            break;

        case PHASE_ATTACK:
            if (!sHitStarted && (!attacker->busy || (Randomizer_FaceoffPercent(sAttacker) >= HIT_AT_PERCENT))) {
                Randomizer_FaceoffPlay(defender, sFighters[defender].hit, 1);
                sHitStarted = 1;
            }
            attackerDone = Randomizer_FaceoffDone(sAttacker);
            defenderDone = Randomizer_FaceoffDone(defender);
            if (sHitStarted && attackerDone && defenderDone) {
                sAttacker = defender;
                sTimer = PAUSE_FRAMES;
                sPhase = PHASE_PAUSE;
            } else if (--sTimer <= 0) {
                Randomizer_FaceoffPlay(sAttacker, attacker->idle, 0);
                Randomizer_FaceoffPlay(defender, sFighters[defender].idle, 0);
                sAttacker = defender;
                sTimer = PAUSE_FRAMES;
                sPhase = PHASE_PAUSE;
            }
            break;
    }
}

/*
 * Draws the title picture's 16x16 tiles (RGBA16, row after row) that cover a w x h area of
 * the screen from (left, top), at (x, y) of whatever is being drawn into, in copy mode
 */
static void Randomizer_FaceoffTiles(s32 x, s32 y, s32 left, s32 top, s32 w, s32 h) {
    s32 col;
    s32 row;

    for (row = 0; row < h; row += TILE) {
        for (col = 0; col < w; col += TILE) {
            s32 tile = (((top + row) / TILE) * (BACKGROUND_W / TILE)) + ((left + col) / TILE);

            func_8001C6AC(x + col, y + row, TILE, TILE, D_82100EB4 + (tile * TILE * TILE * sizeof(u16)), TILE,
                          0x200000);
        }
    }
}

/*
 * Renders a side into the canvas, as func_8001B9D4 renders a widget into its picture: the
 * first side drawn puts the patch of the title picture behind the canvas in it (where the
 * widget would draw its backdrop), the second draws on top of the first
 */
static void Randomizer_FaceoffRender(s32 side, s32 first) {
    unk_func_8001B1FC* widget = sFighters[side].widget;

    // The species' size, as func_8001B778 sets it
    func_8000E88C(&widget->unk_24->unk_030, widget->unk_18, widget->unk_18, widget->unk_18);

    func_80006498(&gDisplayListHead, sCanvas);
    if (first) {
        gDPPipeSync(gDisplayListHead++);
        gDPSetCycleType(gDisplayListHead++, G_CYC_COPY);
        gDPSetRenderMode(gDisplayListHead++, G_RM_NOOP, G_RM_NOOP2);
        gDPSetTexturePersp(gDisplayListHead++, G_TP_NONE);
        Randomizer_FaceoffTiles(0, 0, CANVAS_X, CANVAS_Y, CANVAS_W, CANVAS_H);
        gDPPipeSync(gDisplayListHead++);
        gDPSetTexturePersp(gDisplayListHead++, G_TP_PERSP);
    }
    func_800067E4(&gDisplayListHead, 0, 0, CANVAS_W, CANVAS_H); // clears the depth
    func_80015348();
    func_80015094(widget->unk_20);
}

// A widget drawing into the canvas: func_8001B1FC's, without a picture of its own, and with
// its camera moved (see CAMERA_FOVY)
static unk_func_8001B1FC* Randomizer_FaceoffWidget(void) {
    unk_func_8001B1FC* widget = main_pool_alloc(sizeof(unk_func_8001B1FC), 0);
    unk_D_86002F34_00C* camera;
    Vec3f* eye;
    f32 scale;

    widget->unk_00 = 0; // not turned by the D-pad, and not spinning by itself
    widget->unk_1C = func_80019760(1);
    widget->unk_14 = 0;
    widget->unk_04 = CANVAS_W;
    widget->unk_06 = CANVAS_H;
    widget->unk_08 = CANVAS_W;
    widget->unk_0A = CANVAS_H;
    widget->unk_0C = 0;
    widget->unk_28 = sCanvas;
    widget->unk_2C = sCanvas->depth_p;
    func_8001B154(widget, 0, 0, CANVAS_W, CANVAS_H);

    // Looking at (0, 0, 0) from eye
    camera = widget->unk_20->unk_0C;
    eye = &camera->unk_60.eye;
    scale = CAMERA_DISTANCE / sqrtf(SQ(eye->x) + SQ(eye->y) + SQ(eye->z));
    eye->z = sqrtf(SQ(eye->x) + SQ(eye->z)) * scale;
    eye->y = (eye->y * scale) + CAMERA_RAISE;
    eye->x = 0.0f;
    camera->unk_60.at.y += CAMERA_RAISE;
    func_80011E68(camera, CAMERA_FOVY, 100.0f, 12800.0f);
    return widget;
}

// When the title screen starts, once its picture is loaded
void Randomizer_FaceoffStart(void) {
    s32 species[SIDES];
    u8* table;
    s32 i;

    sReady = 0;
    sRandomState = Randomizer_FaceoffSeed();
    species[0] = Randomizer_FaceoffBelow(NUM_SPECIES) + 1;
    species[1] = Randomizer_FaceoffBelow(NUM_SPECIES - 1) + 1;
    if (species[1] >= species[0]) {
        species[1]++; // never the same one twice
    }
#ifdef FACEOFF_SPECIES_L
    // For testing: a given pair
    species[0] = FACEOFF_SPECIES_L;
    species[1] = FACEOFF_SPECIES_R;
#endif

    // What screens with 3D Pokemon load: fragment31, which the models' code calls into
    // (rom.yaml has it as the Transfer Pak checker, but the intro loads it too), and the
    // models' archives and work memory
    FRAGMENT_LOAD(fragment31);
    func_8001987C();

    // The canvas and its depth, as func_8001B1FC makes a widget's picture
    sCanvas = func_80006314(0, 2, CANVAS_W, CANVAS_H, 0);
    func_80006414(sCanvas, func_80006314(0, 2, CANVAS_W, CANVAS_H, 1));

    // Kept: the pool frees only its newest blocks, and the widgets come after it
    table = main_pool_alloc(ANIM_ENTRIES * ANIM_ENTRY_SIZE, 0);
    for (i = 0; i < SIDES; i++) {
        unk_func_8001B1FC* widget = Randomizer_FaceoffWidget();

        sFighters[i].widget = widget;
        func_8001B480(widget, species[i], 0);
        widget->unk_24->unk_01E.y = sFighterAngle[i];
        // The model's position is turned with it (func_8000F3FC), so this is what moves it
        // straight to the side
        widget->unk_24->unk_024.x += sFighterX[i] * COSS(sFighterAngle[i]);
        widget->unk_24->unk_024.z -= sFighterX[i] * SINS(sFighterAngle[i]);

        Randomizer_FaceoffAnimations(i, species[i], table);
        Randomizer_FaceoffPlay(i, sFighters[i].idle, 0);
    }

    sAttacker = Randomizer_FaceoffBelow(SIDES);
    sTimer = PAUSE_FRAMES;
    sPhase = PHASE_PAUSE;
    sReady = 1;

#ifdef FACEOFF_TEST
    // The pair and their animations, in gRandomizerState's free bytes (0x80000358), for
    // reading from an emulator savestate
    for (i = 0; i < SIDES; i++) {
        u8* out = (u8*)0x80000358 + (i * 0x10);
        s32 j;

        out[0] = species[i];
        out[1] = Randomizer_FaceoffAnimCount(i);
        out[2] = sFighters[i].idle;
        out[3] = sFighters[i].hit;
        out[4] = sFighters[i].numAttacks;
        for (j = 0; j < MAX_ATTACKS; j++) {
            out[5 + j] = sFighters[i].attacks[j];
        }
    }
#endif
}

// Every frame, with the title picture drawn and before what goes on it
void Randomizer_FaceoffDraw(void) {
    s32 row;

    if (!sReady) {
        return;
    }
    Randomizer_FaceoffUpdate();

    // The attacker last, so that it's in front where they meet; drawing into the canvas
    // switches the drawing there, so the screen is put back after (as the frame starts)
    Randomizer_FaceoffRender(sAttacker ^ 1, TRUE);
    Randomizer_FaceoffRender(sAttacker, FALSE);
    func_800079C4();

    gSPDisplayList(gDisplayListHead++, D_8006F4E0);
    gDPSetAlphaCompare(gDisplayListHead++, G_AC_NONE);

    // Graphics plugins that don't emulate drawing into a picture (high-level ones such as
    // Glide64) draw the models onto the screen's top left corner instead: the title picture
    // is drawn there again over them (the same pixels where the drawing into pictures works)
    Randomizer_FaceoffTiles(0, 0, 0, 0, CANVAS_W, CANVAS_H);

    for (row = 0; row < CANVAS_H; row += CANVAS_ROWS) {
        func_8001C6AC(CANVAS_X, CANVAS_Y + row, CANVAS_W, CANVAS_ROWS,
                      sCanvas->img_p + (row * CANVAS_W * sizeof(u16)), CANVAS_W, 0x200000);
    }
    gSPDisplayList(gDisplayListHead++, D_8006F630);
}

#endif
