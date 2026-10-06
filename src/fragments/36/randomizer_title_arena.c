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
 *  - each Pokemon is a model in the first list, loaded the way the rental card loads one
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
 * The camera follows the fight (see SHOT_*): between turns a shot of the field or of one
 * of them, then the attacker as its turn begins, then the defender as the hit lands; a
 * critical hit or a Hyper Beam lands three times, from three angles.
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

#define PAUSE_FRAMES 90     // in the idle stance between turns
#define HYPER_BEAM 63
#define BIG_HIT_CHANCE 5    // one turn in this many is a critical hit (as is every Hyper Beam)
#define REPLAY_TIMES 3      // a critical hit or a Hyper Beam lands this many times, from as many angles
#define REPLAY_FRAMES 30    // each time
#define REPLAY_LEAD 8       // the attack goes back this many frames before the hit each time
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
 * The camera's shots. The logo and the subtitle cover the top third of the screen, so the
 * camera looks at a point above what it films on the screen, by LOOK_RAISE of its distance
 * (about 9 degrees), which puts that about 40 pixels under the middle, in the middle of the
 * rest.
 */
#define CAMERA_FOVY 50.0f
#define TAN_HALF_FOVY 0.4663f // tan(CAMERA_FOVY / 2)
#define RAD_TO_FOVY(half) ((half) * (360.0f / 3.14159265f)) // half the field of view, in degrees
#define CAMERA_NEAR 10.0f // the battle's (func_8431AFD0)
#define CAMERA_FAR 12800.0f
#define LOOK_RAISE 0.15f

/*
 * The shots, after the way Pokemon Battle Revolution films its battles (its first trailer,
 * 2006): the camera follows the fight, a new shot every two or three seconds, from the
 * ground up to high above, some of them tilted, and every one of them moving.
 *  - Between turns, a shot of the field or of one of them, a new one every IDLE_FRAMES.
 *  - WINDUP_FRAMES before a turn's attack, the attacker.
 *  - As the hit lands (the defender's reaction starts), the defender, close and tilted, the
 *    camera shaking; reached by a cut or a quick swing of the camera from the attacker.
 */
enum {
    // Between turns
    SHOT_PAN,      // round the whole field, from far
    SHOT_ORBIT,    // going round one, from a little farther
    SHOT_SPLIT,    // the screen split in two, each half close on one
    SHOT_CHASE,    // from behind one, moving up to the other
    SHOT_OVERHEAD, // high above the field, looking down, turning
    SHOT_SIDE,     // low beside one, drifting sideways
    SHOT_GAZE,     // close on one's head, following it
    SHOT_GAMEBOY,  // the Game Boy games' view (see SHOT_RBY)
    SHOT_SIGHTS,   // over one's shoulder at the other (see SHOT_AIM)
    NUM_IDLE_SHOTS,
    // The attacker
    SHOT_LOW = NUM_IDLE_SHOTS, // from the ground, looking up, pushing in
    SHOT_HIGH,                 // from above, looking down, coming down
    SHOT_PUSH,                 // in front, pushing in fast
    SHOT_SHOULDER,             // from behind it, at its target
    SHOT_FACE,                 // close on its head, following it, from low, level or high
    SHOT_TRACK,                // beside it, moving with it, as Colosseum and XD film attacks
    SHOT_RBY,                  // behind the one on the left at the other, as in Red, Blue and Yellow
    SHOT_AIM,                  // over its shoulder at its target, as a game played over the shoulder aims
    SHOT_HIT,                  // the defender: close, tilted, pulling back
    SHOT_HIT_LOW,              // the defender: low and wider, tilted
    SHOT_WHIP,                 // a quick swing from the attacker to the defender
};
#define NUM_ATTACK_SHOTS (SHOT_HIT - SHOT_LOW)
#define NUM_HIT_SHOTS (SHOT_WHIP - SHOT_HIT)

#define IDLE_FRAMES 75   // the longest a shot between turns lasts (30 frames a second)
#define WINDUP_FRAMES 24 // the attacker's shot starts this long before its attack
#define MOVE_FRAMES 60   // the shots that push in or come down do it over this long
#define WHIP_FRAMES 8
#define SHAKE_FRAMES 14
#define SHAKE_SIZE 0.025f // of the camera's distance
#define GROUND_EYE 8.0f   // the camera's height on the ground

