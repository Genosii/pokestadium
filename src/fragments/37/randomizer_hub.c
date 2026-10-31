/*
 * The 3D hub (randomizer_hub.h): in place of the area select, the Stadium complex seen from
 * where one would stand in the middle of it, at eye level. A prototype.
 *
 * The game has no model of the complex: the area select is one picture of it, seen from
 * above (game-screens.md). So the hub is the open field, the arena of Free Battle (arena 0
 * of stadium_models), with each building a picture standing round the camera in a ring: a
 * piece of the area select's picture, cut along its 16x16 tiles, which is how the game
 * keeps it in memory (func_82200088). The camera never moves, only turns, so flat
 * pictures read as the buildings around one. VS Mewtwo has no building: Mewtwo flies
 * there, the model the battles use.
 *
 * Left and Right turn to the next building, in half a second; the building's name and a line
 * about it show at the bottom. A goes in, returning the mode's state as the area select does;
 * B goes back to the menus. Coming back from a mode, the hub faces its building: the game's
 * last state (gLastGameState) is still that mode's.
 *
 * The scene is built as the randomizer's title builds its own (randomizer_title_arena.c),
 * and drawn on a stack of its own (Randomizer_HubCallOnStack, randomizer_hub_stack.s, a copy of
 * the title's); the pictures go into the same depth buffer after it.
 *
 * Built only with HUB=1; empty otherwise.
 */
#include "randomizer_hub.h"

#ifdef RANDOMIZER_HUB

#include "src/11BA0.h"
#include "src/12D80.h"
#include "src/17300.h"
#include "src/19840.h"
#include "src/1C720.h"
#include "src/1CF30.h"
#include "src/20330.h"
#include "src/20470.h"
#include "src/26820.h"
#include "src/2E110.h"
#include "src/2E460.h"
#include "src/3FB0.h"
#include "src/49790.h"
#include "src/4F410.h"
#include "src/6A40.h"
#include "src/6BC0.h"
#include "src/DDC0.h"
#include "src/F420.h"
#include "src/controller.h"
#include "src/geo_layout.h"
#include "src/memmap.h"
#include "src/memory.h"
#include "src/stage_loader.h"

#define SCREEN_W 320
#define SCREEN_H 240

// The game's last state (gLastGameState, a static of src/29BA0.c; main code never moves)
#define LAST_GAME_STATE (*(s32*)0x8007566C)

#define ARENA 0      // the open field, Free Battle's
#define PICTURE 1    // the area select's picture of the complex, in the backgrounds archive
#define PICTURE_COLUMNS 20 // its 16x16 tiles across (320 pixels)
#define TILE_BYTES 0x200   // a tile, RGBA16

#define CAMERA_NEAR 10.0f // the battle's, which the arenas' fog is set for
#define CAMERA_FAR 12800.0f
#define CAMERA_FOVY 50.0f
#define EYE_Y 60.0f       // a person's eye beside the Pokemon (about 45 units to a metre)
#define LOOK_UP 0.12f     // the camera looks a little up: the buildings stand above the horizon

#define BOARD_DISTANCE 900.0f // the pictures, from the camera
#define BOARD_TILE 88.0f      // a tile of a picture, in the arena's units
#define BOARD_BASE -20.0f     // its bottom, a little into the ground

#define MEWTWO 150
#define MEWTWO_DISTANCE 420.0f
#define MEWTWO_HEIGHT 150.0f
#define MEWTWO_BOB 12.0f

#define NUM_PLACES 8
#define PLACE_ANGLE 0x2000 // between two places, 360 degrees / 8
#define TURN_FRAMES 15     // half a second
#define FADE_FRAMES 16
#define STACK_SIZE 0x4000

void Randomizer_HubCallOnStack(void (*func)(void), void* stackTop);

typedef struct HubPlace {
    /* 0x00 */ s16 state; // what the area select returns for it
    /* 0x02 */ u8 x;      // its picture, in tiles of the area select's (none if cols is 0)
    /* 0x03 */ u8 y;
    /* 0x04 */ u8 cols;
    /* 0x05 */ u8 rows;
    /* 0x08 */ const char* name;
    /* 0x0C */ const char* line;
} HubPlace; // size = 0x10

