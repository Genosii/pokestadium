/*
 * The randomizer's battle camera (in randomizer_battleui, see randomizer_battle_ui.h): with
 * Options' "Battle camera" on Custom, about half of the ordinary attacks are filmed with the
 * shots the title screen films its demo battles with (src/randomizer_shots.h) instead of by
 * the battle's own camera: the attacker as its attack starts, then the defender as the hit
 * lands, reached by a cut or a quick swing of the camera, the camera shaking (or, if the
 * attack's shot already ends on the defender, in it). A critical hit or a Hyper Beam is
 * always filmed, after Groudon's Hyper Beam in Pokemon Battle Revolution's first trailer: its
 * hit lands, then the whole attack plays twice more from behind the attacker, its effects
 * too, landing again from other angles, while the battle waits (the damage, the HP bar and
 * the message come once). Between the menus and a turn, the camera moves to both Pokemon and
 * on into the battle's camera in place of the black jagged wipe, unless that hides something.
 * The battle films the rest itself, and everything else: the turns' openings, the moves with
 * cameras of their own, switching and fainting.
 *
 * The battle's camera director (func_8432ADD8, see docs/game-engine.md, "The battle's
 * camera") moves the camera every frame and decides when a step of an attack ends by where
 * the camera is, so it mustn't see this one: after it has moved the camera, the hook
 * (randomizer_battle_ui_stub.s) keeps where it put it and puts this file's view in its place
 * for the drawing, and once the scene is drawn puts the battle's back.
 *
 * Built only with RANDOMIZER=1; empty otherwise so the default build still matches.
 */
#include "randomizer_battle_ui.h"

#ifdef RANDOMIZER

#include "src/11BA0.h"
#include "src/17300.h"
#include "src/1C720.h"
#include "src/6BC0.h"
#include "src/memmap.h"
#include "src/randomizer_state.h"

#define SHOT_LOOK_RAISE 0.05f // of the distance: what the camera films a little under the middle
#define SHOT_BELOW(n) Randomizer_BattleCameraBelow(n)
static s32 Randomizer_BattleCameraBelow(s32 n);
#include "src/randomizer_shots.h"

// The director's state (D_84390240.unk_00), as measured
#define CAMERA_MODE_MENUS 1 // unk_1C: between turns, the menus
#define CAMERA_MODE_MOVE 2 // a move
#define OUTCOME_CRITICAL 2 // unk_1A: set with the "Critical hit!" message (func_84371080), or a one-hit KO's
#define SCRIPT_ATTACK 2    // unk_38: an ordinary attack's camera script, or the other one
#define SCRIPT_ATTACK_ALT 3
#define STEP_ATTACKER 0  // unk_20: the camera going to the attacker,
#define STEP_ANIMATION 1 // its attack playing,
#define STEP_HIT 2       // the defender reacting to the hit

// A Pokemon's actions (func_84305760, unk_4C0): what it does once the hit lands (func_84323FA0)
#define ACTION_AFTER_HIT 4

#define SIDES 2
#define CRITICAL 1   // unk_654.unk_38.unk_5B: the last hit it took was a critical one
#define FAINTED 0x10 // unk_654.unk_2D: a Pokemon that has fainted,
#define FAINTED_ALT 0x13
#define UNDERGROUND 0x4000 // unk_654.unk_34: dug in, which the battle draws see-through (unk_01D 0)
#define OPAQUE 0xFF
#define FILM_CHANCE 2    // one ordinary attack in this many is filmed by this file
#define HYPER_BEAM 63
#define REPLAY_TIMES 3       // a critical hit or a Hyper Beam lands this many times, from as many angles,
#define REPLAY_HIT_FRAMES 40 // each time for at least this long before it lands again
#define AIM_EARLY 30 // over the shoulder, the hit comes this many frames before the attack's animation ends
#define WHIP_FRAMES 8
#define SHAKE_FRAMES 14
#define SHAKE_SIZE 0.025f // of the camera's distance
/*
 * The wipe between the menus and a turn (func_84329B04, the director's unk_30 when its unk_2E
 * isn't 0): asked for at step 1 (both having picked, the menus sliding off), it starts closing
 * at step 3, the battle's camera is on what comes next from step 9, as it opens, and it's over
 * at 0
 */
#define WIPE_ASKED 1
#define WIPE_OPENING 9
#define MENU_READY 3 // unk_654.unk_10: the player has picked (the box reads "Ready!")
/*
 * unk_654.unk_34: what the battle swaps or moves while the wipe hides the screen
 * (func_843062F0, func_84306218, func_8431FAB4, func_8431F998): a substitute (0x800, its
 * doll's model loaded in or out), in the air (4, 8, 0x200, 0x400) or underground (0x4000)
 */
#define WIPE_HIDES 0x4E0C
#define WIPE_POSES 0x1F6D0 // unk_4B4: the poses func_843066E0 puts back then, 4, 6, 7, 9, 10 and 12 to 16
#define MOVE_IN_FRAMES 50  // the camera going from where it was to the first to attack or both,
#define MOVE_OUT_FRAMES 45 // and from them into the battle's camera, as the wipe would open
#define BOTH_PITCH 0x900   // looking down on both by about 12 degrees,
#define OPENING_FRAMING 2.6f // the first to attack, from in front and to the side, this far from it,
#define OPENING_SLANT 0x1800 // this far round from its front (34 degrees)
/*
 * The camera stays on one side of the line between the two for the whole battle, the side from
 * which the player's Pokemon is on the left: z above 0 (they stand on z 0). A view from the
 * other side, this file's or the battle's, is drawn from where it is mirrored across the line.
 */
#define FIELD_LINE_Z 0.0f
#define HANDOVER_FRAMES 24 // a cut to the battle's camera turned into a move this long
#define CUT_DISTANCE 80.0f // the battle's camera moving farther than this in a frame is a cut
#define SCREEN_W 320
#define SCREEN_H 240
/*
 * fragment62's layout never changes (it's spliced, mod-architecture.md), so its own variables
 * are where they were; two that it keeps to itself, the battle's sky (D_8438E780, as
 * func_84300340 draws it) and its scene (D_8438E784), come right after D_8438E778
 */
