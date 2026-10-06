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

#include "src/17300.h"
#include "src/1C720.h"
#include "src/randomizer_state.h"

#define SHOT_LOOK_RAISE 0.05f // of the distance: what the camera films a little under the middle
#define SHOT_BELOW(n) Randomizer_BattleCameraBelow(n)
static s32 Randomizer_BattleCameraBelow(s32 n);
#include "src/randomizer_shots.h"

// The director's state (D_84390240.unk_00), as measured
#define CAMERA_MODE_MOVE 2 // unk_1C: a move
#define OUTCOME_CRITICAL 2 // unk_1A: set with the "Critical hit!" message (func_84371080), or a one-hit KO's
#define SCRIPT_ATTACK 2    // unk_38: an ordinary attack's camera script, or the other one
#define SCRIPT_ATTACK_ALT 3
#define STEP_ATTACKER 0  // unk_20: the camera going to the attacker,
#define STEP_ANIMATION 1 // its attack playing,
#define STEP_HIT 2       // the defender reacting to the hit

// A Pokemon's actions (func_84305760, unk_4C0): its attack from the start, its animation and
// its effects (func_84320CEC), and what it does once the hit lands (func_84323FA0)
#define ACTION_ATTACK 3
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
#define REPLAY_HIT_FRAMES 40 // each time for this long before the attack plays again
#define REPLAY_ATTACK_MIN 20 // the attack played again lasts as long as it took to hit, within these
#define REPLAY_ATTACK_MAX 150
#define AIM_EARLY 30 // over the shoulder, the hit comes this many frames before the attack's animation ends
#define WHIP_FRAMES 8
#define SHAKE_FRAMES 14
#define SHAKE_SIZE 0.025f // of the camera's distance
/*
 * The wipe between the menus and a turn (func_84329B04, the director's unk_30 when its unk_2E
 * isn't 0): it starts closing at step 3, the battle's camera is on what comes next from step
 * 9, as it opens, and it's over at 0
 */
#define WIPE_CLOSING 3
#define WIPE_OPENING 9
/*
 * unk_654.unk_34: what the battle swaps or moves while the wipe hides the screen
 * (func_843062F0, func_84306218, func_8431FAB4, func_8431F998): a substitute (0x800, its
 * doll's model loaded in or out), in the air (4, 8, 0x200, 0x400) or underground (0x4000)
 */
#define WIPE_HIDES 0x4E0C
#define WIPE_POSES 0x1F6D0 // unk_4B4: the poses func_843066E0 puts back then, 4, 6, 7, 9, 10 and 12 to 16
#define MOVE_IN_FRAMES 30  // the camera going from where it was to both Pokemon,
#define MOVE_OUT_FRAMES 45 // and from them into the battle's camera, as the wipe would open
#define BOTH_PITCH 0x900   // looking down on both by about 12 degrees,
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
static s32 sDrive[SIDES]; // frames into the animation this file plays on a Pokemon the battle holds still
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
static u8 sReplayAttack; // its attack playing again, before the hit lands again
static s16 sReplayFrame;
static s32 sAttackStart; // the frame the attack's animation started (-1 not yet),
static s16 sAttackFrames; // how long it took to hit,
static s16 sAttackAnim;   // and the attacker's animation for it (-1 none seen)
static unk_D_84390010* sAttacker;
static s32 sStep;  // of the attack, the last frame
static s32 sFrame; // frames since the battle started
static u32 sRandom;
static RandomizerShotView sDrawnView; // the view drawn the last frame, this file's or the battle's
static u8 sWiping;  // the battle's wipe is going on,
static u8 sMoving;  // and the camera moves in its place (MOVE_IN, then MOVE_OUT)
static s16 sMoveFrame;
static s16 sOutFrame;
static f32 sMoveSide; // which side of the field (z) it films both from
static RandomizerShotView sMoveFrom;

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

