/*
 * The title screen's 3D scene (randomizer_title, see randomizer_title.h): one of the
 * battle arenas, picked at random every time the title starts, with two random Pokemon
 * facing each other where the battle puts them and taking turns attacking, filmed the way
 * a battle is, under the logo and "PRESS START".
 *
 * The scene is built the way the battle builds its own (func_84301430 in fragment62),
 * from main code only:
 *  - the arena is a file of the stadium_models archive, a fragment whose entry gives its
 *    parts: three layers of geometry (0, 1 and 3), the sky behind them (2: a 4x64 RGBA32
 *    gradient, or a colour) and its fog (4);
 *  - the scene's graph is the battle's (D_84384364) with this file's camera, fog, layers
 *    and lists of models (the battle's are D_800AC840 and D_800AC858, main code's; this
 *    file has its own, since another screen's models may still be in those): the camera,
 *    two lights that follow it, the layers, and the models;
 *  - each Pokemon is a model in a list of its own, loaded the way the rental card loads one
 *    (func_8001B480), and placed and turned the way the battle places its own
 *    (func_84306C2C and func_84307C5C).
 * Then every frame: the sky, the depth cleared, and the scene drawn (func_84300E88), twice
 * for a split screen, once in each half.
 *
 * The attacks are the battle's animations. Every species has a table in the ROM (at
 * _70D3A0_ROM_START plus D_80075BD0's offset for it, 0xBC0 bytes, as func_84302658 in
 * fragment62 loads it) of 0x10-byte entries whose first byte is the model's animation: one
 * per move, at the move's id - 1, then the idle stance (165) and the reaction to a hit
 * (168), among others. A turn plays the animation of one of the attacker's damaging moves
 * with its cry, then, partway through it, the defender's hit reaction, then both stand in
 * their idle stance for a moment, and the other attacks.
 *
 * The camera cuts to a new shot, picked at random, every few seconds (see SHOT_*).
 *
 * Two things the battle has that the title doesn't: fragment34 loaded before fragment31
 * (the arenas call into fragment31's table, which jumps on into fragment34), and room on
 * the stack: the setup and the drawing run on a stack of their own
 * (randomizer_title_stack.s).
 *
 * Built only with RANDOMIZER=1; empty otherwise so the default build still matches.
 */
#include "randomizer_title.h"

#ifdef RANDOMIZER

#include "src/11BA0.h"
#include "src/12D80.h"
#include "src/17300.h"
#include "src/19840.h"
#include "src/1C720.h"
#include "src/20470.h"
#include "src/2E460.h"
#include "src/3FB0.h"
#include "src/4F410.h"
#include "src/6BC0.h"
#include "src/F420.h"
#include "src/geo_layout.h"
#include "src/memmap.h"
#include "src/memory.h"
#include "src/randomizer_save.h"
#include "src/stage_loader.h"

#define SCREEN_W 320
#define SCREEN_H 240

#define SIDES 2
#define NUM_SPECIES 151
#define NUM_MOVES 165
#define NUM_ARENAS 18 // the files of stadium_models

// A species' battle animation table (see above)
#define ANIM_ENTRY_SIZE 0x10
#define ANIM_ENTRIES 188
#define ANIM_TABLE_SIZE (ANIM_ENTRIES * ANIM_ENTRY_SIZE)
#define ANIM_ENTRY_IDLE 165
#define ANIM_ENTRY_HIT 168
#define MAX_ATTACKS 8

// A species' place in the battle (D_84390028, 0x10 bytes from D_70110, as func_84302658 loads
// it): its size (f32 at 0x00) and how high it floats (f32 at 0x08)
#define PLACE_ENTRY_SIZE 0x10

#define PAUSE_FRAMES 45     // in the idle stance between turns
#define HIT_AT_PERCENT 45   // how far into the attack the defender reacts
#define TURN_MAX_FRAMES 300 // a turn ends by then even if an animation never does
#define CRY_MODE 2          // func_8004E810's, as the Pokedex plays a cry

/*
 * The size of a model: the rental card shows each species scaled by D_8006FF00's unk_02 /
 * 100 to about the same height, about 536 units, so a model is about 536 / that scale
 * units tall.
 */
#define CARD_HEIGHT 536.0f

/*
 * The camera's shots. The logo covers the top half of the screen, so the camera looks at a
 * point above what it films, by LOOK_RAISE of its distance (about 11 degrees), which puts
 * that about 50 pixels under the middle of the screen.
 */
#define CAMERA_FOVY 50.0f
#define CAMERA_NEAR 10.0f
#define CAMERA_FAR 12800.0f // the battle's
#define LOOK_RAISE 0.2f