// Round the camera, to the right: Right goes down the list, Left up it
static HubPlace sPlaces[NUM_PLACES] = {
    { STATE_STADIUM_MENU, 6, 3, 7, 5, "Stadium", "Battle through the four Cups." },
    { STATE_GB_TOWER, 14, 3, 4, 4, "GB Tower", "Play your Game Boy games." },
    { STATE_FREE_BATTLE, 14, 7, 6, 5, "Free Battle", "Battle a friend, under your own rules." },
    { STATE_KIDS_CLUB, 8, 8, 6, 4, "Kids Club", "Play the mini-games." },
    { STATE_VICTORY_PALACE, 4, 8, 4, 4, "Victory Palace", "See the Pok\xE9mon that won the Cups." },
    { STATE_POKEMON_LAB, 1, 5, 5, 4, "Pok\xE9mon Lab", "Sort, trade and study your Pok\xE9mon." },
    { STATE_GYM_LEADER_CASTLE, 1, 0, 4, 5, "Gym Leader Castle", "The eight Gym Leaders, then the Elite Four." },
    { STATE_VS_MEWTWO, 0, 0, 0, 0, "VS Mewtwo", "Face the legendary Mewtwo." },
};
#define PLACE_MEWTWO 7

static unk_D_86002F34_00C sCamera;
static unk_D_8690A610 sFog;
static GraphNode sLayers[3];
static GraphNode sModelLists[2];
static GraphNode* sScene;
static void* sSky;
static u8* sStackTop;
static u8* sPicture;              // the area select's picture, as 16x16 tiles
static Vtx* sBoards[NUM_PLACES];  // each picture's corners, 4 a tile
static unk_D_86002F58_004_000* sMewtwo;
static Mtx sProjection[2];        // the pictures', a frame's being drawn while the next is made
static Mtx sView[2];
static LookAt sLookAt[2];
static Vp sViewport = { { { SCREEN_W * 2, SCREEN_H * 2, G_MAXZ / 2, 0 }, { SCREEN_W * 2, SCREEN_H * 2, G_MAXZ / 2, 0 } } };

static s32 sPlace;      // the place the camera faces, or is turning to
static s32 sHeading;    // where it faces now, 0x10000 to a turn (not wrapped)
static s32 sFrom;       // where a turn started
static s32 sTo;         // and ends
static s32 sTurnFrame;  // TURN_FRAMES when not turning
static s32 sFrame;

static s32 Randomizer_HubLight1(s32 arg0, unk_D_86002F34_alt18* arg1);
static s32 Randomizer_HubLight2(s32 arg0, unk_D_86002F34_alt18* arg1);
static s32 Randomizer_HubLayer(s32 arg0, unk_D_86002F58_004_000* arg1);
static s32 Randomizer_HubCamera(s32 arg0, GraphNode* arg1);

// The battle's scene (D_84384364), with this file's nodes, as the title's
static u32 sSceneLayout[] = {
    0x0C00FFFF, 0x05000000, 0x07000000, (u32)&sCamera, 0x05000000, 0x0D000000, 0x05000000, 0x07000000,
    (u32)&sFog, 0x14000000, 0x002D0019, 0xFFFFFF28, 0x08000000, (u32)Randomizer_HubLight1, 0x00000000, 0x14000000,
    0x002D0019, 0x80808028, 0x08000000, (u32)Randomizer_HubLight2, 0x00000000, 0x16646464, 0x0F000002, 0x05000000,
    0x1F00FFFF, 0x00000000, 0x00000000, 0x00000000, 0x00640064, 0x00640000, 0x08000000, (u32)Randomizer_HubLayer,
    0x00000000, 0x05000000, 0x07000000, (u32)&sLayers[0], 0x06000000, 0x06000000, 0x0F000003, 0x05000000,
    0x1F00FFFF, 0x00000000, 0x00000000, 0x00000000, 0x00640064, 0x00640000, 0x08000000, (u32)Randomizer_HubLayer,
    0x00000000, 0x05000000, 0x07000000, (u32)&sLayers[1], 0x06000000, 0x06000000, 0x0F000002, 0x05000000,
    0x1F00FFFF, 0x00000000, 0x00000000, 0x00000000, 0x00640064, 0x00640000, 0x08000000, (u32)Randomizer_HubLayer,
    0x00000000, 0x05000000, 0x07000000, (u32)&sLayers[2], 0x06000000, 0x06000000, 0x0F000003, 0x05000000,
    0x0A000000, (u32)&sModelLists[0], 0x06000000, 0x0F000002, 0x05000000, 0x0A000000, (u32)&sModelLists[1], 0x06000000,
    0x09000000, 0x08000000, (u32)Randomizer_HubCamera, 0x00000000, 0x06000000, 0x06000000, 0x06000000, 0x01000000,
};