#define BATTLE_SKY (((void**)&D_8438E778)[2])
#define BATTLE_SCENE (((GraphNode**)&D_8438E778)[3])
#define SKY_COLOUR (*(Color_RGBA8_u32*)((u8*)&D_8438E778 + (0x8438E7A4 - 0x8438E778)))
#define SPLIT_LATERAL 0.09f    // of the distance: the split screen's Pokemon toward the line between the halves,
#define SPLIT_LEFT_RAISE 0.12f  // the left one lower,
#define SPLIT_RIGHT_RAISE -0.06f // the right one higher
#define IDLE_CHANCE 2 // one of the battle's cuts between shots in the menus in this many goes to one of the title's
#define CUT_AFTER_CHANGE 60 // which, this long after a change of its mode, script or attacker, is smoothed
#define BOTH_MARGIN 1.2f   // with this much room round them
#define ASPECT (4.0f / 3.0f)

/*
 * The size of a Pokemon: the rental card shows each species scaled by D_8006FF00's unk_02 /
 * 100 to about the same height, about 536 units (randomizer_title_arena.c)
 */
#define CARD_HEIGHT 536.0f

// The battle's camera, kept while this file's view is in its place
typedef struct RandomizerBattleCamera {
    /* 0x00 */ Vec3f eye;
    /* 0x0C */ Vec3f at;
    /* 0x18 */ Vec3f up;
    /* 0x24 */ f32 fovy;
} RandomizerBattleCamera; // size = 0x28

static RandomizerBattleCamera sBattleCamera;
static u8 sSwapped;
static RandomizerShotSide sSides[SIDES]; // the player's (left), the other (right)
static s16 sSpecies[SIDES];              // the one each side's middle is tracked for
static u8 sTracked[SIDES];
static u8 sShown[SIDES]; // whether the battle shows each side's Pokemon, kept while this file's view is drawn,
static u8 sAlpha[SIDES]; // and how see-through it draws it
static RandomizerShot sShot;
static u8 sNextShot; // after a swing of the camera
static s16 sShake;   // frames of shaking left
static RandomizerShotView sLastView; // the last frame's, for the swing
static u8 sAttack;   // an ordinary attack is going on,
static u8 sFilming;  // filmed by this file,
static u8 sSawAttacker; // the battle's camera having looked at the attacker,
static u8 sHitLanded;   // and then at the defender, or the attack at its hit step
static u8 sBigHit;      // a critical hit or a Hyper Beam,
static u8 sReplay;      // how many times its hit has landed (0 when it isn't being replayed),
static s16 sReplayFrame;
static unk_D_84390010* sAttacker;
static s32 sStep;  // of the attack, the last frame
static s32 sFrame; // frames since the battle started
static u32 sRandom;
static RandomizerShotView sDrawnView; // the view drawn the last frame, this file's or the battle's
static u8 sWiping;  // the battle's wipe is going on,
static u8 sMoving;  // and the camera moves in its place (MOVE_IN, then MOVE_OUT)
static u8 sMovedReady; // the move since the player picked has started
static s16 sMoveFrame;
static s16 sOutFrame;
static RandomizerShotView sMoveFrom;
static u8 sCustom;      // Options' "Battle camera" on Custom
static u8 sShowing;     // this file's view shows Pokemon the battle hides (Randomizer_BattleCameraShow)
static u8 sWasFilming;  // last frame
static s16 sHandover;   // frames left of a move into the battle's camera, or into an attack's shot,
static RandomizerShotView sHandFrom; // from this view
static RandomizerShotView sLastOwn;  // the battle's view last frame
static s32 sLastKey;    // its mode, script and attacker last frame,
static s16 sKeyAge;     // and how many frames since they changed
static u8 sIdleShot;   // one of the title's shots is on in the menus (sShot)
static s8 sSplitHalf = -1; // the half of a split screen being drawn (0 the left, 1 the right),
static unk_D_86002F34_00C* sCamera;      // with the battle's camera,
static unk_D_86002F34_00C_018 sViewport; // as wide as the screen,
static unk_D_86002F34_00C_024 sProjection;
static unk_D_86002F34_00C_040 sOrtho;
static s16 sIdleAnim[SIDES];  // the animation and frame each side's model was on last frame
static s32 sIdleFrame[SIDES];

enum { MOVE_NONE, MOVE_IN, MOVE_OUT };

// A random number from 0 to n - 1, from the randomizer's own generator, so the battle's
// stays as it would be
static s32 Randomizer_BattleCameraBelow(s32 n) {
    sRandom = (sRandom * 0x19660D) + 0x3C6EF35F;
    return ((u64)sRandom * (u32)n) >> 32;
}

// A number from -1 to 1
static f32 Randomizer_BattleCameraWobble(void) {
    return (Randomizer_BattleCameraBelow(201) - 100) / 100.0f;
}

// Which side a Pokemon in the battle is on: 0 the player's, 1 the other
static s32 Randomizer_BattleCameraSideOf(unk_D_84390010* pokemon) {
    return (pokemon == D_84390010[0]) ? 0 : 1;
}

// A Pokemon's idle stance, from its species' battle animation table
static s32 Randomizer_BattleCameraIdleAnim(unk_D_84390010* pokemon) {
    return D_84384570[func_84307F00(pokemon)]->unk_A50.unk_00;
}

// How many frames a model's animation has (0 for none), and the one it's on
static s32 Randomizer_BattleCameraAnimLength(unk_D_86002F58_004_000* model) {
    return ((model->unk_040.unk_00 < 0) || (model->unk_040.unk_04 == NULL)) ? 0 : model->unk_040.unk_04->unk_0A;
}

static s32 Randomizer_BattleCameraAnimFrame(unk_D_86002F58_004_000* model) {
    s32 frame = model->unk_040.unk_08 >> 16;

    return (frame < 0) ? 0 : frame;
}

/*
 * Both sides' Pokemon, every frame: where each is (the points its model marked as it was last
 * drawn, followed smoothly, from where it stands when a new one comes out) and how big (its
 * height from its card's scale, its width the battle's)
 */
static void Randomizer_BattleCameraSides(void) {
    s32 i;

    for (i = 0; i < SIDES; i++) {
        RandomizerShotSide* side = &sSides[i];
        unk_D_86002F58_004_000* model = &D_84390010[i]->unk_000;
        s32 species = model->unk_01A;

        side->model = model;
        if (species != sSpecies[i]) {
            sSpecies[i] = species;
            sTracked[i] = FALSE;
            side->height = ((species >= 1) && (species <= 151))
                               ? CARD_HEIGHT * 100.0f / D_8006FF00[species - 1].unk_02
                               : 100.0f;
            side->width = (D_84390028[i].unk_00 > 0.0f) ? D_84390028[i].unk_00 : side->height * 0.6f;
            func_8000E88C(&side->middle, model->unk_024.x, model->unk_024.y + (side->height * 0.5f),
                          model->unk_024.z);
        }
        Randomizer_ShotTrack(side, &sTracked[i]);
    }
}