#define PAN_FRAMING 1.05f   // distance / the width of the field
#define PAN_PITCH 0x700     // looking down 10 degrees
#define PAN_TURN 0x70
#define OVERHEAD_PITCH 0x2400 // looking down 50 degrees
#define OVERHEAD_TURN 0x40
#define SPLIT_PITCH 0x300
#define SPLIT_TURN 0x0C
#define SPLIT_FRAMING 3.2f  // distance / height, in a half as wide
#define ORBIT_FRAMING 3.8f  // distance / height
#define ORBIT_PITCH 0x500
#define ORBIT_TURN 0xC0     // a full turn in about 11 seconds
#define CHASE_MOVE 0.8f     // how much of the shot the camera moves for
#define MIN_DISTANCE 90.0f  // from a Pokemon, for the smallest ones
#define TRACK_FOLLOW 0.2f   // how much of the way to where a Pokemon is the framing moves each frame
#define ACTION_FRAMING 2.2f // distance / height
#define RBY_RAISE -0.04f    // the other a little above the middle of the screen,
#define RBY_LATERAL 0.33f   // and to the right, at about (245, 110)
#define RBY_FAR 1.6f        // the camera this much farther behind the one on the left than close
#define RBY_HOLD 12         // frames before it zooms in on the other,
#define RBY_ZOOM_FRAMES 16  // and how long it takes, easing in and out
#define RBY_ZOOM_FRAMING 0.8f // zoomed in: half the screen's height / the other's height,
#define RBY_ZOOM_MIN 0.05f  // but no narrower than about 6 degrees (the tangent of half that)
#define AIM_BACK 0.9f       // the camera behind the head by this much of the height and AIM_BACK_MIN,
#define AIM_BACK_MIN 50.0f
#define AIM_ABOVE 0.25f     // above it by this much,
#define AIM_BESIDE 0.75f    // and to its right by this much of how far behind, clear of wings;
#define AIM_RAISE 0.06f     // the other under the logo,
#define AIM_LATERAL 0.12f   // right of the middle, at about (191, 136)
#define FACE_FRAMING 0.8f   // distance / height
#define FACE_MIN 60.0f
#define FACE_FOLLOW 0.2f    // how much of the way to the head the camera turns each frame

/*
 * Points a model marks on itself as it's drawn (func_80014CB8, read with func_80015390): the
 * battle aims its effects and its camera at them. Measured for a dozen species: 7 is the head
 * or the mouth, 11 the top of the head on some (Onix, Gyarados, Lapras) but a cannon or a foot
 * on others, 9 the chest, 1 and 2 the hands, 3 to 6 the feet, 8 the tail, 100 the root of the
 * body (where the shadow goes).
 */
#define POINT_HEAD 7
#define POINT_TOP 11

// Where the camera is and what it looks at, how far it's tilted and how wide it sees
typedef struct RandomizerArenaView {
    /* 0x00 */ Vec3f eye;
    /* 0x0C */ Vec3f at;
    /* 0x18 */ s16 roll;
    /* 0x1C */ f32 fovy; // in degrees
} RandomizerArenaView; // size = 0x20

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
    /* 0x28 */ u8 beam;    // its Hyper Beam animation
    /* 0x29 */ u8 tracked; // its middle found from its model's points (Randomizer_ArenaTrack)
} RandomizerArenaSide; // size = 0x2C

enum {
    PHASE_PAUSE,
    PHASE_ATTACK,
};

static unk_D_86002F34_00C sCamera;
static unk_D_8690A610 sFog;
static GraphNode sLayers[3];
static GraphNode sModelLists[2]; // the scene's own, in place of D_800AC840 and D_800AC858
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
static u8 sBigHit;    // a critical hit or a Hyper Beam this turn
static u8 sReplay;    // which time it's landing (0 when it isn't being replayed)
static s16 sReplayFrame;
static s16 sImpact;   // the attacker's animation's frame as the hit landed
static s16 sTimer;