// A Pokemon's idle stance and hit reaction, from its species' battle animation table
static s32 Randomizer_BattleCameraIdleAnim(unk_D_84390010* pokemon) {
    return D_84384570[func_84307F00(pokemon)]->unk_A50.unk_00;
}

static s32 Randomizer_BattleCameraHitAnim(unk_D_84390010* pokemon) {
    return D_84384570[func_84307F00(pokemon)]->unk_A80.unk_00;
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
 * defender, then close, then low from its other side, shaking. Again, the defender's hit
 * reaction starts over, as the battle starts it (func_8430897C), and the attacker goes on as
 * it does once the hit lands.
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
    sReplayAttack = FALSE;
    sReplayFrame = 0;
}

/*
 * The attack played again from the start, its effects too (the attacker's action, as the
 * battle starts it), from behind the attacker, then over its shoulder; the hit lands again
 * as long after as it did the first time
 */
static void Randomizer_BattleCameraReplayAttack(void) {
    static const u8 sAttackShots[REPLAY_TIMES - 1] = { SHOT_SHOULDER, SHOT_AIM };

    func_84305760(D_84390204, ACTION_ATTACK);
    Randomizer_ShotCut(&sShot, sAttackShots[sReplay - 1], Randomizer_BattleCameraSideOf(D_84390204));
    sDrive[Randomizer_BattleCameraSideOf(D_84390204)] = 0;
    sReplayAttack = TRUE;
    sReplayFrame = 0;
}

// A frame of the replay
static void Randomizer_BattleCameraReplayStep(void) {
    sReplayFrame++;
    if (sReplayAttack) {
        if (sReplayFrame >= sAttackFrames) {
            Randomizer_BattleCameraReplayHit();
        }
    } else if (sReplayFrame >= REPLAY_HIT_FRAMES) {
        if (sReplay < REPLAY_TIMES) {
            Randomizer_BattleCameraReplayAttack();
        } else {
            sReplay = 0; // the last shot goes on to the attack's end
        }
    }
}