/*
 * One of the title's shots of the field or of one of them, in the menus, set up to stay on the
 * camera's side of the field the whole time (z above 0: in the shots that go round, the yaw's
 * cosine above 0 from the shot's start to its end; in the others, the way they turn)
 */
static void Randomizer_BattleCameraIdleCut(void) {
    static const u8 sIdleShots[] = {
        SHOT_PAN, SHOT_ORBIT, SHOT_SIDE, SHOT_CHASE, SHOT_OVERHEAD, SHOT_GAZE, SHOT_GAMEBOY, SHOT_SIGHTS, SHOT_SPLIT,
    };
    s32 kind = sIdleShots[Randomizer_BattleCameraBelow(ARRAY_COUNT(sIdleShots))];
    s32 side = Randomizer_BattleCameraBelow(SIDES);

#ifdef BATTLE_CAMERA_TEST_IDLE
    kind = BATTLE_CAMERA_TEST_IDLE;
#endif
    Randomizer_ShotCut(&sShot, kind, side);
    switch (kind) {
        case SHOT_PAN:
        case SHOT_ORBIT:
            // Going round by PAN_TURN or ORBIT_TURN a frame, IDLE_FRAMES long (46 or 79 degrees)
            sShot.yaw = (-0x2400 + Randomizer_BattleCameraBelow(0x800)) * sShot.turn;
            break;
        case SHOT_OVERHEAD:
            sShot.yaw &= 0x7FFF; // from in front
            break;
        case SHOT_SIDE:
            sShot.turn = (side == 0) ? -1 : 1;
            break;
        case SHOT_CHASE:
            sShot.turn = 1;
            break;
    }
    sShot.roll = 0;
}

// The attacker, as its attack starts: any of the attacker's shots
static void Randomizer_BattleCameraAttackCut(void) {
    Randomizer_ShotCut(&sShot, SHOT_LOW + Randomizer_BattleCameraBelow(NUM_ATTACK_SHOTS),
                       Randomizer_BattleCameraSideOf(D_84390204));
}

// The defender, as the hit lands: a cut, or a swing of the camera from the attacker to it
static void Randomizer_BattleCameraHitCut(void) {
    s32 shot = SHOT_HIT + Randomizer_BattleCameraBelow(NUM_HIT_SHOTS);
    s32 whip = Randomizer_BattleCameraBelow(2);

    Randomizer_ShotCut(&sShot, shot, Randomizer_BattleCameraSideOf(D_84390200));
    sShot.roll = (0x500 + Randomizer_BattleCameraBelow(0x400)) * sShot.turn; // 7 to 12 degrees
    if (whip) {
        sNextShot = shot;
        sShot.kind = SHOT_WHIP;
    } else {
        sShake = SHAKE_FRAMES;
    }
}

/*
 * Whether the attack's shot already ends on the defender, so that the hit lands in it, the
 * camera only shaking, rather than cutting to the defender: from behind the attacker at
 * it, or the Game Boy view zoomed in on it (on the right, so the attacker on the left)
 */
static s32 Randomizer_BattleCameraOnDefender(void) {
    return (sShot.kind == SHOT_SHOULDER) ||
           ((sShot.kind == SHOT_RBY) && (Randomizer_BattleCameraSideOf(D_84390200) == 1));
}

/*
 * The hit landing on a critical hit or a Hyper Beam, the first time or again: low on the
 * defender, then close, then low from its other side, shaking. Again, only the hit: the
 * attacker's action once a hit lands starts over (func_84305760 with 4, as the battle starts
 * it), which starts the defender's reaction and, a few frames on, the move's hit effects
 * (func_84303A48, func_84303BB8), and the reaction starts from its first frame
 * (func_8430897C); the attacker itself goes on.
 */
static void Randomizer_BattleCameraReplayHit(void) {
    static const u8 sHitShots[REPLAY_TIMES] = { SHOT_HIT_LOW, SHOT_HIT, SHOT_HIT_LOW };
    s32 shot = sHitShots[sReplay];

    if (sReplay != 0) {
        func_84305760(D_84390204, ACTION_AFTER_HIT);
        func_8430897C(D_84390200);
    }
    Randomizer_ShotCut(&sShot, shot, Randomizer_BattleCameraSideOf(D_84390200));
    sShot.roll = (0x500 + Randomizer_BattleCameraBelow(0x400)) * sShot.turn;
    sShake = SHAKE_FRAMES;
    sReplay++;
    sReplayFrame = 0;
}

/*
 * Whether the hit's effects have started: the battle starts them once the defender has been
 * reacting for as long as the move's entry in its animation table says (unk_07), and they'd
 * start twice if the hit landed again before
 */
static s32 Randomizer_BattleCameraHitEffectsStarted(void) {
    s32 move = D_84390204->unk_654.unk_38.unk_5A;

    return (move < 1) ||
           (D_84390200->unk_4C4 > (D_84384570[func_84307F00(D_84390200)]->unk_000[move - 1].unk_07 + 1));
}

// A frame of the replay
static void Randomizer_BattleCameraReplayStep(void) {
    sReplayFrame++;
    if ((sReplayFrame >= REPLAY_HIT_FRAMES) && Randomizer_BattleCameraHitEffectsStarted()) {
        if (sReplay < REPLAY_TIMES) {
            Randomizer_BattleCameraReplayHit();
        } else {
            sReplay = 0; // the last shot goes on to the attack's end
        }
    }
}

// The replay cut short (the battle has moved on)
static void Randomizer_BattleCameraReplayStop(void) {
    sReplay = 0;
}

/*
 * Whether this attack is a critical hit or a Hyper Beam. The battle works out a turn before
 * its animation, and queuing "Critical hit!" (func_84371080) moves its flag (D_843C4DA5) to
 * the defender (unk_5B) and marks the director (unk_1A), so those are what's left by the
 * time the attack is filmed; the move has to do damage.
 */
