/*
 * The title screen's 3D scene (randomizer_title, see randomizer_title.h): one of the
 * battle arenas, picked at random every time the title starts, with a random Pokemon in
 * the middle of it and the camera going round it, under the logo and "PRESS START".
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
 *  - the Pokemon is a model added to the first list, loaded the way the rental card loads
 *    one (func_8001B480).
 * Then every frame: the sky, the depth cleared, and the scene drawn (func_84300E88).
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
#include "src/6BC0.h"
#include "src/F420.h"
#include "src/geo_layout.h"
#include "src/memmap.h"
#include "src/memory.h"
#include "src/randomizer_save.h"
#include "src/stage_loader.h"

#define SCREEN_W 320
#define SCREEN_H 240

#define NUM_SPECIES 151
#define NUM_ARENAS 18 // the files of stadium_models

// A species' battle animation table (_70D3A0_ROM_START + D_80075BD0's offset; game-data.md)
#define ANIM_ENTRY_SIZE 0x10
#define ANIM_ENTRIES 188
#define ANIM_ENTRY_IDLE 165

/*
 * The camera: going round the Pokemon, a little above it, at a distance that frames it at
 * about 40% of the screen's height. The size comes from the rental card's: the card shows
 * each species scaled by D_8006FF00's unk_02 / 100 to about the same height, about 536
 * units, so a model is about 536 / that scale units tall.
 */
#define CAMERA_FOVY 50.0f
#define CARD_HEIGHT 536.0f
#define CAMERA_FRAMING 2.68f // distance / height: the height at 40% of the view (tan 25 = 0.466)
#define CAMERA_PITCH 0x600   // looking down a little
#define CAMERA_TURN 0x30     // a turn every 1365 frames (about 45 seconds)

#define POKEMON_SCALE 1.0f // the battle's

// Its own stack, for the setup and the drawing (randomizer_title_stack.s)
#define STACK_SIZE 0x4000
void Randomizer_CallOnStack(void (*func)(void), void* stackTop);

static unk_D_86002F34_00C sCamera;
static unk_D_8690A610 sFog;
static GraphNode sLayers[3];
static GraphNode sModelLists[2]; // the scene's own, in place of D_800AC840 and D_800AC858
static GraphNode* sScene;
static u8* sStackTop;
static s32 sArena;
static s32 sSpecies;
static void* sSky;
static unk_D_86002F58_004_000* sPokemon;
static s16 sYaw;
static f32 sDistance;
static f32 sLookHeight;
static u8 sReady;
static u32 sRandomState;

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

