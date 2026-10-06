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
 * The camera's shots are src/randomizer_shots.h's, which real battles film their attacks with
 * too, after the way Pokemon Battle Revolution films its battles (its first trailer, 2006):
 * the camera follows the fight, a new shot every two or three seconds, from the ground up to
 * high above, some of them tilted, and every one of them moving.
 *  - Between turns, a shot of the field or of one of them, a new one every IDLE_FRAMES.
 *  - WINDUP_FRAMES before a turn's attack, the attacker.
 *  - As the hit lands (the defender's reaction starts), the defender, close and tilted, the
 *    camera shaking; reached by a cut or a quick swing of the camera from the attacker.
 * The logo and the subtitle cover the top third of the screen, so the camera looks at a point
 * above what it films on the screen, by SHOT_LOOK_RAISE of its distance (about 9 degrees),
 * which puts that about 40 pixels under the middle, in the middle of the rest.
 */
#define SHOT_LOOK_RAISE 0.15f
#define SHOT_BELOW(n) Randomizer_ArenaBelow(n)
static s32 Randomizer_ArenaBelow(s32 n);
#include "src/randomizer_shots.h"

#define CAMERA_NEAR 10.0f // the battle's (func_8431AFD0)
#define CAMERA_FAR 12800.0f
#define WINDUP_FRAMES 24  // the attacker's shot starts this long before its attack
#define WHIP_FRAMES 8
#define SHAKE_FRAMES 14
#define SHAKE_SIZE 0.025f // of the camera's distance

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

// Its own stack, for the setup and the drawing (randomizer_title_stack.s)
#define STACK_SIZE 0x4000
void Randomizer_CallOnStack(void (*func)(void), void* stackTop);

typedef struct RandomizerArenaSide {
    /* 0x00 */ RandomizerShotSide shot; // where it is in the arena and how big, as the shots film it
    /* 0x18 */ s16 species;
    /* 0x1A */ s16 lastFrame; // of the attack or hit reaction playing, to see it end
    /* 0x1C */ u8 idle;
    /* 0x1D */ u8 hit;
    /* 0x1E */ u8 busy; // playing an attack or the hit reaction
    /* 0x1F */ u8 numAttacks;
    /* 0x20 */ u8 attacks[MAX_ATTACKS];
    /* 0x28 */ u8 beam;    // its Hyper Beam animation
    /* 0x29 */ u8 tracked; // its middle found from its model's points (Randomizer_ShotTrack)
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

static RandomizerShot sShot;
static u8 sIdleShot; // the last of the shots between turns
static u8 sNextShot; // after a swing of the camera
static s16 sShake;   // frames of shaking left
static RandomizerShotView sLastView; // the last frame's, for the swing

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
    s32 count = side->shot.model->unk_000.unk_0C->unk_28(0, 0)->unk_04; // how many the model has
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
    func_8001BD04(side->shot.model, anim);
    func_80017464(side->shot.model, 0);
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

    side->shot.model = model;
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
    side->shot.width = ((f32*)sTable)[0];
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
    side->shot.height = CARD_HEIGHT / cardScale;
    func_8000E88C(&side->shot.middle, x, y - ((f32)((s16)(info->unk_14 >> 6) >> 4) / cardScale), 0.0f);

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
    Randomizer_ShotCut(&sShot, shot, side);
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
    s32 whip = (sShot.kind >= SHOT_LOW) && Randomizer_ArenaBelow(2);

#ifdef ARENA_TEST_HIT_SHOT
    shot = ARENA_TEST_HIT_SHOT;
#endif
#ifdef ARENA_TEST_WHIP
    whip = (sShot.kind >= SHOT_LOW) && ARENA_TEST_WHIP;
#endif
    Randomizer_ArenaCut(shot, sAttacker ^ 1);
    sShot.roll = (0x500 + Randomizer_ArenaBelow(0x400)) * sShot.turn; // 7 to 12 degrees
    if (whip) {
        sNextShot = shot;
        sShot.kind = SHOT_WHIP;
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
    sShot.roll = (0x500 + Randomizer_ArenaBelow(0x400)) * sShot.turn;
    sShake = SHAKE_FRAMES;
    sReplay++;
    sReplayFrame = 0;
}

// When the title screen starts
void Randomizer_ArenaStart(void) {
    sReady = 0;
    sRandomState = Randomizer_ArenaSeed();
#ifdef ARENA_TEST_SEED
    sRandomState = ARENA_TEST_SEED;
#endif
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
    // What was picked, over gRandomizerState's battle camera hook (0x80000358; a battle sets
    // it again as it starts) and the byte after it, for reading from an
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
    s32 frame = side->shot.model->unk_040.unk_08 >> 16;

    return (frame < 0) ? 0 : frame;
}

// How far through its animation a side is, in percent
static s32 Randomizer_ArenaPercent(RandomizerArenaSide* side) {
    s32 frames = side->shot.model->unk_040.unk_04->unk_0A;

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
    if (func_80017514(side->shot.model) || (frame < side->lastFrame)) {
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
                    func_80017464(attacker->shot.model, MAX(sImpact - REPLAY_LEAD, 0));
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

/*
 * Draws the scene from a view, into a part of the screen x to x + w wide. The near and far
 * clipping planes are the battle's: the arenas' fog is set for them, and the fog comes out
 * thicker or thinner with the near plane. The camera's up (unk_60.up) tilts it.
 */
static void Randomizer_ArenaDrawView(s32 x, s32 w, RandomizerShotView* view) {
    Randomizer_ShotUp(view, &sCamera.unk_60.up);
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
            sSides[i].shot.model->unk_000.unk_01 |= 1;
        } else {
            sSides[i].shot.model->unk_000.unk_01 &= ~1;
        }
    }
}

// The middle of the field, between the two
static void Randomizer_ArenaCentre(Vec3f* centre) {
    func_8000E88C(centre, 0.0f, (sSides[0].shot.middle.y + sSides[1].shot.middle.y) * 0.5f, 0.0f);
}

// How far the camera is to show the whole field
static f32 Randomizer_ArenaFieldDistance(void) {
    return ((sSides[1].shot.middle.x - sSides[0].shot.middle.x) + MAX(sSides[0].shot.height, sSides[1].shot.height)) * PAN_FRAMING;
}

// A shot's view on one of its frames (all of them but the split screen's)
static void Randomizer_ArenaCompose(RandomizerShotView* view, s32 shot, s32 frame) {
    RandomizerArenaSide* side = &sSides[sShot.side];
    RandomizerArenaSide* other = &sSides[sShot.side ^ 1];
    s16 facing = (sShot.side == 0) ? 0x4000 : -0x4000; // the yaw from which the camera sees its face
    f32 dir = (sShot.side == 0) ? 1.0f : -1.0f;        // the way it faces along x
    f32 idle = Randomizer_ShotEase((f32)frame / IDLE_FRAMES);
    f32 distance;
    f32 start;
    s16 yaw;
    Vec3f point;
    Vec3f eye;

    switch (shot) {
        case SHOT_PAN:
            Randomizer_ArenaCentre(&point);
            Randomizer_ShotAimFrom(view, &point, Randomizer_ArenaFieldDistance(), PAN_PITCH,
                                    sShot.yaw + (frame * PAN_TURN * sShot.turn), 0);
            break;

        case SHOT_OVERHEAD:
            // From in front or behind, give or take 22 degrees, so that they're side by side
            // across the screen (one above the other, the nearer one went off the bottom)
            Randomizer_ArenaCentre(&point);
            Randomizer_ShotAimFrom(view, &point, Randomizer_ArenaFieldDistance() * 1.2f, OVERHEAD_PITCH,
                                    (sShot.yaw & 0x8000) + (sShot.yaw & 0x1FFF) - 0x1000 +
                                        (frame * OVERHEAD_TURN * sShot.turn),
                                    0);
            break;

        case SHOT_ORBIT:
            Randomizer_ShotAimFrom(view, &side->shot.middle, Randomizer_ShotDistance(&side->shot, ORBIT_FRAMING), ORBIT_PITCH,
                                    sShot.yaw + (frame * ORBIT_TURN * sShot.turn), 0);
            break;

        case SHOT_SIDE:
            // Low, from the side of its front, the camera and what it looks at drifting sideways
            // together, so the Pokemon slides across the screen
            distance = Randomizer_ShotDistance(&side->shot, 2.6f);
            yaw = facing + (sShot.turn * 0x3000);
            point = side->shot.middle;
            point.x += COSS(yaw) * side->shot.height * 0.8f * (0.5f - idle) * sShot.turn;
            point.z -= SINS(yaw) * side->shot.height * 0.8f * (0.5f - idle) * sShot.turn;
            Randomizer_ShotAimLevel(view, &point, distance, MAX(GROUND_EYE, side->shot.middle.y - (side->shot.height * 0.2f)),
                                     yaw, sShot.roll / 2);
            break;

        case SHOT_CHASE:
            // From behind one, over its shoulder, up to in front of the other: along the line
            // between them, from beside the first one to straight in front of the other
            distance = Randomizer_ShotDistance(&other->shot, 2.8f);
            if (distance > (other->shot.middle.x - side->shot.middle.x) * dir * 0.6f) {
                distance = (other->shot.middle.x - side->shot.middle.x) * dir * 0.6f;
            }
            idle = Randomizer_ShotEase((f32)frame / (IDLE_FRAMES * CHASE_MOVE));
            start = side->shot.middle.x - (dir * (Randomizer_ShotDistance(&side->shot, 1.5f) + 60.0f));
            eye.x = start + (((other->shot.middle.x - (dir * distance)) - start) * idle);
            eye.z = MAX(side->shot.width * 1.6f, side->shot.height * 0.7f) * sShot.turn * (1.0f - (0.7f * idle));
            start = side->shot.middle.y + (side->shot.height * 0.6f) + 20.0f;
            eye.y = start + (((other->shot.middle.y + (distance * 0.1f)) - start) * idle);
            Randomizer_ShotAim(view, &eye, &other->shot.middle, 0);
            break;

        default:
            // The attacker's and the defender's, as a real battle's (src/randomizer_shots.h)
            Randomizer_ShotCompose(view, shot, &sShot, frame, &side->shot, &other->shot, &sSides[0].shot,
                                   &sSides[1].shot);
            break;
    }
}

// The split screen: the one on the left in the left half and the other in the right, each seen
// from in front and to the side so that they face each other across the middle
static void Randomizer_ArenaSplit(s32 frame) {
    RandomizerShotView view;

    Randomizer_ArenaShow(0);
    Randomizer_ShotAimFrom(&view, &sSides[0].shot.middle, Randomizer_ShotDistance(&sSides[0].shot, SPLIT_FRAMING), SPLIT_PITCH,
                            0x2000 + (frame * SPLIT_TURN), 0);
    Randomizer_ArenaDrawView(0, SCREEN_W / 2, &view);
    Randomizer_ArenaShow(1);
    Randomizer_ShotAimFrom(&view, &sSides[1].shot.middle, Randomizer_ShotDistance(&sSides[1].shot, SPLIT_FRAMING), SPLIT_PITCH,
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
    RandomizerShotView view;
    RandomizerShotView next;
    f32 ease;
    f32 size;

    if (sShot.kind == SHOT_SPLIT) {
        Randomizer_ArenaSplit(sShot.frame);
        return;
    }

    if (sShot.kind == SHOT_WHIP) {
        // Swinging round from where the camera was to the defender's shot, without moving
        Randomizer_ArenaCompose(&next, sNextShot, 0);
        ease = Randomizer_ShotEase((f32)sShot.frame / WHIP_FRAMES);
        view.eye = sLastView.eye;
        func_8000E88C(&view.at, sLastView.at.x + ((next.at.x - sLastView.at.x) * ease),
                      sLastView.at.y + ((next.at.y - sLastView.at.y) * ease),
                      sLastView.at.z + ((next.at.z - sLastView.at.z) * ease));
        view.roll = sLastView.roll + (s16)((next.roll - sLastView.roll) * ease);
        view.fovy = sLastView.fovy + ((next.fovy - sLastView.fovy) * ease);
    } else {
        Randomizer_ArenaCompose(&view, sShot.kind, sShot.frame);
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
    sShot.frame++;
    if ((sShot.kind == SHOT_WHIP) && (sShot.frame >= WHIP_FRAMES)) {
        sShot.kind = sNextShot;
        sShot.frame = 0;
        sShake = SHAKE_FRAMES;
    } else if ((sShot.kind < NUM_IDLE_SHOTS) && (sShot.frame >= IDLE_FRAMES)) {
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

#ifdef ARENA_TEST_POINTS
// Each side's points (see POINT_HEAD) from the root of its model, for reading from an
// emulator savestate: their address at 0x80000360
typedef struct RandomizerTestPoint {
    u16 id;
    s16 x;
    s16 y;
    s16 z;
} RandomizerTestPoint;

static struct {
    u32 magic;
    u8 species[SIDES];
    u8 count[SIDES];
    RandomizerTestPoint points[SIDES][12];
} sTestPoints;

static void Randomizer_ArenaTestPoints(void) {
    s32 i;
    s32 j;

    for (i = 0; i < SIDES; i++) {
        unk_D_86002F58_004_000* model = sSides[i].shot.model;

        sTestPoints.species[i] = sSides[i].species;
        sTestPoints.count[i] = model->unk_0A7;
        for (j = 0; (j < model->unk_0A7) && (j < 12); j++) {
            sTestPoints.points[i][j].id = model->unk_0A8[j].unk_00;
            sTestPoints.points[i][j].x = model->unk_0A8[j].unk_04.x - model->unk_024.x;
            sTestPoints.points[i][j].y = model->unk_0A8[j].unk_04.y - model->unk_024.y;
            sTestPoints.points[i][j].z = model->unk_0A8[j].unk_04.z - model->unk_024.z;
        }
    }
    sTestPoints.magic = 0x50545331;
    *(u32*)0x80000360 = (u32)&sTestPoints;
}
#endif

// The scene's drawing, on its own stack
static void Randomizer_ArenaRender(void) {
    Randomizer_ArenaTurns();
    Randomizer_ShotTrack(&sSides[0].shot, &sSides[0].tracked);
    Randomizer_ShotTrack(&sSides[1].shot, &sSides[1].tracked);

    // The models' animations move on a frame (func_80015348), once even when the scene is
    // drawn twice
    func_80015348();
    func_800079C4();
    Randomizer_ArenaSky();
    Randomizer_ArenaShoot();
    gSPDisplayList(gDisplayListHead++, D_8006F630);
    Randomizer_ArenaAdvance();
#ifdef ARENA_TEST_POINTS
    Randomizer_ArenaTestPoints();
#endif
}

// Every frame, over the title picture and under the logo and "PRESS START"
void Randomizer_ArenaDraw(void) {
    if (sReady == 1) {
        Randomizer_CallOnStack(Randomizer_ArenaRender, sStackTop);
    }
}

#endif