static s32 Randomizer_BattleCameraIsBig(void) {
    s32 critical = (D_843C4DA5 != 0) || ((D_84390240.unk_00->unk_1A == OUTCOME_CRITICAL) &&
                                         (D_84390200->unk_654.unk_38.unk_5B == CRITICAL));

    return (D_843C5238->unk_44.unk_00 == HYPER_BEAM) || (critical && (D_843C5238->unk_44.unk_02 != 0));
}

// Over the shoulder, the hit comes early: the attack's last frames are mostly its back
static s32 Randomizer_BattleCameraHitsEarly(s32 step) {
    unk_D_86002F58_004_000* model = &D_84390204->unk_000;
    s32 length = Randomizer_BattleCameraAnimLength(model);

    return (sShot.kind == SHOT_AIM) && (step == STEP_ANIMATION) && (length > 0) &&
           (model->unk_040.unk_00 != Randomizer_BattleCameraIdleAnim(D_84390204)) &&
           (Randomizer_BattleCameraAnimFrame(model) >= (length - AIM_EARLY));
}

/*
 * Whether an ordinary attack is being filmed by this file this frame, deciding as each starts:
 * the director films a move, with an ordinary attack's script, its attacker using a move (one
 * that can't move uses none). The hit lands, for the camera, as the battle's own camera turns
 * from the attacker to the defender (where the battle has it look, camera's at, nearer the
 * defender), which for some moves is before the attack's hit step: their effects are drawn
 * over the screen where the defender is then (Scratch's claw marks and orange flash). Or at
 * the hit step, whichever comes first; over the shoulder, earlier.
 */
static s32 Randomizer_BattleCameraFilming(unk_D_86002F34_00C* camera) {
    unk_D_84390240_000* director = D_84390240.unk_00;
    RandomizerShotSide* attacker;
    RandomizerShotSide* defender;
    s32 step;
    s32 atDefender;

    if ((director == NULL) || (D_84390204 == NULL) || (D_84390200 == NULL) || (D_843C5238 == NULL) ||
        (director->unk_1C != CAMERA_MODE_MOVE) ||
        ((director->unk_38 != SCRIPT_ATTACK) && (director->unk_38 != SCRIPT_ATTACK_ALT)) ||
        (director->unk_20 > STEP_HIT) || (D_843C5238->unk_44.unk_00 == 0)) {
        if (sAttack) {
            Randomizer_BattleCameraReplayStop();
        }
        sAttack = FALSE;
        return FALSE;
    }

    step = director->unk_20;
    if (!sAttack || (D_84390204 != sAttacker) || (step < sStep)) {
        // A new attack
        if (sAttack) {
            Randomizer_BattleCameraReplayStop();
        }
        sAttack = TRUE;
        sAttacker = D_84390204;
        sFilming = (Randomizer_BattleCameraBelow(FILM_CHANCE) == 0);
#ifdef BATTLE_CAMERA_TEST_ALWAYS
        sFilming = TRUE;
#endif
        sBigHit = FALSE;
        sSawAttacker = FALSE;
        sHitLanded = FALSE;
        sReplay = 0;
        if (sFilming) {
            Randomizer_BattleCameraAttackCut();
#ifdef BATTLE_CAMERA_TEST_SHOT
            sShot.kind = BATTLE_CAMERA_TEST_SHOT;
#endif
        }
    }
    sStep = step;

    // A big hit is always filmed, from when it's known (before the hit lands)
    if (!sBigHit && !sHitLanded && Randomizer_BattleCameraIsBig()) {
        sBigHit = TRUE;
        if (!sFilming) {
            sFilming = TRUE;
            Randomizer_BattleCameraAttackCut();
        }
    }
#ifdef BATTLE_CAMERA_TEST_BIG
    sBigHit = TRUE;
#endif

    if (!sFilming) {
        return FALSE;
    }

    attacker = &sSides[Randomizer_BattleCameraSideOf(D_84390204)];
    defender = &sSides[Randomizer_BattleCameraSideOf(D_84390200)];
    atDefender = ABS(camera->unk_60.at.x - defender->middle.x) < ABS(camera->unk_60.at.x - attacker->middle.x);
    if (!atDefender) {
        sSawAttacker = TRUE;
    }
    if (!sHitLanded &&
        ((step == STEP_HIT) || (sSawAttacker && atDefender) || Randomizer_BattleCameraHitsEarly(step))) {
        sHitLanded = TRUE;
        if (sBigHit) {
            Randomizer_BattleCameraReplayHit();
        } else if (Randomizer_BattleCameraOnDefender()) {
            sShake = SHAKE_FRAMES;
        } else {
            Randomizer_BattleCameraHitCut();
        }
    } else if (sReplay != 0) {
        Randomizer_BattleCameraReplayStep();
    }
    return TRUE;
}

// This frame's view of the attack being filmed
static void Randomizer_BattleCameraShoot(RandomizerShotView* view) {
    RandomizerShotView next;
    RandomizerShotSide* side = &sSides[sShot.side];
    RandomizerShotSide* other = &sSides[sShot.side ^ 1];
    f32 ease;
    f32 size;

    if (sShot.kind == SHOT_WHIP) {
        // Swinging round from where the camera was to the defender's shot, without moving
        Randomizer_ShotCompose(&next, sNextShot, &sShot, 0, side, other, &sSides[0], &sSides[1]);
        ease = Randomizer_ShotEase((f32)sShot.frame / WHIP_FRAMES);
        *view = sLastView;
        func_8000E88C(&view->at, sLastView.at.x + ((next.at.x - sLastView.at.x) * ease),
                      sLastView.at.y + ((next.at.y - sLastView.at.y) * ease),
                      sLastView.at.z + ((next.at.z - sLastView.at.z) * ease));
        view->roll = sLastView.roll + (s16)((next.roll - sLastView.roll) * ease);
        view->fovy = sLastView.fovy + ((next.fovy - sLastView.fovy) * ease);
        if (++sShot.frame >= WHIP_FRAMES) {
            sShot.kind = sNextShot;
            sShot.frame = 0;
            sShake = SHAKE_FRAMES;
        }
    } else {
        Randomizer_ShotCompose(view, sShot.kind, &sShot, sShot.frame, side, other, &sSides[0], &sSides[1]);
        sLastView = *view;
        sShot.frame++;
    }

    // Shaking, less and less, as a hit lands
    if (sShake > 0) {
        size = sqrtf(SQ(view->at.x - view->eye.x) + SQ(view->at.y - view->eye.y) + SQ(view->at.z - view->eye.z)) *
               SHAKE_SIZE * sShake / SHAKE_FRAMES;
        view->at.x += Randomizer_BattleCameraWobble() * size;
        view->at.y += Randomizer_BattleCameraWobble() * size;
        view->eye.x += Randomizer_BattleCameraWobble() * size * 0.5f;
        view->eye.y += Randomizer_BattleCameraWobble() * size * 0.5f;
        sShake--;
    }
}