enum {
    SHOT_PAN,   // round the whole battlefield, slowly, from far
    SHOT_CLOSE, // close on one Pokemon, from in front of it
    SHOT_SPLIT, // the screen split in two, each half close on one Pokemon
    SHOT_ORBIT, // going round one Pokemon, from a little farther
    SHOT_CHASE, // from behind one Pokemon, moving up to the other
    NUM_SHOTS,
};

#define PAN_FRAMING 1.05f   // distance / the width of the field
#define PAN_PITCH 0x700     // looking down 10 degrees
#define PAN_TURN 0x50       // a quarter turn in about 7 seconds
#define CLOSE_FRAMING 2.6f  // distance / height
#define CLOSE_PITCH 0x300
#define CLOSE_TURN 0x0C
#define SPLIT_FRAMING 3.0f  // distance / height, in a half as wide
#define ORBIT_FRAMING 3.6f  // distance / height
#define ORBIT_PITCH 0x500
#define ORBIT_TURN 0xC0     // a full turn in about 11 seconds
#define CHASE_MOVE 0.8f     // how much of the shot the camera moves for
#define MIN_DISTANCE 90.0f  // from a Pokemon, for the smallest ones

static const s16 sShotLengths[NUM_SHOTS] = { 240, 150, 150, 240, 180 }; // frames (30 a second)

// Its own stack, for the setup and the drawing (randomizer_title_stack.s)
#define STACK_SIZE 0x4000
void Randomizer_CallOnStack(void (*func)(void), void* stackTop);

typedef struct RandomizerArenaSide {
    /* 0x00 */ unk_D_86002F58_004_000* model;
    /* 0x04 */ Vec3f middle; // in the arena
    /* 0x10 */ f32 height;
    /* 0x14 */ f32 width;
    /* 0x18 */ s16 species;
    /* 0x1A */ s16 lastFrame; // of the attack or hit reaction playing, to see it end
    /* 0x1C */ u8 idle;
    /* 0x1D */ u8 hit;
    /* 0x1E */ u8 busy; // playing an attack or the hit reaction
    /* 0x1F */ u8 numAttacks;
    /* 0x20 */ u8 attacks[MAX_ATTACKS];
} RandomizerArenaSide; // size = 0x28

enum {
    PHASE_PAUSE,
    PHASE_ATTACK,
};

static unk_D_86002F34_00C sCamera;
static unk_D_8690A610 sFog;
static GraphNode sLayers[3];
static GraphNode sModelLists[SIDES]; // the scene's own, in place of D_800AC840 and D_800AC858
static GraphNode* sScene;
static u8* sStackTop;
static u8* sTable; // a species' animation table or place, read from the ROM
static void* sSky;
static RandomizerArenaSide sSides[SIDES];
static s32 sArena;
static u32 sRandomState;
static u8 sReady;

static u8 sPhase;
static u8 sAttacker;
static u8 sHitStarted;
static s16 sTimer;

static u8 sShot;
static u8 sShotSide;
static s16 sShotFrame;
static s16 sShotYaw;
static s16 sShotTurn;

static s32 Randomizer_ArenaLight1(s32 arg0, unk_D_86002F34_alt18* arg1);
static s32 Randomizer_ArenaLight2(s32 arg0, unk_D_86002F34_alt18* arg1);
static s32 Randomizer_ArenaLayer(s32 arg0, unk_D_86002F58_004_000* arg1);
static s32 Randomizer_ArenaCamera(s32 arg0, GraphNode* arg1);

// The battle's scene (D_84384364), with this file's nodes and callbacks
static u32 sSceneLayout[] = {
    0x0C00FFFF, 0x05000000, 0x07000000, (u32)&sCamera, 0x05000000, 0x0D000000, 0x05000000, 0x07000000,
    (u32)&sFog, 0x14000000, 0x002D0019, 0xFFFFFF28, 0x08000000, (u32)Randomizer_ArenaLight1, 0x00000000, 0x14000000,
    0x002D0019, 0x80808028, 0x08000000, (u32)Randomizer_ArenaLight2, 0x00000000, 0x16646464, 0x0F000002, 0x05000000,
    0x1F00FFFF, 0x00000000, 0x00000000, 0x00000000, 0x00640064, 0x00640000, 0x08000000, (u32)Randomizer_ArenaLayer,
    0x00000000, 0x05000000, 0x07000000, (u32)&sLayers[0], 0x06000000, 0x06000000, 0x0F000003, 0x05000000,
    0x1F00FFFF, 0x00000000, 0x00000000, 0x00000000, 0x00640064, 0x00640000, 0x08000000, (u32)Randomizer_ArenaLayer,
    0x00000000, 0x05000000, 0x07000000, (u32)&sLayers[1], 0x06000000, 0x06000000, 0x0F000002, 0x05000000,
    0x1F00FFFF, 0x00000000, 0x00000000, 0x00000000, 0x00640064, 0x00640000, 0x08000000, (u32)Randomizer_ArenaLayer,
    0x00000000, 0x05000000, 0x07000000, (u32)&sLayers[2], 0x06000000, 0x06000000, 0x0F000003, 0x05000000,
    0x0A000000, (u32)&sModelLists[0], 0x06000000, 0x0F000002, 0x05000000, 0x0A000000, (u32)&sModelLists[1], 0x06000000,
    0x09000000, 0x08000000, (u32)Randomizer_ArenaCamera, 0x00000000, 0x06000000, 0x06000000, 0x06000000, 0x01000000,
};

