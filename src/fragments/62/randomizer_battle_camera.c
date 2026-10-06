/*
 * The randomizer's battle camera (in randomizer_battleui, see randomizer_battle_ui.h): with
 * Options' "Battle camera" on Custom, about half of the ordinary attacks are filmed with the
 * shots the title screen films its demo battles with (src/randomizer_shots.h) instead of by
 * the battle's own camera: the attacker as its attack starts, then the defender as the hit
 * lands, reached by a cut or a quick swing of the camera, the camera shaking. A critical hit
 * or a Hyper Beam is always filmed, and its hit lands three times, from three angles, after
 * Groudon's Hyper Beam in Pokemon Battle Revolution's first trailer: the defender's reaction
 * starts again and the attacker goes back to just before the hit each time, while the
 * battle carries on (the damage, the HP bar and the message come once). The battle films the
 * rest itself, and everything else: the turns' openings, the moves with cameras of their own,
 * switching and fainting.
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
#define SCRIPT_ATTACK 2    // unk_38: an ordinary attack's camera script, or the other one
#define SCRIPT_ATTACK_ALT 3
#define STEP_ATTACKER 0  // unk_20: the camera going to the attacker,
#define STEP_ANIMATION 1 // its attack playing,
#define STEP_HIT 2       // the defender reacting to the hit

#define SIDES 2
#define FAINTED 0x10 // unk_654.unk_2D: a Pokemon that has fainted,
#define FAINTED_ALT 0x13
#define FILM_CHANCE 2    // one ordinary attack in this many is filmed by this file
#define HYPER_BEAM 63
#define REPLAY_TIMES 3   // a critical hit or a Hyper Beam lands this many times, from as many angles
#define REPLAY_FRAMES 30 // each time
#define REPLAY_LEAD 8    // the attack goes back this many frames before the hit each time
#define WHIP_FRAMES 8
#define SHAKE_FRAMES 14
#define SHAKE_SIZE 0.025f // of the camera's distance

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
static u8 sShown[SIDES]; // whether the battle shows each side's Pokemon, kept while this file's view is drawn
static RandomizerShot sShot;
static u8 sNextShot; // after a swing of the camera
static s16 sShake;   // frames of shaking left
static RandomizerShotView sLastView; // the last frame's, for the swing
static u8 sAttack;   // an ordinary attack is going on,
static u8 sFilming;  // filmed by this file,
static u8 sSawAttacker; // the battle's camera having looked at the attacker,
static u8 sHitLanded;   // and then at the defender, or the attack at its hit step
static u8 sBigHit;      // a critical hit or a Hyper Beam,
static u8 sReplay;      // which time its hit is landing (0 when it isn't being replayed)
static s16 sReplayFrame;
static s16 sImpactAnim; // the attacker's animation as the hit landed (-1 its idle stance),
static s16 sImpact;     // and its frame
static unk_D_84390010* sReplayed; // an attacker whose animation is being replayed,
static s16 sAfterAnim;            // and the one it goes back to as that ends (-1 none)
static unk_D_84390010* sAttacker;
static s32 sStep; // of the attack, the last frame
static u32 sRandom;

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

// The frame a model's animation is on
static s32 Randomizer_BattleCameraAnimFrame(unk_D_86002F58_004_000* model) {
    s32 frame = model->unk_040.unk_08 >> 16;

    return (frame < 0) ? 0 : frame;
}

/*
 * A critical hit or a Hyper Beam landing, the first time or again: low on the defender, then
 * over the attacker's shoulder, then close on the defender from its other side, shaking each
 * time. Again, the defender's hit reaction starts over (as the battle starts it,
 * func_8430897C) and the attacker's animation, unless it was back to its idle stance, goes
 * back to just before the hit.
 */
static void Randomizer_BattleCameraReplay(void) {
    static const u8 sReplayShots[REPLAY_TIMES] = { SHOT_HIT_LOW, SHOT_AIM, SHOT_HIT };
    unk_D_86002F58_004_000* attacker = &D_84390204->unk_000;
    s32 shot = sReplayShots[sReplay];

    if (sReplay == 0) {
        sImpactAnim = attacker->unk_040.unk_00;
        sImpact = Randomizer_BattleCameraAnimFrame(attacker);
        if (sImpactAnim == D_84384570[func_84307F00(D_84390204)]->unk_A50.unk_00) {
            sImpactAnim = -1;
        }
        sAfterAnim = -1;
    } else {
        if (sImpactAnim >= 0) {
            if (attacker->unk_040.unk_00 != sImpactAnim) {
                sAfterAnim = attacker->unk_040.unk_00;
                sReplayed = D_84390204;
            }
            func_8001BD04(attacker, sImpactAnim);
            func_80017464(attacker, MAX(sImpact - REPLAY_LEAD, 0));
        }
        func_8430897C(D_84390200);
    }
    Randomizer_ShotCut(&sShot, shot,
                       Randomizer_BattleCameraSideOf((shot == SHOT_AIM) ? D_84390204 : D_84390200));
    sShot.roll = (0x500 + Randomizer_BattleCameraBelow(0x400)) * sShot.turn;
    sShake = SHAKE_FRAMES;
    sReplay++;
    sReplayFrame = 0;
}