// The battle's own view this frame (its camera never tilts)
static void Randomizer_BattleCameraOwnView(RandomizerShotView* view, unk_D_86002F34_00C* camera) {
    view->eye = camera->unk_60.eye;
    view->at = camera->unk_60.at;
    view->roll = 0;
    view->fovy = camera->unk_24.fovy;
}

/*
 * A view part of the way (t, 0 to 1) from one to another, the camera swinging round what it
 * looks at rather than going straight, so that it keeps its distance from what's in between:
 * the point it looks at goes straight across, and the camera's distance from it, its height
 * and its direction ease across. view may be from or to.
 */
static void Randomizer_BattleCameraSwing(RandomizerShotView* view, RandomizerShotView* from, RandomizerShotView* to,
                                         f32 t) {
    Vec3f at;
    f32 fromDistance;
    f32 toDistance;
    s16 fromPitch;
    s16 toPitch;
    s16 fromYaw;
    s16 toYaw;
    s16 roll = from->roll + (s16)((to->roll - from->roll) * t);
    f32 fovy = from->fovy + ((to->fovy - from->fovy) * t);

    func_800102A4(&from->at, &from->eye, &fromDistance, &fromPitch, &fromYaw);
    func_800102A4(&to->at, &to->eye, &toDistance, &toPitch, &toYaw);
    func_8000E88C(&at, from->at.x + ((to->at.x - from->at.x) * t), from->at.y + ((to->at.y - from->at.y) * t),
                  from->at.z + ((to->at.z - from->at.z) * t));
    func_80010354(&at, &view->eye, fromDistance + ((toDistance - fromDistance) * t),
                  fromPitch + (s16)((s16)(toPitch - fromPitch) * t), fromYaw + (s16)((s16)(toYaw - fromYaw) * t));
    view->at = at;
    view->roll = roll;
    view->fovy = fovy;
}

/*
 * Both Pokemon from the side, square to the line between them (on the side the camera keeps
 * to), a little above them, as far as it takes to see them whole
 */
static void Randomizer_BattleCameraBoth(RandomizerShotView* view) {
    RandomizerShotSide* left = &sSides[0];
    RandomizerShotSide* right = &sSides[1];
    Vec3f between;
    f32 apart;
    f32 across;
    f32 tall;
    f32 distance;

    func_8000E88C(&between, right->middle.x - left->middle.x, 0.0f, right->middle.z - left->middle.z);
    apart = Randomizer_ShotNormalize(&between);
    across = (apart + MAX(left->width, right->width)) * 0.5f;
    tall = (MAX(left->height, right->height) + ABS(right->middle.y - left->middle.y)) * 0.5f;
    distance = MAX(across / (TAN_HALF_FOVY * ASPECT), tall / TAN_HALF_FOVY) * BOTH_MARGIN;

    func_8000E88C(&view->at, (left->middle.x + right->middle.x) * 0.5f, (left->middle.y + right->middle.y) * 0.5f,
                  (left->middle.z + right->middle.z) * 0.5f);
    func_8000E88C(&view->eye, view->at.x - (between.z * distance * COSS(BOTH_PITCH)),
                  view->at.y + (distance * SINS(BOTH_PITCH)), view->at.z + (between.x * distance * COSS(BOTH_PITCH)));
    view->roll = 0;
    view->fovy = CAMERA_FOVY;
}

/*
 * The first to attack this turn (the director's unk_2A, set as both have picked,
 * func_84320108), from in front and to the side, a little above its middle
 */
static void Randomizer_BattleCameraOpening(RandomizerShotView* view) {
    RandomizerShotSide* side = &sSides[D_84390240.unk_00->unk_2A & 1];
    s16 facing = ((D_84390240.unk_00->unk_2A & 1) == 0) ? 0x4000 : -0x4000; // the yaw that sees its face

    Randomizer_ShotAimLevel(view, &side->middle, Randomizer_ShotDistance(side, OPENING_FRAMING),
                            side->middle.y + (side->height * 0.3f),
                            facing - ((facing > 0) ? OPENING_SLANT : -OPENING_SLANT), 0);
    view->fovy = CAMERA_FOVY;
}

// A view kept on the camera's side of the field, mirrored across the line if it isn't
static void Randomizer_BattleCameraKeepSide(RandomizerShotView* view) {
    if (view->eye.z < FIELD_LINE_Z) {
        view->eye.z = (2.0f * FIELD_LINE_Z) - view->eye.z;
        view->at.z = (2.0f * FIELD_LINE_Z) - view->at.z;
        view->roll = -view->roll;
    }
}

/*
 * The wipe's bands drawn, or see-through: the alpha of its colour command (D_8438ABE8), which
 * the RSP reads, so written back from the cache
 */
static void Randomizer_BattleCameraWipeShown(s32 shown) {
    Gfx* gfx;

    for (gfx = D_8438ABE8; (u8)(gfx->words.w0 >> 24) != (u8)G_ENDDL; gfx++) {
        if ((u8)(gfx->words.w0 >> 24) == (u8)G_SETPRIMCOLOR) {
            gfx->words.w1 = (gfx->words.w1 & ~0xFF) | (shown ? OPAQUE : 0);
            osWritebackDCache(gfx, sizeof(Gfx));
            return;
        }
    }
}

/*
 * Whether the wipe hides something the battle does while the screen is black: a Pokemon's
 * model swapped or moved (WIPE_HIDES), or put back from one of the poses func_843066E0 resets
 * (unk_4B4)
 */
static s32 Randomizer_BattleCameraKeepsWipe(void) {
    s32 i;

    for (i = 0; i < SIDES; i++) {
        unk_D_84390010* pokemon = D_84390010[i];

        if ((pokemon->unk_654.unk_34 & WIPE_HIDES) ||
            ((pokemon->unk_4B4 >= 0) && (pokemon->unk_4B4 < 32) && ((1 << pokemon->unk_4B4) & WIPE_POSES))) {
            return TRUE;
        }
    }
    return FALSE;
}