// The lights, turned with the camera (func_84300058 and func_843000C0)
static s32 Randomizer_ArenaLight1(s32 arg0, unk_D_86002F34_alt18* arg1) {
    f32 distance;
    s16 pitch;
    s16 yaw;

    if (arg0 == 2) {
        func_800102A4(&D_8006F088->unk_60.at, &D_8006F088->unk_60.eye, &distance, &pitch, &yaw);
        arg1->unk_1C = pitch + 0x2000;
        arg1->unk_1E = yaw - 0x2000;
    }
    return 0;
}

static s32 Randomizer_ArenaLight2(s32 arg0, unk_D_86002F34_alt18* arg1) {
    f32 distance;
    s16 pitch;
    s16 yaw;

    if (arg0 == 2) {
        func_800102A4(&D_8006F088->unk_60.at, &D_8006F088->unk_60.eye, &distance, &pitch, &yaw);
        arg1->unk_1C = -0x2000 - pitch;
        arg1->unk_1E = yaw + 0x6000;
    }
    return 0;
}

// The arena's layers at their size and colour (func_8430012C, without the battle's fades)
static s32 Randomizer_ArenaLayer(s32 arg0, unk_D_86002F58_004_000* arg1) {
    if (arg0 == 2) {
        func_8000E88C(&arg1->unk_030, 1.0f, 1.0f, 1.0f);
        arg1->unk_03C.rgba = 0xFFFFFF00;
    }
    return 0;
}

// Where the battle moves its camera (func_84300020); this file moves it before drawing
static s32 Randomizer_ArenaCamera(UNUSED s32 arg0, UNUSED GraphNode* arg1) {
    return 0;
}

// Mixes a number's bits thoroughly (a 32-bit hash finaliser, as in randomizer_intro.c)
static u32 Randomizer_ArenaMix(u32 x) {
    x ^= x >> 16;
    x *= 0x7FEB352D;
    x ^= x >> 15;
    x *= 0x846CA68B;
    x ^= x >> 16;
    return x;
}

// A number from 0 to n - 1 (the team generator's 32-bit LCG; its fragment isn't loaded here)
static s32 Randomizer_ArenaBelow(s32 n) {
    sRandomState = (sRandomState * 0x19660D) + 0x3C6EF35F;
    return ((u64)sRandomState * (u32)n) >> 32;
}

// Either way round: 1 or -1
static s32 Randomizer_ArenaSign(void) {
    return Randomizer_ArenaBelow(2) ? 1 : -1;
}

/*
 * The seed: the CPU's cycle count, which an emulator repeats from boot to boot if nothing is
 * pressed, so mixed with the intro's count of boots in the save file (src/randomizer_save.h),
 * which the title has loaded. Only read: the intro counts the boots.
 */
static u32 Randomizer_ArenaSeed(void) {
    u32 boots = 0;

    if (func_80028AFC(RANDOMIZER_SAVE_BANK)) {
        bcopy(RANDOMIZER_SAVE_AT(RANDOMIZER_SAVE_BOOT_COUNT + 4), &boots, sizeof(boots));
    }
    return Randomizer_ArenaMix(osGetCount() ^ Randomizer_ArenaMix(boots + 0x5EED));
}

// Reads size bytes of the ROM, from start, into sTable
static void Randomizer_ArenaRead(u32 start, s32 size) {
    osInvalDCache(sTable, size);
    func_80003B30((u32)sTable, start, start + size, 0);
}