// The replay cut short (the battle has moved on): the attacker as it is once the hit lands
static void Randomizer_BattleCameraReplayStop(void) {
    if (sReplayAttack && (sAttacker != NULL)) {
        func_84305760(sAttacker, ACTION_AFTER_HIT);
    }
    sReplayAttack = FALSE;
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
    s32 anim;

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
        sReplayAttack = FALSE;
        sAttackStart = -1;
        sAttackAnim = -1;
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

    // The attack's animation, for the replay: when it starts, and which it is
    if ((step == STEP_ANIMATION) && !sHitLanded) {
        if (sAttackStart < 0) {
            sAttackStart = sFrame;
        }
        anim = D_84390204->unk_000.unk_040.unk_00;
        if ((anim >= 0) && (anim != Randomizer_BattleCameraIdleAnim(D_84390204))) {
            sAttackAnim = anim;
        }
    }
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
        sAttackFrames = (sAttackStart < 0) ? REPLAY_ATTACK_MIN : CLAMP(sFrame - sAttackStart, REPLAY_ATTACK_MIN, REPLAY_ATTACK_MAX);
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

// A view part of the way (t, 0 to 1) from one to another; view may be from
static void Randomizer_BattleCameraBlend(RandomizerShotView* view, RandomizerShotView* from, RandomizerShotView* to,
                                         f32 t) {
    func_8000E88C(&view->eye, from->eye.x + ((to->eye.x - from->eye.x) * t),
                  from->eye.y + ((to->eye.y - from->eye.y) * t), from->eye.z + ((to->eye.z - from->eye.z) * t));
    func_8000E88C(&view->at, from->at.x + ((to->at.x - from->at.x) * t), from->at.y + ((to->at.y - from->at.y) * t),
                  from->at.z + ((to->at.z - from->at.z) * t));
    view->roll = from->roll + (s16)((to->roll - from->roll) * t);
    view->fovy = from->fovy + ((to->fovy - from->fovy) * t);
}

/*
 * Both Pokemon from the side, square to the line between them (on sMoveSide's side of it), a
 * little above them, as far as it takes to see them whole
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
    func_8000E88C(&view->eye, view->at.x - (sMoveSide * between.z * distance * COSS(BOTH_PITCH)),
                  view->at.y + (distance * SINS(BOTH_PITCH)),
                  view->at.z + (sMoveSide * between.x * distance * COSS(BOTH_PITCH)));
    view->roll = 0;
    view->fovy = CAMERA_FOVY;
}

/*
 * Which side of the line between the two Pokemon the camera films both from: the one it's on
 * (1 or -1, as Randomizer_BattleCameraBoth puts it)
 */
static f32 Randomizer_BattleCameraSideOfLine(Vec3f* eye) {
    f32 cross = ((sSides[1].middle.x - sSides[0].middle.x) * (eye->z - sSides[0].middle.z)) -
                ((sSides[1].middle.z - sSides[0].middle.z) * (eye->x - sSides[0].middle.x));

    return (cross < 0.0f) ? -1.0f : 1.0f;
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
 * unless it hides something: the camera goes from where it was to both Pokemon, then, as the
 * wipe would open, on into the battle's camera, by then on what comes next. The wipe keeps its
 * time, its bands see-through. Returns whether the camera is moving, with this frame's view.
 */
static s32 Randomizer_BattleCameraMove(unk_D_86002F34_00C* camera, RandomizerShotView* view) {
    unk_D_84390240_000* director = D_84390240.unk_00;
    s32 wiping = (director != NULL) && (director->unk_2E != 0) && (director->unk_30 >= WIPE_CLOSING);
    RandomizerShotView next;

    if (wiping && !sWiping) {
        if (Randomizer_BattleCameraKeepsWipe()) {
            Randomizer_BattleCameraWipeShown(TRUE);
        } else {
            Randomizer_BattleCameraWipeShown(FALSE);
            sMoving = MOVE_IN;
            sMoveFrame = 0;
            sMoveFrom = sDrawnView;
            sMoveSide = Randomizer_BattleCameraSideOfLine(&sDrawnView.eye);
        }
    }
    sWiping = wiping;
    if (sMoving == MOVE_NONE) {
        return FALSE;
    }
    if ((sMoving == MOVE_IN) && (!wiping || (director->unk_30 >= WIPE_OPENING))) {
        sMoving = MOVE_OUT;
        sOutFrame = 0;
    }

    Randomizer_BattleCameraBoth(&next);
    Randomizer_BattleCameraBlend(view, &sMoveFrom, &next, Randomizer_ShotEase((f32)sMoveFrame / MOVE_IN_FRAMES));
    sMoveFrame++;
    if (sMoving == MOVE_OUT) {
        Randomizer_BattleCameraOwnView(&next, camera);
        Randomizer_BattleCameraBlend(view, view, &next, Randomizer_ShotEase((f32)sOutFrame / MOVE_OUT_FRAMES));
        if (++sOutFrame > MOVE_OUT_FRAMES) {
            sMoving = MOVE_NONE;
        }
    }
    return TRUE;
}

// Whether this file's view has a side's Pokemon in it: the shot's own, and the other too in a
// shot of both (from behind one, at the other)
static s32 Randomizer_BattleCameraFilms(s32 side) {
    return sMoving || (side == sShot.side) || (sShot.kind == SHOT_SHOULDER) || (sShot.kind == SHOT_GAMEBOY) ||
           (sShot.kind == SHOT_RBY) || (sShot.kind == SHOT_SIGHTS) || (sShot.kind == SHOT_AIM);
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
            }
        } else {
            *flags = (*flags & ~1) | sShown[i];
            pokemon->unk_000.unk_01D = sAlpha[i];
        }
    }
}