/*
 * In place of the black jagged wipe between the menus and a turn, and a turn and the menus,
 * unless it hides something: the camera goes from where it was to the first to attack (into
 * the turn, the director's unk_44 2) or to both Pokemon (to the menus), then, as the wipe
 * would open, on into the battle's camera (own), by then on what comes next. The wipe keeps
 * its time, its bands see-through. Into the turn, the camera already sets off to both as the
 * player picks, the battle taking a moment before it asks for the wipe. Returns whether the
 * camera is moving, with this frame's view.
 */
static s32 Randomizer_BattleCameraMove(RandomizerShotView* own, RandomizerShotView* view) {
    unk_D_84390240_000* director = D_84390240.unk_00;
    s32 wiping = (director != NULL) && (director->unk_2E != 0) && (director->unk_30 >= WIPE_ASKED);
    s32 ready = (director != NULL) && (director->unk_1C == CAMERA_MODE_MENUS) && (director->unk_30 == 0) &&
                (D_84390010[0]->unk_654.unk_10 == MENU_READY);
    RandomizerShotView next;

    if (wiping && !sWiping) {
        if (Randomizer_BattleCameraKeepsWipe()) {
            Randomizer_BattleCameraWipeShown(TRUE);
            sMoving = MOVE_NONE;
        } else {
            // From where it is: the camera may already be on its way to both, since the pick
            Randomizer_BattleCameraWipeShown(FALSE);
            sMoving = MOVE_IN;
            sMoveFrame = 0;
            sMoveFrom = sDrawnView;
        }
    } else if (ready && (sMoving == MOVE_NONE) && !sMovedReady) {
        // The player has picked: on to both while the battle gets the turn ready
        sMoving = MOVE_IN;
        sMoveFrame = 0;
        sMoveFrom = sDrawnView;
        sMovedReady = TRUE;
    }
    if (!ready) {
        sMovedReady = FALSE;
    }
    sWiping = wiping;
    if (sMoving == MOVE_NONE) {
        return FALSE;
    }
    if ((sMoving == MOVE_IN) && (wiping ? (director->unk_30 >= WIPE_OPENING) : !ready)) {
        sMoving = MOVE_OUT;
        sOutFrame = 0;
    }

    if (wiping && (director->unk_44 == 2)) {
        Randomizer_BattleCameraOpening(&next);
    } else {
        Randomizer_BattleCameraBoth(&next);
    }
    Randomizer_BattleCameraSwing(view, &sMoveFrom, &next, Randomizer_ShotEase((f32)sMoveFrame / MOVE_IN_FRAMES));
    sMoveFrame++;
    if (sMoving == MOVE_OUT) {
        Randomizer_BattleCameraSwing(view, view, own, Randomizer_ShotEase((f32)sOutFrame / MOVE_OUT_FRAMES));
        if (++sOutFrame > MOVE_OUT_FRAMES) {
            sMoving = MOVE_NONE;
        }
    }
    return TRUE;
}

// Whether this file's view has a side's Pokemon in it: in a split screen, the half's own; the
// shot's own, and the other too in a shot of both (from behind one at the other, or of the
// field); both as the camera moves between shots
static s32 Randomizer_BattleCameraFilms(s32 side) {
    if (sSplitHalf >= 0) {
        return side == sSplitHalf;
    }
    return sMoving || (sHandover > 0) || (side == sShot.side) || (sShot.kind == SHOT_SHOULDER) ||
           (sShot.kind == SHOT_GAMEBOY) || (sShot.kind == SHOT_RBY) || (sShot.kind == SHOT_SIGHTS) ||
           (sShot.kind == SHOT_AIM) || (sShot.kind == SHOT_PAN) || (sShot.kind == SHOT_OVERHEAD) ||
           (sShot.kind == SHOT_CHASE);
}

/*
 * The Pokemon this file's view is of shown while it's drawn, or put back as the battle had
 * them. The battle shows both every frame, then hides the one its camera isn't on, or one
 * that has fainted (func_8432A578 and func_8432A510), and as the hit lands draws the
 * attacker see-through (unk_01D 0) for a moment: the ones in the view are drawn, unless
 * fainted. In the other shots the other one is as the battle has it, out of the way of a
 * hit's shots as an attacker lunges at its target. A Pokemon underground is the battle's to
 * draw see-through, and one in the air high up.
 */
static void Randomizer_BattleCameraShow(s32 show) {
    s32 i;

    for (i = 0; i < SIDES; i++) {
        unk_D_84390010* pokemon = D_84390010[i];
        u8* flags = &pokemon->unk_000.unk_000.unk_01;

        if (show) {
            sShown[i] = *flags & 1;
            sAlpha[i] = pokemon->unk_000.unk_01D;
            if (Randomizer_BattleCameraFilms(i) && (pokemon->unk_654.unk_2D != FAINTED) &&
                (pokemon->unk_654.unk_2D != FAINTED_ALT)) {
                *flags |= 1;
                if (!(pokemon->unk_654.unk_34 & UNDERGROUND)) {
                    pokemon->unk_000.unk_01D = OPAQUE;
                }
            } else if (sSplitHalf >= 0) {
                *flags &= ~1;
            }
        } else {
            *flags = (*flags & ~1) | sShown[i];
            pokemon->unk_000.unk_01D = sAlpha[i];
        }
    }
}

/*
 * Both Pokemon's idle stances kept going. The battle puts one back on its idle stance's first
 * frame as it hides it, every frame (func_843087F8), and both as a turn starts (func_84321184)
 * and as the menus come back, which the wipe used to hide: one on the same idle stance it was
 * on the frame before, put back on its first frame, goes on from where it was instead.
 */
static void Randomizer_BattleCameraKeepIdle(void) {
    s32 i;

    for (i = 0; i < SIDES; i++) {
        unk_D_84390010* pokemon = D_84390010[i];
        unk_D_86002F58_004_000* model = &pokemon->unk_000;
        s32 anim = model->unk_040.unk_00;
        s32 frame = Randomizer_BattleCameraAnimFrame(model);
        s32 length = Randomizer_BattleCameraAnimLength(model);

        if ((anim >= 0) && (anim == Randomizer_BattleCameraIdleAnim(pokemon)) && (anim == sIdleAnim[i]) &&
            (frame == 0) && (sIdleFrame[i] > 0) && (length > 0)) {
            frame = (sIdleFrame[i] + 1) % length;
            func_80017464(model, frame);
        }
        sIdleAnim[i] = anim;
        sIdleFrame[i] = frame;
    }
}