static u8 sShot;
static u8 sShotSide; // the one it's on
static u8 sIdleShot; // the last of the shots between turns
static u8 sNextShot; // after a swing of the camera
static s16 sShotFrame;
static s16 sShotYaw;  // where a shot that goes round starts
static s16 sShotPitch; // how far above what it looks at a shot is
static s16 sShotSlant; // how far to the side of a Pokemon's front a shot is
static s16 sShotTurn; // which way it goes round, or to which side it's tilted: 1 or -1
static s16 sShotRoll; // how far it's tilted
static s16 sShake;    // frames of shaking left
static Vec3f sFocus;  // where a shot that follows a head looks, catching up with it
static s16 sFocusYaw; // the side of the Pokemon the head is on
static u8 sFocusSet;
static RandomizerArenaView sLastView; // the last frame's, for the swing

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

    // Hyper Beam always among them (the last if there are already as many as there can be)
    side->beam = sTable[(HYPER_BEAM - 1) * ANIM_ENTRY_SIZE];
    if ((side->beam < count) && (side->beam != side->idle) && (side->beam != side->hit)) {
        for (i = 0; i < side->numAttacks; i++) {
            if (side->attacks[i] == side->beam) {
                break;
            }
        }
        if (i == side->numAttacks) {
            side->attacks[(side->numAttacks < MAX_ATTACKS) ? side->numAttacks++ : (MAX_ATTACKS - 1)] = side->beam;
        }
    } else {
        side->beam = 0xFF;
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
 * to the scene's first list of models as func_8001BB58 adds one to D_800AC840, and loaded as
 * func_8001B480 loads the rental card's.
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
    func_80012094(&sModelLists[0], (GraphNode*)&model->unk_000);
    model->unk_0A6 = index; // its side, for the effects some species have (func_8003260C)
    side->tracked = FALSE;
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

// Cuts to a shot of a side's Pokemon (or both), set up at random
static void Randomizer_ArenaCut(s32 shot, s32 side) {
    sShot = shot;
    sShotSide = side;
    sShotFrame = 0;
    sShotYaw = Randomizer_ArenaBelow(0x10000);
    sShotSlant = 0x0C00 + Randomizer_ArenaBelow(0x1800); // 17 to 50 degrees
    sShotTurn = Randomizer_ArenaSign();
    // Half of them tilted, up to 10 degrees
    sShotRoll = Randomizer_ArenaBelow(2) ? (0x200 + Randomizer_ArenaBelow(0x500)) * sShotTurn : 0;
    // For the head: from below, level or above (-11, 6 or 22 degrees)
    sShotPitch = ((Randomizer_ArenaBelow(3) - 1) * 0x0C00) + 0x0400;
    sFocusSet = FALSE;
}

// A shot between turns, never the same kind twice in a row
static void Randomizer_ArenaIdleCut(void) {
    s32 shot = Randomizer_ArenaBelow(NUM_IDLE_SHOTS - 1);

    if (shot >= sIdleShot) {
        shot++;
    }
#ifdef ARENA_TEST_SHOT
    shot = ARENA_TEST_SHOT;
#endif
    sIdleShot = shot;
    Randomizer_ArenaCut(shot, Randomizer_ArenaBelow(SIDES));
}

// The attacker, as its turn begins
static void Randomizer_ArenaAttackCut(void) {
    s32 shot = SHOT_LOW + Randomizer_ArenaBelow(NUM_ATTACK_SHOTS);

#ifdef ARENA_TEST_ATTACK_SHOT
    shot = ARENA_TEST_ATTACK_SHOT;
#endif
    Randomizer_ArenaCut(shot, sAttacker);
}

// The defender, as the hit lands: a cut, or a swing of the camera from the attacker to it
static void Randomizer_ArenaHitCut(void) {
    s32 shot = SHOT_HIT + Randomizer_ArenaBelow(NUM_HIT_SHOTS);
    s32 whip = (sShot >= SHOT_LOW) && Randomizer_ArenaBelow(2);

#ifdef ARENA_TEST_HIT_SHOT
    shot = ARENA_TEST_HIT_SHOT;
#endif
#ifdef ARENA_TEST_WHIP
    whip = (sShot >= SHOT_LOW) && ARENA_TEST_WHIP;
#endif
    Randomizer_ArenaCut(shot, sAttacker ^ 1);
    sShotRoll = (0x500 + Randomizer_ArenaBelow(0x400)) * sShotTurn; // 7 to 12 degrees
    if (whip) {
        sNextShot = shot;
        sShot = SHOT_WHIP;
    } else {
        sShake = SHAKE_FRAMES;
    }
}

// A critical hit or a Hyper Beam landing, the first time or again: low on the defender, then
// from behind the attacker, then close on the defender from its other side, shaking each time
static void Randomizer_ArenaReplay(void) {
    static const u8 sReplayShots[REPLAY_TIMES] = { SHOT_HIT_LOW, SHOT_SHOULDER, SHOT_HIT };
    s32 shot = sReplayShots[sReplay];

    Randomizer_ArenaCut(shot, (shot == SHOT_SHOULDER) ? sAttacker : (sAttacker ^ 1));
    sShotRoll = (0x500 + Randomizer_ArenaBelow(0x400)) * sShotTurn;
    sShake = SHAKE_FRAMES;
    sReplay++;
    sReplayFrame = 0;
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
    // The whole field first
    sShake = 0;
    sIdleShot = SHOT_PAN;
    Randomizer_ArenaCut(SHOT_PAN, 0);
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
    s32 anim;

    switch (sPhase) {
        case PHASE_PAUSE:
            if ((sTimer == WINDUP_FRAMES) && (attacker->numAttacks != 0)) {
                Randomizer_ArenaAttackCut();
            }
            if (--sTimer > 0) {
                break;
            }
            if (attacker->numAttacks == 0) {
                // Nothing to attack with: the other's turn
                sAttacker ^= 1;
                sTimer = PAUSE_FRAMES;
                break;
            }
            anim = attacker->attacks[Randomizer_ArenaBelow(attacker->numAttacks)];
            Randomizer_ArenaPlay(attacker, anim, 1);
            func_8004E810(attacker->species, CRY_MODE);
            sBigHit = (anim == attacker->beam) || (Randomizer_ArenaBelow(BIG_HIT_CHANCE) == 0);
#ifdef ARENA_TEST_BIG
            sBigHit = TRUE;
#endif
            sReplay = 0;
            sHitStarted = 0;
            sTimer = TURN_MAX_FRAMES;
            sPhase = PHASE_ATTACK;
            break;

        case PHASE_ATTACK:
            if (!sHitStarted && (!attacker->busy || (Randomizer_ArenaPercent(attacker) >= HIT_AT_PERCENT))) {
                Randomizer_ArenaPlay(defender, defender->hit, 1);
                sHitStarted = 1;
                if (sBigHit) {
                    sImpact = Randomizer_ArenaFrame(attacker);
                    Randomizer_ArenaReplay();
                } else {
                    Randomizer_ArenaHitCut();
                }
            }
            if (sReplay != 0) {
                // A critical hit or a Hyper Beam lands again from another angle, the attack
                // going back to just before the hit and the defender reacting again, before
                // the turn goes on to its end
                if (++sReplayFrame < REPLAY_FRAMES) {
                    break;
                }
                if (sReplay < REPLAY_TIMES) {
                    func_80017464(attacker->model, MAX(sImpact - REPLAY_LEAD, 0));
                    attacker->lastFrame = -1;
                    Randomizer_ArenaPlay(defender, defender->hit, 1);
                    Randomizer_ArenaReplay();
                    break;
                }
                sReplay = 0;
            }
            attackerDone = Randomizer_ArenaDone(attacker);
            defenderDone = Randomizer_ArenaDone(defender);
            if (sHitStarted && attackerDone && defenderDone) {
                sAttacker ^= 1;
                sTimer = PAUSE_FRAMES;
                sPhase = PHASE_PAUSE;
                Randomizer_ArenaIdleCut();
            } else if (--sTimer <= 0) {
                Randomizer_ArenaPlay(attacker, attacker->idle, 0);
                Randomizer_ArenaPlay(defender, defender->idle, 0);
                sAttacker ^= 1;
                sTimer = PAUSE_FRAMES;
                sPhase = PHASE_PAUSE;
                Randomizer_ArenaIdleCut();
            }
            break;
    }
}

// How far from a side's Pokemon the camera is to show it framing times its height
static f32 Randomizer_ArenaDistance(RandomizerArenaSide* side, f32 framing) {
    f32 distance = side->height * framing;

    return (distance < MIN_DISTANCE) ? MIN_DISTANCE : distance;
}

// The length of a vector, which is made a unit long (if it isn't 0)
static f32 Randomizer_ArenaNormalize(Vec3f* v) {
    f32 length = sqrtf(SQ(v->x) + SQ(v->y) + SQ(v->z));

    if (length > 0.001f) {
        v->x /= length;
        v->y /= length;
        v->z /= length;
    }
    return length;
}

// The directions to the right of and up from looking along forward (a unit vector), level: the
// screen's, for a camera looking that way
static void Randomizer_ArenaAxes(Vec3f* forward, Vec3f* right, Vec3f* up) {
    func_8000E88C(right, -forward->z, 0.0f, forward->x);
    if (Randomizer_ArenaNormalize(right) < 0.001f) {
        func_8000E88C(right, 1.0f, 0.0f, 0.0f); // straight up or down
    }
    func_8000E88C(up, (right->y * forward->z) - (right->z * forward->y),
                  (right->z * forward->x) - (right->x * forward->z), (right->x * forward->y) - (right->y * forward->x));
}

/*
 * The camera at eye framing a point: looking at a point above it, by raise of its distance
 * (LOOK_RAISE puts it under the logo from any height), and to its left, by lateral of its
 * distance, which puts it right of the middle of the screen; tilted by roll
 */
static void Randomizer_ArenaAimAt(RandomizerArenaView* view, Vec3f* eye, Vec3f* point, f32 raise, f32 lateral,
                                  s16 roll) {
    Vec3f forward;
    Vec3f right;
    Vec3f up;
    f32 distance;

    func_8000E88C(&forward, point->x - eye->x, point->y - eye->y, point->z - eye->z);
    distance = Randomizer_ArenaNormalize(&forward);
    Randomizer_ArenaAxes(&forward, &right, &up);
    view->eye = *eye;
    func_8000E88C(&view->at, point->x + (((up.x * raise) - (right.x * lateral)) * distance),
                  point->y + (((up.y * raise) - (right.y * lateral)) * distance),
                  point->z + (((up.z * raise) - (right.z * lateral)) * distance));
    view->roll = roll;
    view->fovy = CAMERA_FOVY;
}

// The same, with the point a little under the middle of the screen
static void Randomizer_ArenaAim(RandomizerArenaView* view, Vec3f* eye, Vec3f* point, s16 roll) {
    Randomizer_ArenaAimAt(view, eye, point, LOOK_RAISE, 0.0f, roll);
}

// The camera distance away from a point, pitch above it, at yaw round it, framing it
static void Randomizer_ArenaAimFrom(RandomizerArenaView* view, Vec3f* point, f32 distance, s16 pitch, s16 yaw,
                                    s16 roll) {
    Vec3f eye;

    func_80010354(point, &eye, distance, pitch, yaw);
    Randomizer_ArenaAim(view, &eye, point, roll);
}

// The camera on a level with height, distance away from a point across the ground, at yaw
static void Randomizer_ArenaAimLevel(RandomizerArenaView* view, Vec3f* point, f32 distance, f32 height, s16 yaw,
                                     s16 roll) {
    Vec3f eye;

    func_8000E88C(&eye, point->x + (distance * SINS(yaw)), height, point->z + (distance * COSS(yaw)));
    Randomizer_ArenaAim(view, &eye, point, roll);
}

/*
 * Draws the scene from a view, into a part of the screen x to x + w wide. The near and far
 * clipping planes are the battle's: the arenas' fog is set for them, and the fog comes out
 * thicker or thinner with the near plane. The camera's up (unk_60.up) tilts it.
 */
static void Randomizer_ArenaDrawView(s32 x, s32 w, RandomizerArenaView* view) {
    Vec3f forward;
    Vec3f right;
    Vec3f up;

    func_8000E88C(&forward, view->at.x - view->eye.x, view->at.y - view->eye.y, view->at.z - view->eye.z);
    Randomizer_ArenaNormalize(&forward);
    Randomizer_ArenaAxes(&forward, &right, &up);
    func_8000E88C(&sCamera.unk_60.up, (up.x * COSS(view->roll)) + (right.x * SINS(view->roll)),
                  (up.y * COSS(view->roll)) + (right.y * SINS(view->roll)),
                  (up.z * COSS(view->roll)) + (right.z * SINS(view->roll)));
    func_80011DAC(&sCamera, x, 0, w, SCREEN_H);
    func_80011E68(&sCamera, view->fovy, CAMERA_NEAR, CAMERA_FAR);
    sCamera.unk_60.at = view->at;
    sCamera.unk_60.eye = view->eye;
    func_80015094(sScene);
}

/*
 * Draws only one side's Pokemon, or both (SIDES). In a split screen each half has only its own:
 * a model drawn twice in a frame, from two cameras, comes out in pieces from the second, since
 * the effects some animations have (func_80032F94) keep what they drew from the first camera
 * to draw again (an attacking Alakazam or Pidgeotto came out as big shards over the right half
 * in arenas 13 and 15)
 */
static void Randomizer_ArenaShow(s32 only) {
    s32 i;

    for (i = 0; i < SIDES; i++) {
        if ((only == SIDES) || (only == i)) {
            sSides[i].model->unk_000.unk_01 |= 1;
        } else {
            sSides[i].model->unk_000.unk_01 &= ~1;
        }
    }
}

// 0 to 1 as t goes from 0 to 1, easing in and out, and 1 after
static f32 Randomizer_ArenaEase(f32 t) {
    if (t > 1.0f) {
        t = 1.0f;
    }
    return t * t * (3.0f - (2.0f * t));
}

/*
 * Where a side's head is (see POINT_HEAD), from the frame before: its point 7, or halfway up
 * to its point 11 when that's above it, as the top of the head is; its middle if it has none
 */
static void Randomizer_ArenaHead(RandomizerArenaSide* side, Vec3f* out) {
    Vec3f* head = func_80015390(side->model, POINT_HEAD, NULL);
    Vec3f* top = func_80015390(side->model, POINT_TOP, NULL);

    if (head == NULL) {
        *out = side->middle;
        return;
    }
    *out = *head;
    if ((top != NULL) && (top->y > head->y) &&
        ((SQ(top->x - head->x) + SQ(top->z - head->z)) < SQ(side->height * 0.5f))) {
        func_8000E88C(out, (head->x + top->x) * 0.5f, (head->y + top->y) * 0.5f, (head->z + top->z) * 0.5f);
    }
}

/*
 * Where a side's Pokemon is: the middle of the points its model marked as it was drawn
 * (see POINT_HEAD), followed smoothly; until it's drawn, the estimate from its card. Some
 * fly their idle animation far above where the battle puts them: Pidgeotto's body is 80 to
 * 120 units up, its card's middle 27.
 */
static void Randomizer_ArenaTrack(RandomizerArenaSide* side) {
    unk_D_86002F58_004_000* model = side->model;
    Vec3f lo;
    Vec3f hi;
    s32 i;

    if (model->unk_0A7 == 0) {
        return;
    }
    lo = hi = model->unk_0A8[0].unk_04;
    for (i = 1; i < model->unk_0A7; i++) {
        Vec3f* point = &model->unk_0A8[i].unk_04;

        lo.x = MIN(lo.x, point->x);
        lo.y = MIN(lo.y, point->y);
        lo.z = MIN(lo.z, point->z);
        hi.x = MAX(hi.x, point->x);
        hi.y = MAX(hi.y, point->y);
        hi.z = MAX(hi.z, point->z);
    }
    lo.x = (lo.x + hi.x) * 0.5f;
    lo.y = (lo.y + hi.y) * 0.5f;
    lo.z = (lo.z + hi.z) * 0.5f;
    if (!side->tracked) {
        side->middle = lo;
        side->tracked = TRUE;
    }
    side->middle.x += (lo.x - side->middle.x) * TRACK_FOLLOW;
    side->middle.y += (lo.y - side->middle.y) * TRACK_FOLLOW;
    side->middle.z += (lo.z - side->middle.z) * TRACK_FOLLOW;
}

// Follows a side's head (sFocus): from where it is the first time, then catching up with it, so
// that the camera doesn't shake
static void Randomizer_ArenaFollow(RandomizerArenaSide* side) {
    Vec3f point;

    Randomizer_ArenaHead(side, &point);
    if (!sFocusSet) {
        sFocus = point;
        sFocusSet = TRUE;
    }
    sFocus.x += (point.x - sFocus.x) * FACE_FOLLOW;
    sFocus.y += (point.y - sFocus.y) * FACE_FOLLOW;
    sFocus.z += (point.z - sFocus.z) * FACE_FOLLOW;
}

// The middle of the field, between the two
static void Randomizer_ArenaCentre(Vec3f* centre) {
    func_8000E88C(centre, 0.0f, (sSides[0].middle.y + sSides[1].middle.y) * 0.5f, 0.0f);
}

// How far the camera is to show the whole field
static f32 Randomizer_ArenaFieldDistance(void) {
    return ((sSides[1].middle.x - sSides[0].middle.x) + MAX(sSides[0].height, sSides[1].height)) * PAN_FRAMING;
}

// A shot's view on one of its frames (all of them but the split screen's)
static void Randomizer_ArenaCompose(RandomizerArenaView* view, s32 shot, s32 frame) {
    RandomizerArenaSide* side = &sSides[sShotSide];
    RandomizerArenaSide* other = &sSides[sShotSide ^ 1];
    s16 facing = (sShotSide == 0) ? 0x4000 : -0x4000; // the yaw from which the camera sees its face
    f32 dir = (sShotSide == 0) ? 1.0f : -1.0f;        // the way it faces along x
    f32 idle = Randomizer_ArenaEase((f32)frame / IDLE_FRAMES);
    f32 move = Randomizer_ArenaEase((f32)frame / MOVE_FRAMES);
    f32 distance;
    f32 start;
    s16 yaw;
    Vec3f point;
    Vec3f eye;

    switch (shot) {
        case SHOT_PAN:
            Randomizer_ArenaCentre(&point);
            Randomizer_ArenaAimFrom(view, &point, Randomizer_ArenaFieldDistance(), PAN_PITCH,
                                    sShotYaw + (frame * PAN_TURN * sShotTurn), 0);
            break;

        case SHOT_OVERHEAD:
            // From in front or behind, give or take 22 degrees, so that they're side by side
            // across the screen (one above the other, the nearer one went off the bottom)
            Randomizer_ArenaCentre(&point);
            Randomizer_ArenaAimFrom(view, &point, Randomizer_ArenaFieldDistance() * 1.2f, OVERHEAD_PITCH,
                                    (sShotYaw & 0x8000) + (sShotYaw & 0x1FFF) - 0x1000 +
                                        (frame * OVERHEAD_TURN * sShotTurn),
                                    0);
            break;

        case SHOT_ORBIT:
            Randomizer_ArenaAimFrom(view, &side->middle, Randomizer_ArenaDistance(side, ORBIT_FRAMING), ORBIT_PITCH,
                                    sShotYaw + (frame * ORBIT_TURN * sShotTurn), 0);
            break;

        case SHOT_SIDE:
            // Low, from the side of its front, the camera and what it looks at drifting sideways
            // together, so the Pokemon slides across the screen
            distance = Randomizer_ArenaDistance(side, 2.6f);
            yaw = facing + (sShotTurn * 0x3000);
            point = side->middle;
            point.x += COSS(yaw) * side->height * 0.8f * (0.5f - idle) * sShotTurn;
            point.z -= SINS(yaw) * side->height * 0.8f * (0.5f - idle) * sShotTurn;
            Randomizer_ArenaAimLevel(view, &point, distance, MAX(GROUND_EYE, side->middle.y - (side->height * 0.2f)),
                                     yaw, sShotRoll / 2);
            break;

        case SHOT_CHASE:
            // From behind one, over its shoulder, up to in front of the other: along the line
            // between them, from beside the first one to straight in front of the other
            distance = Randomizer_ArenaDistance(other, 2.8f);
            if (distance > (other->middle.x - side->middle.x) * dir * 0.6f) {
                distance = (other->middle.x - side->middle.x) * dir * 0.6f;
            }
            idle = Randomizer_ArenaEase((f32)frame / (IDLE_FRAMES * CHASE_MOVE));
            start = side->middle.x - (dir * (Randomizer_ArenaDistance(side, 1.5f) + 60.0f));
            eye.x = start + (((other->middle.x - (dir * distance)) - start) * idle);
            eye.z = MAX(side->width * 1.6f, side->height * 0.7f) * sShotTurn * (1.0f - (0.7f * idle));
            start = side->middle.y + (side->height * 0.6f) + 20.0f;
            eye.y = start + (((other->middle.y + (distance * 0.1f)) - start) * idle);
            Randomizer_ArenaAim(view, &eye, &other->middle, 0);
            break;

        case SHOT_LOW:
            // From the ground in front of it, to one side, looking up, pushing in and going round
            Randomizer_ArenaAimLevel(view, &side->middle, Randomizer_ArenaDistance(side, 2.3f) * (1.15f - (0.25f * move)),
                                     GROUND_EYE, facing + (sShotSlant * sShotTurn) + (frame * 0x10 * sShotTurn),
                                     sShotRoll);
            break;

        case SHOT_HIGH:
            // From above, in front of it to one side, coming down
            Randomizer_ArenaAimLevel(view, &side->middle, Randomizer_ArenaDistance(side, 1.5f),
                                     side->middle.y + (side->height * (1.9f - (0.5f * move))) + 20.0f,
                                     facing + (sShotSlant * sShotTurn), 0);
            break;

        case SHOT_PUSH:
            // In front of it, about its middle's height, pushing in fast
            Randomizer_ArenaAimLevel(view, &side->middle, Randomizer_ArenaDistance(side, 2.9f - (0.8f * move)),
                                     side->middle.y + (side->height * 0.1f), facing + ((sShotSlant / 2) * sShotTurn),
                                     sShotRoll / 2);
            break;

        case SHOT_SHOULDER:
            // From behind it, over its shoulder, at the other, moving in a little
            eye.x = side->middle.x - (dir * (Randomizer_ArenaDistance(side, 1.2f) + 40.0f)) +
                    (dir * move * (other->middle.x - side->middle.x) * dir * 0.15f);
            eye.y = side->middle.y + (side->height * 0.6f) + 10.0f;
            eye.z = MAX(side->width * 1.6f, side->height * 0.7f) * sShotTurn; // clear of long ones' tails
            Randomizer_ArenaAim(view, &eye, &other->middle, sShotRoll);
            break;

        case SHOT_GAZE:
        case SHOT_FACE:
            // Close on its head, from the side it's on (in front, or behind for a Snorlax lying
            // on its back), a little to one side, from below, level or above, the camera
            // turning after the head as it moves (smoothly, so it doesn't shake), and coming
            // slowly closer
            if (!sFocusSet) {
                Randomizer_ArenaHead(side, &point);
                sFocusYaw = facing;
                if ((SQ(point.x - side->middle.x) + SQ(point.z - side->middle.z)) > SQ(side->height * 0.2f)) {
                    func_800102A4(&side->middle, &point, &start, &yaw, &sFocusYaw);
                }
            }
            Randomizer_ArenaFollow(side);
            distance = MAX(side->height * FACE_FRAMING, FACE_MIN) * (1.1f - (0.15f * idle));
            func_80010354(&sFocus, &eye, distance, sShotPitch, sFocusYaw + (sShotSlant * sShotTurn));
            if (eye.y < GROUND_EYE) {
                eye.y = GROUND_EYE;
            }
            Randomizer_ArenaAim(view, &eye, &sFocus, sShotRoll / 2);
            break;

        case SHOT_TRACK:
            // Beside it, a little in front, the camera moving with it (its middle, followed
            // smoothly), so that it stays where it is on the screen as it lunges or jumps, as
            // Colosseum and XD film attacks
            distance = Randomizer_ArenaDistance(side, ACTION_FRAMING);
            func_8000E88C(&eye, side->middle.x + (dir * distance * 0.35f), side->middle.y + (distance * 0.15f),
                          side->middle.z + (distance * 0.9f * sShotTurn));
            point = side->middle;
            point.x += dir * side->height * 0.3f; // room in front of it
            Randomizer_ArenaAim(view, &eye, &point, sShotRoll / 2);
            break;

        case SHOT_GAMEBOY:
        case SHOT_RBY:
            // The Game Boy games' view of a battle (Red, Blue and Yellow): behind the one on the
            // left (the player's in the battle) and to its right, its back at the bottom left,
            // the other far off at the top right; then, quickly, zooming in on the other, which
            // comes to the middle under the logo, easing in and out. Zoomed in, the field of
            // view is narrower (start is the tangent of half of it), and so are the angles that
            // put the other where it is on the screen
            side = &sSides[0];
            other = &sSides[1];
            func_8000E88C(&eye, side->middle.x - (((side->height * 0.6f) + 40.0f) * RBY_FAR),
                          side->middle.y + (((side->height * 0.6f) + 10.0f) * RBY_FAR),
                          ((side->height * 0.8f) + 30.0f) * RBY_FAR);
            distance = sqrtf(SQ(other->middle.x - eye.x) + SQ(other->middle.y - eye.y) + SQ(other->middle.z - eye.z));
            start = other->height * RBY_ZOOM_FRAMING / distance;
            start = CLAMP(start, RBY_ZOOM_MIN, TAN_HALF_FOVY);
            move = Randomizer_ArenaEase((f32)MAX(frame - RBY_HOLD, 0) / RBY_ZOOM_FRAMES);
            start = TAN_HALF_FOVY + ((start - TAN_HALF_FOVY) * move);
            Randomizer_ArenaAimAt(view, &eye, &other->middle,
                                  (RBY_RAISE + ((LOOK_RAISE - RBY_RAISE) * move)) * start / TAN_HALF_FOVY,
                                  RBY_LATERAL * (1.0f - move) * start / TAN_HALF_FOVY, 0);
            view->fovy = RAD_TO_FOVY(start / (1.0f + (0.28f * start * start))); // atan, closely enough
            break;

        case SHOT_SIGHTS:
        case SHOT_AIM:
            // As a game played over the shoulder aims: close behind its head, a little above it
            // and to its right, its back big at the left of the screen, at the other, right of
            // the middle under the logo; following the head and pushing in a little
            Randomizer_ArenaFollow(side);
            distance = ((side->height * AIM_BACK) + AIM_BACK_MIN) * (1.0f - (0.15f * idle));
            func_8000E88C(&eye, sFocus.x - (dir * distance), sFocus.y + (side->height * AIM_ABOVE),
                          sFocus.z + (dir * distance * AIM_BESIDE));
            Randomizer_ArenaAimAt(view, &eye, &other->middle, AIM_RAISE, AIM_LATERAL, 0);
            break;

        case SHOT_HIT:
            // Close in front of it to one side, tilted, pulling back
            Randomizer_ArenaAimLevel(view, &side->middle,
                                     Randomizer_ArenaDistance(side, 2.4f) *
                                         (0.95f + (0.2f * Randomizer_ArenaEase(frame / 45.0f))),
                                     side->middle.y, facing + (0x1400 * sShotTurn), sShotRoll);
            break;

        case SHOT_HIT_LOW:
            // From the ground, farther and more to the side, tilted
            Randomizer_ArenaAimLevel(view, &side->middle,
                                     Randomizer_ArenaDistance(side, 3.4f) *
                                         (1.0f + (0.1f * Randomizer_ArenaEase(frame / 45.0f))),
                                     GROUND_EYE, facing + (0x2000 * sShotTurn), sShotRoll);
            break;
    }
}

// The split screen: the one on the left in the left half and the other in the right, each seen
// from in front and to the side so that they face each other across the middle
static void Randomizer_ArenaSplit(s32 frame) {
    RandomizerArenaView view;

    Randomizer_ArenaShow(0);
    Randomizer_ArenaAimFrom(&view, &sSides[0].middle, Randomizer_ArenaDistance(&sSides[0], SPLIT_FRAMING), SPLIT_PITCH,
                            0x2000 + (frame * SPLIT_TURN), 0);
    Randomizer_ArenaDrawView(0, SCREEN_W / 2, &view);
    Randomizer_ArenaShow(1);
    Randomizer_ArenaAimFrom(&view, &sSides[1].middle, Randomizer_ArenaDistance(&sSides[1], SPLIT_FRAMING), SPLIT_PITCH,
                            -0x2000 - (frame * SPLIT_TURN), 0);
    Randomizer_ArenaDrawView(SCREEN_W / 2, SCREEN_W / 2, &view);
    Randomizer_ArenaShow(SIDES);

    // A line down the middle between them
    gDPPipeSync(gDisplayListHead++);
    gDPSetCycleType(gDisplayListHead++, G_CYC_FILL);
    gDPSetRenderMode(gDisplayListHead++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetFillColor(gDisplayListHead++, (GPACK_RGBA5551(0, 0, 0, 1) << 16) | GPACK_RGBA5551(0, 0, 0, 1));
    gDPFillRectangle(gDisplayListHead++, (SCREEN_W / 2) - 2, 0, (SCREEN_W / 2) + 1, SCREEN_H - 1);
    gDPPipeSync(gDisplayListHead++);
}

// A number from -1 to 1
static f32 Randomizer_ArenaWobble(void) {
    return (Randomizer_ArenaBelow(201) - 100) / 100.0f;
}

// The shot of the moment, drawn
static void Randomizer_ArenaShoot(void) {
    RandomizerArenaView view;
    RandomizerArenaView next;
    f32 ease;
    f32 size;

    if (sShot == SHOT_SPLIT) {
        Randomizer_ArenaSplit(sShotFrame);
        return;
    }

    if (sShot == SHOT_WHIP) {
        // Swinging round from where the camera was to the defender's shot, without moving
        Randomizer_ArenaCompose(&next, sNextShot, 0);
        ease = Randomizer_ArenaEase((f32)sShotFrame / WHIP_FRAMES);
        view.eye = sLastView.eye;
        func_8000E88C(&view.at, sLastView.at.x + ((next.at.x - sLastView.at.x) * ease),
                      sLastView.at.y + ((next.at.y - sLastView.at.y) * ease),
                      sLastView.at.z + ((next.at.z - sLastView.at.z) * ease));
        view.roll = sLastView.roll + (s16)((next.roll - sLastView.roll) * ease);
        view.fovy = sLastView.fovy + ((next.fovy - sLastView.fovy) * ease);
    } else {
        Randomizer_ArenaCompose(&view, sShot, sShotFrame);
        sLastView = view;
    }

    // Shaking, less and less, as a hit lands
    if (sShake > 0) {
        size = sqrtf(SQ(view.at.x - view.eye.x) + SQ(view.at.y - view.eye.y) + SQ(view.at.z - view.eye.z)) *
               SHAKE_SIZE * sShake / SHAKE_FRAMES;
        view.at.x += Randomizer_ArenaWobble() * size;
        view.at.y += Randomizer_ArenaWobble() * size;
        view.eye.x += Randomizer_ArenaWobble() * size * 0.5f;
        view.eye.y += Randomizer_ArenaWobble() * size * 0.5f;
        sShake--;
    }

    Randomizer_ArenaDrawView(0, SCREEN_W, &view);
}

// Moves the shot on a frame: the swing to the defender ends in its shot, and a shot between
// turns gives way to another after a while
static void Randomizer_ArenaAdvance(void) {
    sShotFrame++;
    if ((sShot == SHOT_WHIP) && (sShotFrame >= WHIP_FRAMES)) {
        sShot = sNextShot;
        sShotFrame = 0;
        sShake = SHAKE_FRAMES;
    } else if ((sShot < NUM_IDLE_SHOTS) && (sShotFrame >= IDLE_FRAMES)) {
        Randomizer_ArenaIdleCut();
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
    Randomizer_ArenaTrack(&sSides[0]);
    Randomizer_ArenaTrack(&sSides[1]);

    // The models' animations move on a frame (func_80015348), once even when the scene is
    // drawn twice
    func_80015348();
    func_800079C4();
    Randomizer_ArenaSky();
    Randomizer_ArenaShoot();
    gSPDisplayList(gDisplayListHead++, D_8006F630);
    Randomizer_ArenaAdvance();
}

// Every frame, over the title picture and under the logo and "PRESS START"
void Randomizer_ArenaDraw(void) {
    if (sReady == 1) {
        Randomizer_CallOnStack(Randomizer_ArenaRender, sStackTop);
    }
}

#endif