// The arena: its layers in the scene, its sky and its fog (as func_84301430 sets them)
static void Randomizer_ArenaLoad(s32 arena) {
    FragmentEntry parts = func_8000484C(ASSET_LOAD2(stadium_models, 1, 1), arena);
    static const s32 sLayerParts[] = { 0, 1, 3 };
    unk_D_8690A610_018* fog;
    MemoryBlock* block;
    s32 i;

    func_800110E0(NULL, &sCamera, 0, 0, SCREEN_W, SCREEN_H);
    func_800113F8(0, &sFog, 0x3C0, 0x3E8, 0xFF, 0xFF, 0xFF, 0xFF);
    for (i = 0; i < ARRAY_COUNT(sLayers); i++) {
        func_8001103C(NULL, &sLayers[i]);
    }
    for (i = 0; i < ARRAY_COUNT(sModelLists); i++) {
        func_8001103C(NULL, &sModelLists[i]);
    }

    block = func_80002D10(main_pool_get_available(), 0);
    sScene = process_geo_layout(block, sSceneLayout);
    for (i = 0; i < ARRAY_COUNT(sLayerParts); i++) {
        void* layout = (void*)parts(sLayerParts[i], 0);

        if (layout != NULL) {
            func_80012094(&sLayers[i], process_geo_layout(block, layout));
        }
    }
    func_80002D60(block);

    sSky = (void*)parts(2, 0);
    fog = (unk_D_8690A610_018*)parts(4, 0);
    if (fog == NULL) {
        sFog.unk_00.unk_14 = 0;
        sFog.unk_00.unk_01 &= ~1;
    } else {
        sFog.unk_18.unk_00 = fog->unk_00;
        sFog.unk_18.unk_02 = fog->unk_02;
        sFog.unk_18.unk_04.rgba = fog->unk_04.rgba;
        sFog.unk_00.unk_14 = 1;
        sFog.unk_00.unk_01 |= 1; // as func_84300184 turns it on at full colour
    }
}

// A side's idle stance, hit reaction and attacks, from its species' battle animation table
static void Randomizer_ArenaAnimations(RandomizerArenaSide* side) {
    s32 count = side->model->unk_000.unk_0C->unk_28(0, 0)->unk_04; // how many the model has
    s32 move;
    s32 i;

    Randomizer_ArenaRead((u32)_70D3A0_ROM_START + ((u32)D_80075BD0[side->species - 1] & 0xFFFFFF), ANIM_TABLE_SIZE);

    side->idle = sTable[ANIM_ENTRY_IDLE * ANIM_ENTRY_SIZE];
    side->hit = sTable[ANIM_ENTRY_HIT * ANIM_ENTRY_SIZE];
    if (side->idle >= count) {
        side->idle = 0;
    }
    if (side->hit >= count) {
        side->hit = side->idle;
    }

    // Each different animation of the moves that do damage
    side->numAttacks = 0;
    for (move = 1; move <= NUM_MOVES && side->numAttacks < MAX_ATTACKS; move++) {
        u8 anim = sTable[(move - 1) * ANIM_ENTRY_SIZE];

        if (D_80072B00[move - 1].unk_02 == 0 || anim >= count || anim == side->idle || anim == side->hit) {
            continue;
        }
        for (i = 0; i < side->numAttacks; i++) {
            if (side->attacks[i] == anim) {
                break;
            }
        }
        if (i == side->numAttacks) {
            side->attacks[side->numAttacks++] = anim;
        }
    }
}

// Starts one of a side's animations from its first frame
static void Randomizer_ArenaPlay(RandomizerArenaSide* side, s32 anim, s32 busy) {
    func_8001BD04(side->model, anim);
    func_80017464(side->model, 0);
    side->busy = busy;
    side->lastFrame = -1;
}

/*
 * A side's Pokemon, where the battle puts it (func_84307C5C): on the left facing right, or on
 * the right facing left, farther from the middle for the biggest, floating if it flies; added
 * to a list of models as func_8001BB58 adds one to D_800AC840, and loaded as func_8001B480
 * loads the rental card's. Each in a list of its own: the scene draws each list as a group
 * that starts with the drawing's state reset (func_8001638C), and the flames of Charizard and
 * others (func_80032F94) leave some of it changed, which turns the Pokemon drawn after them
 * in the same group pink.
 */