/*
 * The split screen (SHOT_SPLIT): the one on the left in the left half and the other in the
 * right, each with only its own Pokemon shown (one model drawn twice in a frame from two
 * cameras comes out in pieces the second time, randomizer_title_arena.c). The battle draws the
 * left half (its camera made half as wide before its sky and its scene are drawn), and this
 * the right half once it has (Randomizer_OverlayStub): the sky, then the scene again.
 */
static void Randomizer_BattleCameraSplitSet(unk_D_86002F34_00C* camera, s32 half) {
    RandomizerShotView view;

    sSplitHalf = half;
    func_80011DAC(camera, half * (SCREEN_W / 2), 0, SCREEN_W / 2, SCREEN_H);
    func_80011E68(camera, CAMERA_FOVY, sProjection.near, sProjection.far);
    Randomizer_ShotSplitHalf(&view, &sSides[half], half, sShot.frame);
    // Clear of the menus' boxes, the player's at the top left and the other's at the bottom
    // right: the left one a little low and right of its half's middle, the other high and left
    Randomizer_ShotAimAt(&view, &view.eye, &sSides[half].middle, (half == 0) ? SPLIT_LEFT_RAISE : SPLIT_RIGHT_RAISE,
                         (half == 0) ? SPLIT_LATERAL : -SPLIT_LATERAL, 0);
    camera->unk_60.eye = view.eye;
    camera->unk_60.at = view.at;
    Randomizer_ShotUp(&view, &camera->unk_60.up);
    Randomizer_BattleCameraShow(TRUE);
    sShowing = TRUE;
    sSwapped = TRUE;
}