/*
 * The animations of the Pokemon in this file's view that the battle holds still. Hiding one,
 * it puts it on its idle stance's first frame every frame (func_843087F8), which this plays
 * on. Played again, the attack's animation, from the start; and the defender held on its hit
 * reaction's first frame until the hit lands again, so that the battle waits for it.
 */
static void Randomizer_BattleCameraAnimate(void) {
    s32 i;

    for (i = 0; i < SIDES; i++) {
        unk_D_84390010* pokemon = D_84390010[i];
        unk_D_86002F58_004_000* model = &pokemon->unk_000;
        s32 length;

        if (sReplayAttack && (pokemon == D_84390204) && (sAttackAnim >= 0)) {
            func_8001BD04(model, sAttackAnim);
            length = Randomizer_BattleCameraAnimLength(model);
            func_80017464(model, MIN(sDrive[i], MAX(length - 1, 0)));
            sDrive[i]++;
        } else if (sReplayAttack && (pokemon == D_84390200)) {
            func_8001BD04(model, Randomizer_BattleCameraHitAnim(pokemon));
            func_80017464(model, 0);
        } else if (!(model->unk_000.unk_01 & 1) && Randomizer_BattleCameraFilms(i) &&
                   ((length = Randomizer_BattleCameraAnimLength(model)) > 0)) {
            func_80017464(model, sDrive[i] % length);
            sDrive[i]++;
        } else {
            sDrive[i] = 0;
        }
    }
}

/*
 * The hook (gRandomizerState.battleCameraHook), every frame after the battle's director
 * has moved the camera (drawn 0) and once the scene is drawn (drawn 1)
 */
static void Randomizer_BattleCamera(unk_D_86002F34_00C* camera, s32 drawn) {
    RandomizerShotView view;
    s32 moving;

    if (drawn) {
        if (sSwapped) {
            camera->unk_60.eye = sBattleCamera.eye;
            camera->unk_60.at = sBattleCamera.at;
            camera->unk_60.up = sBattleCamera.up;
            camera->unk_24.fovy = sBattleCamera.fovy;
            Randomizer_BattleCameraShow(FALSE);
            sSwapped = FALSE;
        }
        return;
    }
    if ((D_84390010[0] == NULL) || (D_84390010[1] == NULL)) {
        return;
    }
    sFrame++;
    Randomizer_BattleCameraSides();
    moving = Randomizer_BattleCameraMove(camera, &view);
    if (Randomizer_BattleCameraFilming(camera)) {
        // An attack filmed as it starts takes over from the move
        sMoving = MOVE_NONE;
        Randomizer_BattleCameraShoot(&view);
    } else if (!moving) {
        Randomizer_BattleCameraOwnView(&sDrawnView, camera);
        return;
    }
    sDrawnView = view;
    Randomizer_BattleCameraAnimate();

    sBattleCamera.eye = camera->unk_60.eye;
    sBattleCamera.at = camera->unk_60.at;
    sBattleCamera.up = camera->unk_60.up;
    sBattleCamera.fovy = camera->unk_24.fovy;
    camera->unk_60.eye = view.eye;
    camera->unk_60.at = view.at;
    Randomizer_ShotUp(&view, &camera->unk_60.up);
    camera->unk_24.fovy = view.fovy;
    Randomizer_BattleCameraShow(TRUE);
    sSwapped = TRUE;
}

// As the battle starts (Randomizer_BattleUiEntry): the hook, with Options' "Battle camera" on
// Custom
void Randomizer_BattleCameraStart(void) {
    sSwapped = FALSE;
    sAttack = FALSE;
    sReplay = 0;
    sReplayAttack = FALSE;
    sAttacker = NULL;
    sShake = 0;
    sFrame = 0;
    sWiping = FALSE;
    sMoving = MOVE_NONE;
    sSpecies[0] = sSpecies[1] = -1;
    sRandom = osGetCount();
    if (!gRandomizerState.originalCamera) {
        gRandomizerState.battleCameraHook = Randomizer_BattleCamera;
    }
}

#endif