// The lights, turned with the camera (func_84300058 and func_843000C0)
static s32 Randomizer_HubLight1(s32 arg0, unk_D_86002F34_alt18* arg1) {
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

static s32 Randomizer_HubLight2(s32 arg0, unk_D_86002F34_alt18* arg1) {
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

// The arena's layers at their size and colour (func_8430012C)
static s32 Randomizer_HubLayer(s32 arg0, unk_D_86002F58_004_000* arg1) {
    if (arg0 == 2) {
        func_8000E88C(&arg1->unk_030, 1.0f, 1.0f, 1.0f);
        arg1->unk_03C.rgba = 0xFFFFFF00;
    }
    return 0;
}

// Where the battle moves its camera; this file moves it before drawing
static s32 Randomizer_HubCamera(UNUSED s32 arg0, UNUSED GraphNode* arg1) {
    return 0;
}

// The open field: its layers in the scene, its sky and its fog (as func_84301430 sets them)
static void Randomizer_HubArena(void) {
    FragmentEntry parts = func_8000484C(ASSET_LOAD2(stadium_models, 1, 1), ARENA);
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
        sFog.unk_00.unk_01 |= 1;
    }
}

// Mewtwo, in the first list of models, loaded as the title loads its Pokemon, in its idle
// animation (0, which floats); placed every frame (Randomizer_HubMewtwoFly)
static void Randomizer_HubMewtwo(void) {
    unk_D_86002F58_004_000_010* models = func_80019760(1);
    unk_D_86002F58_004_000* model = main_pool_alloc(sizeof(unk_D_86002F58_004_000), 0);
    arg1_func_80010CA8 variant;

    sMewtwo = model;
    func_80011938(NULL, model, 0, &D_8006F050, &D_8006F05C, &D_8006F064);
    func_80012094(&sModelLists[0], (GraphNode*)&model->unk_000);
    model->unk_0A6 = 0;

    variant.raw = 0;
    func_800198E4(models, MEWTWO, variant);
    func_80019CA8(models);
    func_8001BCF0(model);
    func_8001BC34(model, 0, MEWTWO, models->unk_24->unk_08->unk_00[0]);

    model->unk_000.unk_02 &= ~0x40; // no shadow: it's in the air
    model->unk_000.unk_01 |= 1;
    func_8000E88C(&model->unk_030, 1.0f, 1.0f, 1.0f);
    func_8001BD04(model, 0);
    func_80017464(model, 0);
}

// Each place's heading: Right turns the camera clockwise, seen from above
static s16 Randomizer_HubHeading(s32 place) {
    return -place * PLACE_ANGLE;
}

/*
 * A place's picture, standing BOARD_DISTANCE away and facing the camera: a square of two
 * triangles a tile, each with its tile of the picture. The screen's right, for a camera
 * looking along (x, z), is (-z, x) (game-engine.md).
 */
static void Randomizer_HubBoard(s32 place) {
    HubPlace* p = &sPlaces[place];
    s16 heading = Randomizer_HubHeading(place);
    f32 fx = SINS(heading);
    f32 fz = COSS(heading);
    f32 cx = fx * BOARD_DISTANCE;
    f32 cz = fz * BOARD_DISTANCE;
    f32 left = -(p->cols * BOARD_TILE) / 2.0f;
    f32 top = BOARD_BASE + (p->rows * BOARD_TILE);
    Vtx* v;
    s32 row;
    s32 col;
    s32 i;

    if (p->cols == 0) {
        sBoards[place] = NULL;
        return;
    }

    v = main_pool_alloc(p->cols * p->rows * 4 * sizeof(Vtx), 0);
    sBoards[place] = v;
    for (row = 0; row < p->rows; row++) {
        for (col = 0; col < p->cols; col++) {
            for (i = 0; i < 4; i++) {
                // The corners clockwise from the top left
                f32 x = left + ((col + ((i == 1) || (i == 2))) * BOARD_TILE);
                f32 y = top - ((row + (i >= 2)) * BOARD_TILE);

                v->v.ob[0] = cx - (fz * x);
                v->v.ob[1] = y;
                v->v.ob[2] = cz + (fx * x);
                v->v.flag = 0;
                v->v.tc[0] = ((i == 1) || (i == 2)) ? (16 << 5) : 0;
                v->v.tc[1] = (i >= 2) ? (16 << 5) : 0;
                v->v.cn[0] = v->v.cn[1] = v->v.cn[2] = v->v.cn[3] = 0xFF;
                v++;
            }
        }
    }
    osWritebackDCache(sBoards[place], p->cols * p->rows * 4 * sizeof(Vtx));
}

// The arena, Mewtwo and the pictures, on the scene's own stack
static void Randomizer_HubSetup(void) {
    s32 i;

    // As the title: fragment34 before fragment31, and the models' archives and memory
    FRAGMENT_LOAD(fragment34);
    FRAGMENT_LOAD(fragment31);
    func_8001987C();

    Randomizer_HubArena();
    Randomizer_HubMewtwo();
    sPicture = func_8000484C(ASSET_LOAD2(backgrounds, 1, 1), PICTURE);
    for (i = 0; i < NUM_PLACES; i++) {
        Randomizer_HubBoard(i);
    }
}

// The sky behind the arena, and the depth cleared (the title's Randomizer_ArenaSky)
static void Randomizer_HubSky(void) {
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
    gSPTextureRectangle(gDisplayListHead++, 0, 0, SCREEN_W << 2, (SCREEN_H / 2) << 2, G_TX_RENDERTILE, 0, 0, 0,
                        0x10000 / (SCREEN_H / 2));
    gSPTextureRectangle(gDisplayListHead++, 0, (SCREEN_H / 2) << 2, SCREEN_W << 2, SCREEN_H << 2, G_TX_RENDERTILE, 0,
                        0x07E0, 0, 0);
    gSPDisplayList(gDisplayListHead++, D_8006F630);
}

// Mewtwo, flying slowly up and down where VS Mewtwo is, facing the camera
static void Randomizer_HubMewtwoFly(void) {
    s16 heading = Randomizer_HubHeading(PLACE_MEWTWO);
    f32 bob = SINS(sFrame * 0x200) * MEWTWO_BOB;

    func_8000E88C(&sMewtwo->unk_024, SINS(heading) * MEWTWO_DISTANCE, MEWTWO_HEIGHT + bob,
                  COSS(heading) * MEWTWO_DISTANCE);
    sMewtwo->unk_01E.y = heading + 0x8000;
}

// The pictures facing the camera's way, into the scene's depth, from the camera's view
static void Randomizer_HubBoards(Vec3f* eye, Vec3f* at) {
    Mtx* projection = &sProjection[sFrame & 1];
    Mtx* view = &sView[sFrame & 1];
    MtxF viewF;
    u16 norm;
    s32 place;
    s32 row;
    s32 col;

    guPerspective(projection, &norm, CAMERA_FOVY, (f32)SCREEN_W / SCREEN_H, CAMERA_NEAR, CAMERA_FAR, 1.0f);
    // As the game's camera makes its view (func_80013464); the game has no guLookAt
    guLookAtReflectF(viewF.mf, &sLookAt[sFrame & 1], eye->x, eye->y, eye->z, at->x, at->y, at->z, 0.0f, 1.0f,
                     0.0f);
    guMtxF2L(viewF.mf, view);

    gDPPipeSync(gDisplayListHead++);
    gSPViewport(gDisplayListHead++, &sViewport);
    gSPPerspNormalize(gDisplayListHead++, norm);
    gSPMatrix(gDisplayListHead++, projection, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    gSPMatrix(gDisplayListHead++, view, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPClearGeometryMode(gDisplayListHead++, G_LIGHTING | G_FOG | G_CULL_BOTH | G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR);
    gSPSetGeometryMode(gDisplayListHead++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
    gDPSetCycleType(gDisplayListHead++, G_CYC_1CYCLE);
    gDPSetRenderMode(gDisplayListHead++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
    gDPSetCombineMode(gDisplayListHead++, G_CC_DECALRGB, G_CC_DECALRGB);
    gDPSetTexturePersp(gDisplayListHead++, G_TP_PERSP);
    gDPSetTextureFilter(gDisplayListHead++, G_TF_BILERP);
    gDPSetTextureLUT(gDisplayListHead++, G_TT_NONE);
    gDPSetTextureLOD(gDisplayListHead++, G_TL_TILE);
    gSPTexture(gDisplayListHead++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);

    for (place = 0; place < NUM_PLACES; place++) {
        HubPlace* p = &sPlaces[place];
        Vtx* v = sBoards[place];
        s16 off = Randomizer_HubHeading(place) - (s16)sHeading;

        // Only the ones in view or nearly
        if ((v == NULL) || (ABS(off) > 0x4000)) {
            continue;
        }
        for (row = 0; row < p->rows; row++) {
            for (col = 0; col < p->cols; col++) {
                u8* tile = sPicture + ((((p->y + row) * PICTURE_COLUMNS) + p->x + col) * TILE_BYTES);

                gDPLoadTextureBlock(gDisplayListHead++, tile, G_IM_FMT_RGBA, G_IM_SIZ_16b, 16, 16, 0,
                                    G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK,
                                    G_TX_NOLOD, G_TX_NOLOD);
                gSPVertex(gDisplayListHead++, v, 4, 0);
                gSP2Triangles(gDisplayListHead++, 0, 1, 2, 0, 0, 2, 3, 0);
                v += 4;
            }
        }
    }

    gSPTexture(gDisplayListHead++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_OFF);
    gDPPipeSync(gDisplayListHead++);
}

// The camera's turn on a frame, eased in and out
static void Randomizer_HubTurn(void) {
    f32 t;

    if (sTurnFrame >= TURN_FRAMES) {
        sHeading = sTo;
        return;
    }
    sTurnFrame++;
    t = (f32)sTurnFrame / TURN_FRAMES;
    t = t * t * (3.0f - (2.0f * t));
    sHeading = sFrom + (s32)((sTo - sFrom) * t);
}

// The scene, then the pictures, on the scene's own stack
static void Randomizer_HubRender(void) {
    Vec3f eye;
    Vec3f at;

    Randomizer_HubTurn();
    Randomizer_HubMewtwoFly();
    func_8000E88C(&eye, 0.0f, EYE_Y, 0.0f);
    func_8000E88C(&at, SINS((s16)sHeading) * 1000.0f, EYE_Y + (LOOK_UP * 1000.0f), COSS((s16)sHeading) * 1000.0f);

    func_80015348();
    Randomizer_HubSky();
    func_8000E88C(&sCamera.unk_60.up, 0.0f, 1.0f, 0.0f);
    func_80011DAC(&sCamera, 0, 0, SCREEN_W, SCREEN_H);
    func_80011E68(&sCamera, CAMERA_FOVY, CAMERA_NEAR, CAMERA_FAR);
    sCamera.unk_60.eye = eye;
    sCamera.unk_60.at = at;
    func_80015094(sScene);
    Randomizer_HubBoards(&eye, &at);
    gSPDisplayList(gDisplayListHead++, D_8006F630);
}

// The place's name and its line, at the bottom, once the camera has stopped
static void Randomizer_HubCaption(void) {
    HubPlace* p = &sPlaces[sPlace];

    if (sTurnFrame < TURN_FRAMES) {
        return;
    }
    func_80020460(16, 182, SCREEN_W - 32, 46, 0x2121);

    func_8001F3F4();
    func_8001EBE0(4, 0);
    func_8001F324(0xFF, 0xFF, 0x00, 0xFF);
    func_8001F1E8((SCREEN_W - func_8001F5B0(0, 0, "%s", p->name)) / 2, 188, "%s", p->name);
    func_8001EBE0(2, 0);
    func_8001F324(0xFF, 0xFF, 0xFF, 0xFF);
    func_8001F1E8((SCREEN_W - func_8001F5B0(0, 0, "%s", p->line)) / 2, 210, "%s", p->line);
    func_8001F444();
}

static void Randomizer_HubFrame(void) {
    Cont_StartReadInputs();
    Cont_ReadInputs();
    func_8001F730();

    func_800079C4();
    Randomizer_HubCallOnStack(Randomizer_HubRender, sStackTop);
    Randomizer_HubCaption();
    func_80007778();
    sFrame++;
}

// Turns to the next place to the right (1) or the left (-1)
static void Randomizer_HubGo(s32 step) {
    sPlace = (sPlace + step + NUM_PLACES) % NUM_PLACES;
    sFrom = sHeading;
    sTo = sHeading - (step * PLACE_ANGLE);
    sTurnFrame = 0;
    func_80048B90(0x32); // the area select's sound for moving
}

// Fades the screen out to colour (RGBA16), drawing meanwhile
static void Randomizer_HubFadeOut(u16 colour) {
    func_80007990(colour);
    func_80006CB4(FADE_FRAMES);
    do {
        Randomizer_HubFrame();
    } while (func_80007604() != 1);
}

// The hub's screen, as the area select's (func_82202DC4); returns the next state
s32 Randomizer_HubEntry(void) {
    unk_func_80007444* screen;
    s32 state = -1;
    s32 i;

    func_80005E40(0x10000, 0);
    screen = func_80007444(0, 1, 3, 1, 2, 1); // 320x240, as the title's 3D scene
    func_8001E94C(6, 0);
    func_8002D510();
    func_80028AFC(2);

    sPlace = 0;
    for (i = 0; i < NUM_PLACES; i++) {
        if (sPlaces[i].state == LAST_GAME_STATE) {
            sPlace = i;
        }
    }
    sHeading = sFrom = sTo = Randomizer_HubHeading(sPlace);
    sTurnFrame = TURN_FRAMES;
    sFrame = 0;

    sStackTop = (u8*)main_pool_alloc(STACK_SIZE, 0) + STACK_SIZE;
    Randomizer_HubCallOnStack(Randomizer_HubSetup, sStackTop);

    func_80007678(screen);
    func_8000D1F0(0x50); // the area select's music
    func_80006C6C(0xA);
    while (func_80007604() != 0) {
        Randomizer_HubFrame();
    }

    while (state < 0) {
        Randomizer_HubFrame();
        if (sTurnFrame < TURN_FRAMES) {
            continue;
        }
        if (gPlayer1Controller->buttonDown & R_JPAD) {
            Randomizer_HubGo(1);
        } else if (gPlayer1Controller->buttonDown & L_JPAD) {
            Randomizer_HubGo(-1);
        } else if (gPlayer1Controller->buttonPressed & A_BUTTON) {
            // As the area select goes in: Mewtwo's cry for VS Mewtwo, and the music fading,
            // but for the GB Tower's
            state = sPlaces[sPlace].state;
            if (state == STATE_VS_MEWTWO) {
                func_8004E810(MEWTWO, 8);
            }
            func_80048B90(0x33);
            func_80048B90(0x16);
            if (state == STATE_VS_MEWTWO) {
                func_8000D278(0x20);
            } else if (state != STATE_GB_TOWER) {
                func_8000D278(0x28);
            }
            Randomizer_HubFadeOut((state == STATE_VS_MEWTWO) ? 0xFFFF : 1);
        } else if (gPlayer1Controller->buttonPressed & B_BUTTON) {
            func_8000D23C(0x28);
            func_80048B90(3);
            state = STATE_MENU_SELECT;
            Randomizer_HubFadeOut(0xFFFF);
        }
    }

    func_800076C0();
    func_80005EAC();
    return state;
}

#endif