static void Randomizer_ArenaPokemon(s32 index) {
    RandomizerArenaSide* side = &sSides[index];
    s32 species = side->species;
    unk_D_86002F58_004_000_010* models = func_80019760(1);
    unk_D_86002F58_004_000* model = main_pool_alloc(sizeof(unk_D_86002F58_004_000), 0);
    unk_D_8006FF00* info = &D_8006FF00[species - 1];
    f32 cardScale = info->unk_02 / 100.0f;
    arg1_func_80010CA8 variant;
    f32 x;
    f32 y;

    side->model = model;
    func_80011938(NULL, model, 0, &D_8006F050, &D_8006F05C, &D_8006F064);
    func_80012094(&sModelLists[index], (GraphNode*)&model->unk_000);
    model->unk_0A6 = index; // its side, for the effects some species have (func_8003260C)
    model->unk_000.unk_01 &= ~1;

    variant.raw = 0;
    func_800198E4(models, species, variant);
    func_80019CA8(models);
    func_8001BCF0(model);
    func_8001BC34(model, 0, species, models->unk_24->unk_08->unk_00[0]);

    // Its shadow on the ground, but for Diglett and Dugtrio, as func_84306C2C sets it
    if ((species == 50) || (species == 51)) {
        model->unk_000.unk_02 &= ~0x40;
    } else {
        model->unk_000.unk_02 |= 0x40;
    }

    Randomizer_ArenaRead((u32)_70D3A0_ROM_START + ((u32)(D_70110 + ((species - 1) * PLACE_ENTRY_SIZE)) & 0xFFFFFF),
                         PLACE_ENTRY_SIZE);
    side->width = ((f32*)sTable)[0];
    y = ((f32*)sTable)[2];
    if ((species == 95) || (species == 130)) {
        x = 225.0f; // Onix and Gyarados
    } else if ((species == 3) || (species == 131)) {
        x = 175.0f; // Venusaur and Lapras
    } else {
        x = 150.0f;
    }
    if (index == 0) {
        x = -x;
    }
    func_8000E88C(&model->unk_024, x, y, 0.0f);
    model->unk_01E.y = (index == 0) ? 0x4000 : -0x4000;
    func_8000E88C(&model->unk_030, 1.0f, 1.0f, 1.0f); // the battle's size

    // Its size and middle, from the card's (the card's pivot is the middle of the model, scaled,
    // as func_8001B480 reads it)
    side->height = CARD_HEIGHT / cardScale;
    func_8000E88C(&side->middle, x, y - ((f32)((s16)(info->unk_14 >> 6) >> 4) / cardScale), 0.0f);

    Randomizer_ArenaAnimations(side);
    Randomizer_ArenaPlay(side, side->idle, 0);
}

// The arena and the Pokemon, on the scene's own stack
static void Randomizer_ArenaSetup(void) {
    s32 i;

    // What screens with 3D Pokemon load: fragment31, which the models' and the arenas' code
    // calls into, and the models' archives and work memory. And fragment34 before it:
    // fragment31's table jumps on into it (the arenas' callback 0x810001D0 goes to
    // 0x81407874), and a jump into another fragment is only relocated if that one is loaded
    FRAGMENT_LOAD(fragment34);
    FRAGMENT_LOAD(fragment31);
    func_8001987C();

    Randomizer_ArenaLoad(sArena);
    sTable = main_pool_alloc(ANIM_TABLE_SIZE, 0);
    for (i = 0; i < SIDES; i++) {
        Randomizer_ArenaPokemon(i);
    }
}

// A new shot, never the same kind twice in a row
static void Randomizer_ArenaCut(void) {
    s32 shot = Randomizer_ArenaBelow(NUM_SHOTS - 1);

    if (shot >= sShot) {
        shot++;
    }
#ifdef ARENA_TEST_SHOT
    shot = ARENA_TEST_SHOT;
#endif
    sShot = shot;
    sShotSide = Randomizer_ArenaBelow(SIDES);
    sShotFrame = 0;
    sShotYaw = Randomizer_ArenaBelow(0x10000);
    sShotTurn = Randomizer_ArenaSign();
}

// When the title screen starts
void Randomizer_ArenaStart(void) {
    sReady = 0;
    sRandomState = Randomizer_ArenaSeed();
    sArena = Randomizer_ArenaBelow(NUM_ARENAS);
    sSides[0].species = Randomizer_ArenaBelow(NUM_SPECIES) + 1;
    sSides[1].species = Randomizer_ArenaBelow(NUM_SPECIES - 1) + 1;
    if (sSides[1].species >= sSides[0].species) {
        sSides[1].species++; // never the same one twice
    }
#ifdef ARENA_TEST_ARENA
    sArena = ARENA_TEST_ARENA;
#endif
#ifdef ARENA_TEST_SPECIES
    sSides[0].species = ARENA_TEST_SPECIES;
    sSides[1].species = ARENA_TEST_SPECIES2;
#endif

    sStackTop = (u8*)main_pool_alloc(STACK_SIZE, 0) + STACK_SIZE;
    Randomizer_CallOnStack(Randomizer_ArenaSetup, sStackTop);

    sAttacker = Randomizer_ArenaBelow(SIDES);
    sTimer = PAUSE_FRAMES;
    sPhase = PHASE_PAUSE;
    sShot = NUM_SHOTS;
    Randomizer_ArenaCut();
    sReady = 1;

#ifdef ARENA_TEST
    // What was picked, in gRandomizerState's free bytes (0x80000358), for reading from an
    // emulator savestate
    ((u8*)0x80000358)[0] = sArena;
    ((u8*)0x80000358)[1] = sSides[0].species;
    ((u8*)0x80000358)[2] = sSides[1].species;
    ((u8*)0x80000358)[3] = 0xA5;
    ((u32*)0x80000358)[1] = main_pool_get_available();
#endif
}