// The idle stance's animation, from the species' battle animation table
static s32 Randomizer_ArenaIdle(s32 species) {
    static u64 sEntry[ANIM_ENTRY_SIZE / sizeof(u64)]; // DMA needs 8-byte alignment
    u8* table = (u8*)sEntry;
    u32 start = (u32)_70D3A0_ROM_START + ((u32)D_80075BD0[species - 1] & 0xFFFFFF) + (ANIM_ENTRY_IDLE * ANIM_ENTRY_SIZE);
    s32 count = sPokemon->unk_000.unk_0C->unk_28(0, 0)->unk_04;

    osInvalDCache(table, ANIM_ENTRY_SIZE);
    func_80003B30((u32)table, start, start + ANIM_ENTRY_SIZE, 0);
    return (table[0] < count) ? table[0] : 0;
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

// The Pokemon, in the middle of the arena (loaded as func_8001B480 loads the rental card's)
static void Randomizer_ArenaPokemon(s32 species) {
    unk_D_86002F58_004_000_010* models = func_80019760(1);
    arg1_func_80010CA8 variant;

    // Added to the scene's first list of models, as func_8001BB58 adds one to D_800AC840
    sPokemon = main_pool_alloc(sizeof(unk_D_86002F58_004_000), 0);
    func_80011938(NULL, sPokemon, 0, &D_8006F050, &D_8006F05C, &D_8006F064);
    func_80012094(&sModelLists[0], &sPokemon->unk_000);
    sPokemon->unk_0A6 = 0;
    sPokemon->unk_000.unk_01 &= ~1;

    variant.raw = 0;
    func_800198E4(models, species, variant);
    func_80019CA8(models);
    func_8001BCF0(sPokemon);
    func_8001BC34(sPokemon, 0, species, models->unk_24->unk_08->unk_00[0]);
    func_8001BD04(sPokemon, Randomizer_ArenaIdle(species));
    sPokemon->unk_000.unk_02 &= ~0x40;

    func_8000E88C(&sPokemon->unk_024, 0.0f, 0.0f, 0.0f);
    func_8000E88C(&sPokemon->unk_030, POKEMON_SCALE, POKEMON_SCALE, POKEMON_SCALE);
}

// The camera's distance and what it looks at, from the species' size (see CARD_HEIGHT)
static void Randomizer_ArenaFrame(s32 species) {
    unk_D_8006FF00* info = &D_8006FF00[species - 1];
    f32 cardScale = info->unk_02 / 100.0f;
    // The card's pivot: the middle of the model, scaled (as func_8001B480 reads it)
    f32 middle = -(f32)((s16)(info->unk_14 >> 6) >> 4) / cardScale;
    f32 height = CARD_HEIGHT / cardScale;

    sDistance = height * CAMERA_FRAMING;
    // Above the middle, so that the Pokemon is in the lower part of the screen, under the logo
    sLookHeight = middle + (height * 0.3f);
    // Close to small Pokemon, so the near clipping plane comes closer too
    func_80011E68(&sCamera, CAMERA_FOVY, (sDistance * 0.25f < 192.0f) ? (sDistance * 0.25f) : 192.0f, 24576.0f);
}

// The arena and the Pokemon, on the scene's own stack
static void Randomizer_ArenaSetup(void) {
    // What screens with 3D Pokemon load: fragment31, which the models' and the arenas' code
    // calls into, and the models' archives and work memory. And fragment34 before it:
    // fragment31's table jumps on into it (the arenas' callback 0x810001D0 goes to
    // 0x81407874), and a jump into another fragment is only relocated if that one is loaded
    FRAGMENT_LOAD(fragment34);
    FRAGMENT_LOAD(fragment31);
    func_8001987C();

    Randomizer_ArenaLoad(sArena);
    Randomizer_ArenaPokemon(sSpecies);
    Randomizer_ArenaFrame(sSpecies);
}

// When the title screen starts
void Randomizer_ArenaStart(void) {
    sReady = 0;
    sRandomState = Randomizer_ArenaSeed();
    sArena = Randomizer_ArenaBelow(NUM_ARENAS);
    sSpecies = Randomizer_ArenaBelow(NUM_SPECIES) + 1;
#ifdef ARENA_TEST_ARENA
    sArena = ARENA_TEST_ARENA;
#endif
#ifdef ARENA_TEST_SPECIES
    sSpecies = ARENA_TEST_SPECIES;
#endif

    sStackTop = (u8*)main_pool_alloc(STACK_SIZE, 0) + STACK_SIZE;
    sYaw = Randomizer_ArenaBelow(0x10000);
    Randomizer_CallOnStack(Randomizer_ArenaSetup, sStackTop);
    sReady = 1;

#ifdef ARENA_TEST
    // What was picked, in gRandomizerState's free bytes (0x80000358), for reading from an
    // emulator savestate
    ((u8*)0x80000358)[0] = sArena;
    ((u8*)0x80000358)[1] = sSpecies;
    ((u8*)0x80000358)[2] = (sSky == NULL) ? 0 : (((u32)sSky < 0x10000) ? 1 : 2);
    ((u8*)0x80000358)[3] = 0xA5;
#endif
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
    sYaw += CAMERA_TURN;
    func_8000E88C(&sCamera.unk_60.at, 0.0f, sLookHeight, 0.0f);
    func_80010354(&sCamera.unk_60.at, &sCamera.unk_60.eye, sDistance, CAMERA_PITCH, sYaw);

    func_80015348();
    func_800079C4();
    Randomizer_ArenaSky();
    func_80015094(sScene);
    gSPDisplayList(gDisplayListHead++, D_8006F630);
}

// Every frame, over the title picture and under the logo and "PRESS START"
void Randomizer_ArenaDraw(void) {
    if (sReady == 1) {
        Randomizer_CallOnStack(Randomizer_ArenaRender, sStackTop);
    }
}

#endif