// The battle's sky in the right half, as func_84300340 draws it in the whole screen
static void Randomizer_BattleCameraSplitSky(void) {
    s32 x = SCREEN_W / 2;
    s32 middle = SCREEN_H / 2;
    void* sky = BATTLE_SKY;

    gDPPipeSync(gDisplayListHead++);
    if (((u32)sky == 0) || ((u32)sky == -1) || ((u32)sky < 0x10000)) {
        func_800065B4(&gDisplayListHead, x, 0, SCREEN_W - 1, SCREEN_H - 1, 0x0001);
        return;
    }
    gDPSetCycleType(gDisplayListHead++, G_CYC_1CYCLE);
    gDPSetRenderMode(gDisplayListHead++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    gDPSetTexturePersp(gDisplayListHead++, G_TP_NONE);
    gDPSetCombineMode(gDisplayListHead++, G_CC_MODULATEI_PRIM, G_CC_MODULATEI_PRIM);
    gDPSetPrimColor(gDisplayListHead++, 0, 0, SKY_COLOUR.r, SKY_COLOUR.g, SKY_COLOUR.b, 255);
    gSPClearGeometryMode(gDisplayListHead++, G_ZBUFFER | G_LIGHTING);
    gDPLoadTextureBlock(gDisplayListHead++, Memmap_GetFragmentVaddr(sky), G_IM_FMT_RGBA, G_IM_SIZ_32b, 4, 64, 0,
                        G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                        G_TX_NOLOD);
    gSPTextureRectangle(gDisplayListHead++, x << 2, 0, SCREEN_W << 2, middle << 2, G_TX_RENDERTILE, 0, 0, 0,
                        0x10000 / middle);
    gSPTextureRectangle(gDisplayListHead++, x << 2, middle << 2, SCREEN_W << 2, SCREEN_H << 2, G_TX_RENDERTILE, 0,
                        0x07E0, 0, 0);
    gSPDisplayList(gDisplayListHead++, D_8006F630);
}

// After the battle has drawn the left half: the right half, the camera put back, and a line between
static void Randomizer_BattleCameraSplitRight(void) {
    unk_D_86002F34_00C* camera = sCamera;

    Randomizer_BattleCameraSplitSky();
    sBattleCamera.eye = camera->unk_60.eye;
    sBattleCamera.at = camera->unk_60.at;
    sBattleCamera.up = camera->unk_60.up;
    sBattleCamera.fovy = sProjection.fovy;
    Randomizer_BattleCameraSplitSet(camera, 1);
    func_80015094(BATTLE_SCENE); // its callback puts the camera and the Pokemon back
    camera->unk_18 = sViewport;
    camera->unk_24 = sProjection;
    camera->unk_40 = sOrtho;
    sSplitHalf = -1;

    gDPPipeSync(gDisplayListHead++);
    gDPSetCycleType(gDisplayListHead++, G_CYC_FILL);
    gDPSetRenderMode(gDisplayListHead++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetFillColor(gDisplayListHead++, (GPACK_RGBA5551(0, 0, 0, 1) << 16) | GPACK_RGBA5551(0, 0, 0, 1));
    gDPFillRectangle(gDisplayListHead++, (SCREEN_W / 2) - 2, 0, (SCREEN_W / 2) + 1, SCREEN_H - 1);
    gDPPipeSync(gDisplayListHead++);
    gDPSetCycleType(gDisplayListHead++, G_CYC_1CYCLE);
}

/*
 * The hook (gRandomizerState.battleCameraHook), every frame after the battle's director
 * has moved the camera (drawn 0) and once the scene is drawn (drawn 1)
 */
static void Randomizer_BattleCamera(unk_D_86002F34_00C* camera, s32 drawn) {
    unk_D_84390240_000* director = D_84390240.unk_00;
    RandomizerShotView own;
    RandomizerShotView view;
    s32 moving;
    s32 filming;
    s32 jump;
    s32 key;

    if (drawn == 2) {
        // Once the scene is drawn, before the menus
        if (sSplitHalf == 0) {
            Randomizer_BattleCameraSplitRight();
        }
        return;
    }
    if (drawn) {
        if (sSwapped) {
            camera->unk_60.eye = sBattleCamera.eye;
            camera->unk_60.at = sBattleCamera.at;
            camera->unk_60.up = sBattleCamera.up;
            camera->unk_24.fovy = sBattleCamera.fovy;
            if (sShowing) {
                Randomizer_BattleCameraShow(FALSE);
            }
            sSwapped = FALSE;
        }
        return;
    }
    if ((D_84390010[0] == NULL) || (D_84390010[1] == NULL)) {
        return;
    }
    Randomizer_BattleUiTeamName();
    if (!sCustom) {
        return;
    }
    sFrame++;
    Randomizer_BattleCameraSides();
    Randomizer_BattleCameraKeepIdle();

    Randomizer_BattleCameraOwnView(&own, camera);
    Randomizer_BattleCameraKeepSide(&own);
    // The battle's mode, its script in a move, and the attacker: a change of any is a cut the
    // camera may smooth
    key = (director == NULL) ? 0
                             : ((director->unk_1C << 16) |
                                ((director->unk_1C == CAMERA_MODE_MOVE) ? (director->unk_38 << 8) : 0) |
                                (D_84390204 == D_84390010[1]));
    if (key != sLastKey) {
        sLastKey = key;
        sKeyAge = 0;
    } else if (sKeyAge < 0x7FFF) {
        sKeyAge++;
    }

    moving = Randomizer_BattleCameraMove(&own, &view);
    filming = Randomizer_BattleCameraFilming(camera);
    jump = (sqrtf(SQ(own.eye.x - sLastOwn.eye.x) + SQ(own.eye.y - sLastOwn.eye.y) + SQ(own.eye.z - sLastOwn.eye.z)) >
            CUT_DISTANCE) ||
           (sqrtf(SQ(own.at.x - sLastOwn.at.x) + SQ(own.at.y - sLastOwn.at.y) + SQ(own.at.z - sLastOwn.at.z)) >
            CUT_DISTANCE);
    if (filming) {
        // An attack filmed takes over from the move, the camera going to its first shot from
        // where it was
        if (!sWasFilming) {
            sHandover = HANDOVER_FRAMES;
            sHandFrom = sDrawnView;
        }
        sMoving = MOVE_NONE;
        Randomizer_BattleCameraShoot(&view);
        Randomizer_BattleCameraKeepSide(&view);
        if (sHitLanded) {
            sHandover = 0; // the hit's shots cut or swing to the defender themselves
        }
        if (sHandover > 0) {
            sHandover--;
            Randomizer_BattleCameraSwing(&view, &sHandFrom, &view,
                                         Randomizer_ShotEase(1.0f - ((f32)sHandover / HANDOVER_FRAMES)));
        }
    } else if (!moving && sIdleShot) {
        if (sShot.kind != SHOT_SPLIT) {
            Randomizer_BattleCameraShoot(&view);
            Randomizer_BattleCameraKeepSide(&view);
        } else {
            view = own; // drawn only if the split screen ends this frame (below)
        }
    } else if (!moving) {
        // Into the battle's camera after a filmed attack, and where it cuts soon after going on
        // to something else, by a quick move (again from where it is, if it cuts again)
        if (sWasFilming || ((sKeyAge < CUT_AFTER_CHANGE) && jump)) {
            sHandover = HANDOVER_FRAMES;
            sHandFrom = sDrawnView;
        }
        if (sHandover > 0) {
            sHandover--;
            Randomizer_BattleCameraSwing(&view, &sHandFrom, &own,
                                         Randomizer_ShotEase(1.0f - ((f32)sHandover / HANDOVER_FRAMES)));
        } else {
            view = own;
        }
    } else {
        sHandover = 0;
    }
    // In the menus, where the battle's camera cuts to another shot, sometimes one of the
    // title's instead, for as long as the title's last
    if ((director == NULL) || (director->unk_1C != CAMERA_MODE_MENUS) || (director->unk_30 != 0) || moving ||
        filming || (sIdleShot && (sShot.frame >= IDLE_FRAMES))) {
        sIdleShot = FALSE;
    } else if (!sIdleShot && (sHandover == 0) && (sKeyAge >= CUT_AFTER_CHANGE) && jump &&
               (Randomizer_BattleCameraBelow(IDLE_CHANCE) == 0
#ifdef BATTLE_CAMERA_TEST_IDLE
                || TRUE
#endif
                )) {
        sIdleShot = TRUE;
        Randomizer_BattleCameraIdleCut();
        if (sShot.kind != SHOT_SPLIT) {
            Randomizer_BattleCameraShoot(&view);
            Randomizer_BattleCameraKeepSide(&view);
        }
    }
    sWasFilming = filming;
    sLastOwn = own;
    if (sIdleShot && (sShot.kind == SHOT_SPLIT)) {
        // The left half's view for the battle to draw, as wide as half the screen
        sCamera = camera;
        sViewport = camera->unk_18;
        sProjection = camera->unk_24;
        sOrtho = camera->unk_40;
        sBattleCamera.eye = camera->unk_60.eye;
        sBattleCamera.at = camera->unk_60.at;
        sBattleCamera.up = camera->unk_60.up;
        sBattleCamera.fovy = camera->unk_24.fovy;
        Randomizer_BattleCameraSplitSet(camera, 0);
        Randomizer_ShotSplitHalf(&sDrawnView, &sSides[0], 0, sShot.frame);
        sDrawnView.fovy = CAMERA_FOVY;
        sShot.frame++;
        return;
    }
    sDrawnView = view;

    sBattleCamera.eye = camera->unk_60.eye;
    sBattleCamera.at = camera->unk_60.at;
    sBattleCamera.up = camera->unk_60.up;
    sBattleCamera.fovy = camera->unk_24.fovy;
    camera->unk_60.eye = view.eye;
    camera->unk_60.at = view.at;
    Randomizer_ShotUp(&view, &camera->unk_60.up);
    camera->unk_24.fovy = view.fovy;
    sShowing = filming || moving || sIdleShot || (sHandover > 0);
    if (sShowing) {
        Randomizer_BattleCameraShow(TRUE);
    }
    sSwapped = TRUE;
}

/*
 * As the battle starts (Randomizer_BattleUiEntry): the hook, which names the switch menu's box
 * whatever the camera, and films with Options' "Battle camera" on Custom
 */
void Randomizer_BattleCameraStart(void) {
    sSwapped = FALSE;
    sAttack = FALSE;
    sReplay = 0;
    sAttacker = NULL;
    sShake = 0;
    sFrame = 0;
    sWiping = FALSE;
    sMoving = MOVE_NONE;
    sMovedReady = FALSE;
    sHandover = 0;
    sIdleShot = FALSE;
    sSplitHalf = -1;
    sWasFilming = FALSE;
    sLastKey = -1;
    sKeyAge = 0;
    sSpecies[0] = sSpecies[1] = -1;
    sIdleAnim[0] = sIdleAnim[1] = -1;
    sRandom = osGetCount();
    sCustom = !gRandomizerState.originalCamera;
    gRandomizerState.battleCameraHook = Randomizer_BattleCamera;
}

#endif