// The frame a side's animation is on
static s32 Randomizer_ArenaFrame(RandomizerArenaSide* side) {
    s32 frame = side->model->unk_040.unk_08 >> 16;

    return (frame < 0) ? 0 : frame;
}

// How far through its animation a side is, in percent
static s32 Randomizer_ArenaPercent(RandomizerArenaSide* side) {
    s32 frames = side->model->unk_040.unk_04->unk_0A;

    if (frames <= 1) {
        return 100;
    }
    return (Randomizer_ArenaFrame(side) * 100) / (frames - 1);
}

/*
 * Whether a side's attack or hit reaction has played through: it's on its last frame
 * (func_80017514), or it has gone back to an earlier one, for an animation that loops instead
 * of holding its last frame. Then it goes back to its idle stance.
 */
static s32 Randomizer_ArenaDone(RandomizerArenaSide* side) {
    s32 frame;

    if (!side->busy) {
        return 1;
    }
    frame = Randomizer_ArenaFrame(side);
    if (func_80017514(side->model) || (frame < side->lastFrame)) {
        Randomizer_ArenaPlay(side, side->idle, 0);
        return 1;
    }
    side->lastFrame = frame;
    return 0;
}

// The turns, every frame
static void Randomizer_ArenaTurns(void) {
    RandomizerArenaSide* attacker = &sSides[sAttacker];
    RandomizerArenaSide* defender = &sSides[sAttacker ^ 1];
    s32 attackerDone;
    s32 defenderDone;

    switch (sPhase) {
        case PHASE_PAUSE:
            if (--sTimer > 0) {
                break;
            }
            if (attacker->numAttacks == 0) {
                // Nothing to attack with: the other's turn
                sAttacker ^= 1;
                sTimer = PAUSE_FRAMES;
                break;
            }
            Randomizer_ArenaPlay(attacker, attacker->attacks[Randomizer_ArenaBelow(attacker->numAttacks)], 1);
            func_8004E810(attacker->species, CRY_MODE);
            sHitStarted = 0;
            sTimer = TURN_MAX_FRAMES;
            sPhase = PHASE_ATTACK;
            break;

        case PHASE_ATTACK:
            if (!sHitStarted && (!attacker->busy || (Randomizer_ArenaPercent(attacker) >= HIT_AT_PERCENT))) {
                Randomizer_ArenaPlay(defender, defender->hit, 1);
                sHitStarted = 1;
            }
            attackerDone = Randomizer_ArenaDone(attacker);
            defenderDone = Randomizer_ArenaDone(defender);
            if (sHitStarted && attackerDone && defenderDone) {
                sAttacker ^= 1;
                sTimer = PAUSE_FRAMES;
                sPhase = PHASE_PAUSE;
            } else if (--sTimer <= 0) {
                Randomizer_ArenaPlay(attacker, attacker->idle, 0);
                Randomizer_ArenaPlay(defender, defender->idle, 0);
                sAttacker ^= 1;
                sTimer = PAUSE_FRAMES;
                sPhase = PHASE_PAUSE;
            }
            break;
    }
}

// The point the camera looks at to frame a side's Pokemon from a distance (see LOOK_RAISE)
static void Randomizer_ArenaLookAt(Vec3f* at, RandomizerArenaSide* side, f32 distance) {
    func_8000E88C(at, side->middle.x, side->middle.y + (distance * LOOK_RAISE), side->middle.z);
}

// How far from a side's Pokemon the camera is to show it framing times its height
static f32 Randomizer_ArenaDistance(RandomizerArenaSide* side, f32 framing) {
    f32 distance = side->height * framing;

    return (distance < MIN_DISTANCE) ? MIN_DISTANCE : distance;
}

// How far a point is from the camera
static f32 Randomizer_ArenaAway(Vec3f* eye, Vec3f* point) {
    return sqrtf(SQ(eye->x - point->x) + SQ(eye->y - point->y) + SQ(eye->z - point->z));
}