// A replayed attack going back, as it ends, to what the attacker had gone on to (its idle
// stance), unless the battle has given it another animation by then
static void Randomizer_BattleCameraReplayEnd(void) {
    unk_D_86002F58_004_000* model;

    if (sAfterAnim < 0) {
        return;
    }
    model = &sReplayed->unk_000;
    if (model->unk_040.unk_00 != sImpactAnim) {
        sAfterAnim = -1;
    } else if (func_80017514(model)) {
        func_8001BD04(model, sAfterAnim);
        sAfterAnim = -1;
    }
}

/*
 * Whether an ordinary attack is being filmed by this file this frame, deciding as each starts:
 * the director films a move, with an ordinary attack's script, its attacker using a move (one
 * that can't move uses none). The hit lands, for the camera, as the battle's own camera turns
 * from the attacker to the defender (where the battle has it look, camera's at, nearer the
 * defender), which for some moves is before the attack's hit step: their effects are drawn
 * over the screen where the defender is then (Scratch's claw marks and orange flash). Or at
 * the hit step, whichever comes first.
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
        sAttack = FALSE;
        return FALSE;
    }

    step = director->unk_20;
    if (!sAttack || (D_84390204 != sAttacker) || (step < sStep)) {
        // A new attack
        sAttack = TRUE;
        sAttacker = D_84390204;
        sFilming = (Randomizer_BattleCameraBelow(FILM_CHANCE) == 0);
#ifdef BATTLE_CAMERA_TEST_ALWAYS
        sFilming = TRUE;
#endif
        // A critical hit (worked out before the attack's animation starts, and set by a move
        // that does damage) or a Hyper Beam
        sBigHit = (D_843C5238->unk_44.unk_00 == HYPER_BEAM) || ((D_843C4DA5 != 0) && (D_843C5238->unk_44.unk_02 != 0));
#ifdef BATTLE_CAMERA_TEST_BIG
        sBigHit = TRUE;
#endif
        if (sBigHit) {
            sFilming = TRUE;
        }
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
    if (!sFilming) {
        return FALSE;
    }

    attacker = &sSides[Randomizer_BattleCameraSideOf(D_84390204)];
    defender = &sSides[Randomizer_BattleCameraSideOf(D_84390200)];
    atDefender = ABS(camera->unk_60.at.x - defender->middle.x) < ABS(camera->unk_60.at.x - attacker->middle.x);
    if (!atDefender) {
        sSawAttacker = TRUE;
    }
    if (!sHitLanded && ((step == STEP_HIT) || (sSawAttacker && atDefender))) {
        sHitLanded = TRUE;
        if (sBigHit) {
            Randomizer_BattleCameraReplay();
        } else {
            Randomizer_BattleCameraHitCut();
        }
    } else if ((sReplay != 0) && (++sReplayFrame >= REPLAY_FRAMES)) {
        if (sReplay < REPLAY_TIMES) {
            Randomizer_BattleCameraReplay();
        } else {
            sReplay = 0; // the last shot goes on to the attack's end
        }
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

/*
 * The Pokemon this file's view is of shown while it's drawn, or put back as the battle had
 * them. The battle shows both every frame, then hides the one its camera isn't on, or one
 * that has fainted (func_8432A578 and func_8432A510): the shot's own Pokemon is shown, and
 * the other too for a shot of both (from behind one, at the other); otherwise the other is
 * as the battle has it, out of the way of a hit's shots as an attacker lunges at its
 * target. A Pokemon underground or in the air is the battle's to draw as it is: not by this
 * flag, but see-through, or high up
 */
static void Randomizer_BattleCameraShow(s32 show) {
    s32 both = (sShot.kind == SHOT_SHOULDER) || (sShot.kind == SHOT_GAMEBOY) || (sShot.kind == SHOT_RBY) ||
               (sShot.kind == SHOT_SIGHTS) || (sShot.kind == SHOT_AIM);
    s32 i;

    for (i = 0; i < SIDES; i++) {
        unk_D_84390010* pokemon = D_84390010[i];
        u8* flags = &pokemon->unk_000.unk_000.unk_01;

        if (show) {
            sShown[i] = *flags & 1;
            if ((both || (i == sShot.side)) && (pokemon->unk_654.unk_2D != FAINTED) &&
                (pokemon->unk_654.unk_2D != FAINTED_ALT)) {
                *flags |= 1;
            }
        } else {
            *flags = (*flags & ~1) | sShown[i];
        }
    }
}

/*
 * The hook (gRandomizerState.battleCameraHook), every frame after the battle's director
 * has moved the camera (drawn 0) and once the scene is drawn (drawn 1)
 */
static void Randomizer_BattleCamera(unk_D_86002F34_00C* camera, s32 drawn) {
    RandomizerShotView view;

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
    Randomizer_BattleCameraSides();
    Randomizer_BattleCameraReplayEnd();
    if (!Randomizer_BattleCameraFilming(camera)) {
        return;
    }
    Randomizer_BattleCameraShoot(&view);

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
    sShake = 0;
    sAfterAnim = -1;
    sSpecies[0] = sSpecies[1] = -1;
    sRandom = osGetCount();
    if (!gRandomizerState.originalCamera) {
        gRandomizerState.battleCameraHook = Randomizer_BattleCamera;
    }
}

#endif