// Draws the scene from eye, looking at at, into a part of the screen x to x + w wide
static void Randomizer_ArenaView(s32 x, s32 w, Vec3f* at, Vec3f* eye) {
    f32 near = Randomizer_ArenaAway(eye, at) * 0.25f;
    s32 i;

    // The near clipping plane as far as it can be, for the depth's precision, but in front of
    // the Pokemon and the ground: the camera comes close to small Pokemon, and low behind them
    for (i = 0; i < SIDES; i++) {
        if (near > Randomizer_ArenaAway(eye, &sSides[i].middle) * 0.4f) {
            near = Randomizer_ArenaAway(eye, &sSides[i].middle) * 0.4f;
        }
    }
    if (near > eye->y * 0.5f) {
        near = eye->y * 0.5f;
    }
    if (near > 192.0f) {
        near = 192.0f;
    }
    if (near < CAMERA_NEAR) {
        near = CAMERA_NEAR;
    }
    func_80011DAC(&sCamera, x, 0, w, SCREEN_H);
    func_80011E68(&sCamera, CAMERA_FOVY, near, CAMERA_FAR);
    sCamera.unk_60.at = *at;
    sCamera.unk_60.eye = *eye;
    func_80015094(sScene);
}

// Draws the scene from a distance, pitch and yaw away from at
static void Randomizer_ArenaViewFrom(s32 x, s32 w, Vec3f* at, f32 distance, s16 pitch, s16 yaw) {
    Vec3f eye;

    func_80010354(at, &eye, distance, pitch, yaw);
    Randomizer_ArenaView(x, w, at, &eye);
}

// 0 to 1 over the first part of a shot, easing in and out
static f32 Randomizer_ArenaEase(f32 t) {
    if (t > 1.0f) {
        t = 1.0f;
    }
    return t * t * (3.0f - (2.0f * t));
}

// The shot of the moment, drawn
static void Randomizer_ArenaShoot(void) {
    RandomizerArenaSide* side = &sSides[sShotSide];
    RandomizerArenaSide* other = &sSides[sShotSide ^ 1];
    s32 frame = sShotFrame;
    f32 t = (f32)frame / sShotLengths[sShot];
    s16 facing = (sShotSide == 0) ? 0x4000 : -0x4000; // the yaw from which the camera sees its face
    f32 distance;
    f32 start;
    f32 ease;
    f32 dir;
    Vec3f at;
    Vec3f eye;

    switch (sShot) {
        case SHOT_PAN:
            // The middle of the field, as wide as the two Pokemon and the gap between them
            distance = ((sSides[1].middle.x - sSides[0].middle.x) + MAX(sSides[0].height, sSides[1].height)) *
                       PAN_FRAMING;
            func_8000E88C(&at, 0.0f, ((sSides[0].middle.y + sSides[1].middle.y) * 0.5f) + (distance * LOOK_RAISE),
                          0.0f);
            Randomizer_ArenaViewFrom(0, SCREEN_W, &at, distance, PAN_PITCH, sShotYaw + (frame * PAN_TURN * sShotTurn));
            break;

        case SHOT_CLOSE:
            // In front of it, a little to one side, coming slowly closer
            distance = Randomizer_ArenaDistance(side, CLOSE_FRAMING) * (1.1f - (0.15f * t));
            Randomizer_ArenaLookAt(&at, side, distance);
            Randomizer_ArenaViewFrom(0, SCREEN_W, &at, distance, CLOSE_PITCH,
                                     facing + (sShotTurn * 0x1C00) + (frame * CLOSE_TURN * sShotTurn));
            break;

        case SHOT_SPLIT:
            // The one on the left in the left half and the other in the right, each seen from
            // in front and to the side so that they face each other across the middle
            distance = Randomizer_ArenaDistance(&sSides[0], SPLIT_FRAMING);
            Randomizer_ArenaLookAt(&at, &sSides[0], distance);
            Randomizer_ArenaViewFrom(0, SCREEN_W / 2, &at, distance, CLOSE_PITCH, 0x2000 + (frame * CLOSE_TURN));
            distance = Randomizer_ArenaDistance(&sSides[1], SPLIT_FRAMING);
            Randomizer_ArenaLookAt(&at, &sSides[1], distance);
            Randomizer_ArenaViewFrom(SCREEN_W / 2, SCREEN_W / 2, &at, distance, CLOSE_PITCH,
                                     -0x2000 - (frame * CLOSE_TURN));

            // A line down the middle between them
            gDPPipeSync(gDisplayListHead++);
            gDPSetCycleType(gDisplayListHead++, G_CYC_FILL);
            gDPSetRenderMode(gDisplayListHead++, G_RM_NOOP, G_RM_NOOP2);
            gDPSetFillColor(gDisplayListHead++, (GPACK_RGBA5551(0, 0, 0, 1) << 16) | GPACK_RGBA5551(0, 0, 0, 1));
            gDPFillRectangle(gDisplayListHead++, (SCREEN_W / 2) - 2, 0, (SCREEN_W / 2) + 1, SCREEN_H - 1);
            gDPPipeSync(gDisplayListHead++);
            break;

        case SHOT_ORBIT:
            distance = Randomizer_ArenaDistance(side, ORBIT_FRAMING);
            Randomizer_ArenaLookAt(&at, side, distance);
            Randomizer_ArenaViewFrom(0, SCREEN_W, &at, distance, ORBIT_PITCH,
                                     sShotYaw + (frame * ORBIT_TURN * sShotTurn));
            break;

        case SHOT_CHASE:
            // From behind one, over its shoulder, up to in front of the other: along the line
            // between them, from beside the first one to straight in front of the other
            dir = (sShotSide == 0) ? 1.0f : -1.0f; // the way the first one faces
            ease = Randomizer_ArenaEase(t / CHASE_MOVE);
            distance = Randomizer_ArenaDistance(other, CLOSE_FRAMING);
            if (distance > (other->middle.x - side->middle.x) * dir * 0.6f) {
                distance = (other->middle.x - side->middle.x) * dir * 0.6f;
            }
            start = side->middle.x - (dir * (Randomizer_ArenaDistance(side, 1.5f) + 60.0f));
            eye.x = start + (((other->middle.x - (dir * distance)) - start) * ease);
            eye.z = MAX(side->width * 1.2f, side->height * 0.5f) * sShotTurn * (1.0f - (0.7f * ease));
            Randomizer_ArenaLookAt(&at, other, sqrtf(SQ(eye.x - other->middle.x) + SQ(eye.z)));
            start = side->middle.y + (side->height * 0.6f) + 20.0f;
            eye.y = start + (((at.y + (distance * 0.1f)) - start) * ease);
            Randomizer_ArenaView(0, SCREEN_W, &at, &eye);
            break;
    }
}

// The sky behind the arena, and the depth cleared (func_84300340, at full colour)
static void Randomizer_ArenaSky(void) {
    if (sSky == NULL) {
        func_800067E4(&gDisplayListHead, 0, 0, SCREEN_W, SCREEN_H);
        return;
    }
    if ((u32)sSky == (u32)-1) {
        func_8000699C(&gDisplayListHead, 1);
        return;
    }
    if ((u32)sSky < 0x10000) {
        func_8000699C(&gDisplayListHead, (u32)sSky);
        return;
    }

    func_800067E4(&gDisplayListHead, 0, 0, SCREEN_W, SCREEN_H);
    gDPSetCycleType(gDisplayListHead++, G_CYC_1CYCLE);
    gDPSetTexturePersp(gDisplayListHead++, G_TP_NONE);
    gDPSetCombineMode(gDisplayListHead++, G_CC_MODULATEI_PRIM, G_CC_MODULATEI_PRIM);
    gDPSetPrimColor(gDisplayListHead++, 0, 0, 255, 255, 255, 255);
    gSPClearGeometryMode(gDisplayListHead++, G_ZBUFFER | G_LIGHTING);
    gDPLoadTextureBlock(gDisplayListHead++, Memmap_GetFragmentVaddr(sSky), G_IM_FMT_RGBA, G_IM_SIZ_32b, 4, 64, 0,
                        G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                        G_TX_NOLOD);
    // The top half stretched from the gradient's first row down, the bottom half its last row
    gSPTextureRectangle(gDisplayListHead++, 0, 0, SCREEN_W << 2, (SCREEN_H / 2) << 2, G_TX_RENDERTILE, 0, 0, 0,
                        0x10000 / (SCREEN_H / 2));
    gSPTextureRectangle(gDisplayListHead++, 0, (SCREEN_H / 2) << 2, SCREEN_W << 2, SCREEN_H << 2, G_TX_RENDERTILE, 0,
                        0x07E0, 0, 0);
    gSPDisplayList(gDisplayListHead++, D_8006F630);
}

// The scene's drawing, on its own stack
static void Randomizer_ArenaRender(void) {
    Randomizer_ArenaTurns();
    if (++sShotFrame >= sShotLengths[sShot]) {
        Randomizer_ArenaCut();
    }

    // The models' animations move on a frame (func_80015348), once even when the scene is
    // drawn twice
    func_80015348();
    func_800079C4();
    Randomizer_ArenaSky();
    Randomizer_ArenaShoot();
    gSPDisplayList(gDisplayListHead++, D_8006F630);
}

// Every frame, over the title picture and under the logo and "PRESS START"
void Randomizer_ArenaDraw(void) {
    if (sReady == 1) {
        Randomizer_CallOnStack(Randomizer_ArenaRender, sStackTop);
    }
}

#endif
