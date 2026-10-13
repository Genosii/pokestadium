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
 * on into the battle's camera in place of the black jagged wipe, unless that hides something,
 * and of the closing circle after a switch. Switching, the camera looks up after the light of
 * the Pokemon called back, then moves to the Poke Ball thrown instead of cutting to it. The
 * battle films the rest itself, and everything else: the turns' openings, the moves with
 * cameras of their own, and fainting. Camera clips (randomizer_camera_clips.h,
 * docs/camera-editor.md), camera moves of keyframes made for a moment of the battle, play in
 * place of all of that as their moment starts.
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
#include "randomizer_camera_clips.h"

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
 * The line between the two (they stand on z 0). The camera can be on either side of it and go
 * across it during a shot, but a new shot starts on the side the last one ended on, so that
 * whoever was seen from their left the other is seen from their right (the 180 degree rule):
 * each of the camera's sources (the battle's own camera, this file's shots, a clip, the moves
 * in place of the wipe) decides as one of its shots starts whether to draw it mirrored across
 * the line, and keeps that for the whole shot (Randomizer_BattleCameraOtherSide).
 */
#define FIELD_LINE_Z 0.0f
/*
 * Nor does it stand between the two near the line, looking at one head on: there it's turned
 * round what it looks at to this far off the line on its side (35 degrees), so the next shot
 * of the other is the same turn the other way, by less the farther it is inside, out to the
 * full turn OFF_LINE_INSIDE units in from either Pokemon
 */
#define OFF_LINE_SIN 0.574f
#define OFF_LINE_COS 0.819f
#define OFF_LINE_INSIDE 40.0f
#define HANDOVER_FRAMES 24 // a cut to the battle's camera turned into a move this long
#define CUT_DISTANCE 80.0f // the battle's camera moving farther than this in a frame is a cut
/*
 * The circle the director closes and opens instead of the wipe at the end of a turn with a
 * switch or a faint (func_84329858, its unk_2E 0): the camera's iris (unk_CC, func_80011FC8),
 * closing (unk_00 5) then opening (4) over unk_08 frames that unk_0A counts, the scene not
 * drawn while it's closed (1). The director opens it at its unk_30 5 and waits at 6.
 */
#define CIRCLE_OPEN 0
#define CIRCLE_OPENING 4
#define CIRCLE_CLOSING 5
#define CIRCLE_OPENS 6
/*
 * Switching (move script 11, func_84327DC0): in its step 2 the battle waits 105 frames (unk_14
 * counting, func_8431ADAC) as the Pokemon called back turns into a light that rises off the
 * screen, then cuts to a view from the middle of the field and throws the Poke Ball. The
 * camera looks up after the light, holds on the sky, and looks back down at where the new one
 * comes out, the battle's count held back until it has (the ball thrown only then); it keeps
 * that view, from the side, until the new one is out and the script ends, then moves into the
 * battle's camera.
 */
#define SCRIPT_SWITCH 11
#define STEP_RECALL 2
#define RECALL_FRAMES 105 // the battle's wait
#define RISE_START 60     // frames into it: the light leaving its place
#define RISE_FRAMES 40
#define RISE_HOLD 18      // on the sky once the light has gone (0.3 seconds)
#define LOWER_FRAMES 45   // and back down, where it stands, at the spot where the Pokemon stood,
#define SPOT_RAISE 30.0f  // this far above the ground,
#define LOWER_ZOOM 0.5f   // zooming in as it does (its field of view this much as wide), so that the
                          // ball comes in from off the screen
#define RISE 0.5f         // of the camera's distance: how far up it looks once the light has gone
#define STEP_APPEARS 6    // the Poke Ball open, the new one coming out of its light
#define CLOSE_IN_FRAMES 45 // the camera closing in on it, from the side, as it does,
#define CLOSE_IN_FRAMING 2.6f // to this far from it (heights)
#define SEND_OUT_FRAMES 40 // from its view of the new one into the battle's camera once it's out
#define SWITCH_NONE 0
#define SWITCH_VIEW 1  // Randomizer_BattleCameraSwitch's view is drawn
#define SWITCH_ENDED 2 // the switch has just ended
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
 * The camera clips (randomizer_camera_clips.h): a clip's frames are 60ths of a second and the
 * hook runs 60 times a second (docs/game-engine.md), so a clip goes on CLIP_TICKS a run. The
 * director's move scripts the moments start at (docs/camera-editor.md), and Gen 1's split
 */
#define CLIP_TICKS 1
#define SCRIPT_OPENING 1
#define SCRIPT_TURN_END 4
#define SCRIPT_SEND_OUT 18 // after a faint
#define SCRIPT_RECALL 33   // another recall (script 11 is the player's switch)
#define TYPE_FIRE 20       // the first special type in Gen 1
#define DEG_TO_ANGLE(tenths) ((tenths) * (0x10000 / 3600.0f))

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
// Each source's shot drawn mirrored across the line, decided as it starts (FIELD_LINE_Z): the
// battle's own camera (decided again whenever it isn't what's drawn), this file's shots, the
// move in place of the wipe and the clip playing
static u8 sOwnMirror;
static u8 sOwnDrawn; // the battle's own camera was what was drawn the last frame
static u8 sShotMirror;
static u8 sMoveMirror;
static u8 sClipMirror;
static u8 sClipHandover; // the move after it is a clip's blend out, which this file's shots don't cut short
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
static s16 sHandFrames; // out of this many,
static RandomizerShotView sHandFrom; // from this view
static RandomizerShotView sLastOwn;  // the battle's view last frame
static s32 sLastKey;    // its mode, script and attacker last frame,
static s16 sKeyAge;     // and how many frames since they changed
static u8 sCircle;      // the move is in place of the circle, not the wipe
static s32 sSwitchStep; // a switch's step last frame (-1 none),
static s16 sSwitchFrame; // frames into its step 2,
static RandomizerShotView sSendOutView; // and the view kept as the new one comes out,
static s16 sCloseFrame; // closing in on it,
static f32 sWideFovy;   // its field of view widening back to this
static u8 sIdleShot;   // one of the title's shots is on in the menus (sShot)
static s8 sSplitHalf = -1; // the half of a split screen being drawn (0 the left, 1 the right),
static unk_D_86002F34_00C* sCamera;      // with the battle's camera,
static unk_D_86002F34_00C_018 sViewport; // as wide as the screen,
static unk_D_86002F34_00C_024 sProjection;
static unk_D_86002F34_00C_040 sOrtho;
static s16 sIdleAnim[SIDES];  // the animation and frame each side's model was on last frame
static s32 sIdleFrame[SIDES];
static u64 sClipBuffer[CLIP_TABLE_SIZE / sizeof(u64)]; // the clip table, read from the ROM (DMA: 8-byte aligned)
#define sClips (*(RandomizerClipTable*)sClipBuffer)
static RandomizerClip* sClip; // the clip playing (NULL none),
static u8 sClipMoment;        // the moment it's for,
static u8 sClipSide;          // that moment's Pokemon,
static s16 sClipFrame;        // and how far into it (60ths of a second),
static RandomizerShotView sClipFrom; // the view it blends in from
static s32 sMomentKey;        // the moment the battle is at last frame, its Pokemon and which time
static unk_D_84390010* sClipAttacker; // the attack going on (NULL none), its step last frame,
static s32 sClipStep;
static s32 sClipAttacks;      // and how many there have been
static u8 sClipSawAttacker;   // in an attack, the battle's camera having looked at the attacker,
static u8 sClipHitLanded;     // and then at the defender, or the attack at its hit step;
static u8 sClipTake;          // the times a big hit has landed so far
static u8 sFainted[SIDES];    // each side's Pokemon having fainted, last frame

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

static s32 Randomizer_BattleCameraOtherSide(RandomizerShotView* first);
static void Randomizer_BattleCameraMirror(RandomizerShotView* view);

// Whether the shot this file has just cut to (or the one a swing goes to) is drawn mirrored: if
// its first frame is on the other side of the line from the view drawn last
static void Randomizer_BattleCameraShotSide(void) {
    RandomizerShotView first;
    s32 kind = (sShot.kind == SHOT_WHIP) ? sNextShot : sShot.kind;

    if (Randomizer_ShotCompose(&first, kind, &sShot, 0, &sSides[sShot.side], &sSides[sShot.side ^ 1], &sSides[0],
                               &sSides[1])) {
        sShotMirror = Randomizer_BattleCameraOtherSide(&first);
    }
}

/*
 * One of the title's shots of the field or of one of them, in the menus, set up to stay on one
 * side of the field the whole time (z above 0: in the shots that go round, the yaw's cosine
 * above 0 from the shot's start to its end; in the others, the way they turn), and mirrored
 * to the camera's side as it starts (Randomizer_BattleCameraShotSide)
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
    Randomizer_BattleCameraShotSide();
}

// The attacker, as its attack starts: any of the attacker's shots
static void Randomizer_BattleCameraAttackCut(void) {
    Randomizer_ShotCut(&sShot, SHOT_LOW + Randomizer_BattleCameraBelow(NUM_ATTACK_SHOTS),
                       Randomizer_BattleCameraSideOf(D_84390204));
    Randomizer_BattleCameraShotSide();
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
    Randomizer_BattleCameraShotSide();
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
    Randomizer_BattleCameraShotSide();
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
            Randomizer_BattleCameraShotSide();
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
        if (sShotMirror) {
            Randomizer_BattleCameraMirror(&next);
        }
        ease = Randomizer_ShotEase((f32)sShot.frame / WHIP_FRAMES);
        *view = sLastView; // as it was drawn
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
        if (sShotMirror) {
            Randomizer_BattleCameraMirror(view);
        }
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
 * Both Pokemon from the side, square to the line between them (on z's + side; mirrored by the
 * move that goes there if the camera is on the other), a little above them, as far as it takes
 * to see them whole
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

// A view mirrored across the line between the two
static void Randomizer_BattleCameraMirror(RandomizerShotView* view) {
    view->eye.z = (2.0f * FIELD_LINE_Z) - view->eye.z;
    view->at.z = (2.0f * FIELD_LINE_Z) - view->at.z;
    view->roll = -view->roll;
}

// Whether a shot starting with this view is on the other side of the line from the view drawn
// last, the one before it: drawn mirrored, it carries on the screen's directions
static s32 Randomizer_BattleCameraOtherSide(RandomizerShotView* first) {
    return (first->eye.z < FIELD_LINE_Z) != (sDrawnView.eye.z < FIELD_LINE_Z);
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

static void Randomizer_BattleCameraOffLine(RandomizerShotView* view) {
    f32 left = MIN(sSides[0].middle.x, sSides[1].middle.x);
    f32 right = MAX(sSides[0].middle.x, sSides[1].middle.x);
    f32 inside = MIN(view->eye.x - left, right - view->eye.x);
    f32 dx = view->eye.x - view->at.x;
    f32 dz = view->eye.z - view->at.z;
    f32 h = sqrtf(SQ(dx) + SQ(dz));
    // Turned off the line on the side the camera was on last frame: right on the line, the
    // side the view itself is on would come and go from frame to frame
    f32 side = (sDrawnView.eye.z < FIELD_LINE_Z) ? -1.0f : 1.0f;
    f32 t;
    f32 x;
    f32 z;
    f32 size;

    if ((inside <= 0.0f) || (h < 1.0f) || (dz * side >= h * OFF_LINE_SIN)) {
        return;
    }
    t = Randomizer_ShotEase(inside / OFF_LINE_INSIDE);
    x = dx + ((((dx < 0.0f) ? -h : h) * OFF_LINE_COS) - dx) * t;
    z = dz + ((side * h * OFF_LINE_SIN) - dz) * t;
    size = sqrtf(SQ(x) + SQ(z));
    if (size > 0.01f) {
        view->eye.x = view->at.x + (x * h / size);
        view->eye.z = view->at.z + (z * h / size);
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

// Whether a Pokemon has fainted, which the circle after it hides being taken away
static s32 Randomizer_BattleCameraFainted(void) {
    s32 i;

    for (i = 0; i < SIDES; i++) {
        if ((D_84390010[i]->unk_654.unk_2D == FAINTED) || (D_84390010[i]->unk_654.unk_2D == FAINTED_ALT)) {
            return TRUE;
        }
    }
    return FALSE;
}

/*
 * The circle, while the camera moves in its place: told it has closed or opened as it starts
 * to, so that the scene is drawn throughout and the director goes on
 */
static void Randomizer_BattleCameraNoCircle(unk_D_86002F34_00C* camera) {
    unk_D_86002F34_00C_0CC* circle = &camera->unk_CC;

    if (((circle->unk_00 == CIRCLE_CLOSING) || (circle->unk_00 == CIRCLE_OPENING)) &&
        (circle->unk_0A < circle->unk_08)) {
        circle->unk_00 = CIRCLE_OPEN;
        circle->unk_0A = circle->unk_08;
    }
}

// The view of the new one, closing in on it from where the camera looks down at its place
static void Randomizer_BattleCameraCloseIn(RandomizerShotView* view, s32 start) {
    RandomizerShotSide* side = &sSides[(D_84390240.unk_00->unk_2C != 0) ? 1 : 0];
    Vec3f dir;
    f32 distance;
    f32 t;

    if (start) {
        sCloseFrame = 0;
    }
    func_8000E88C(&dir, view->eye.x - view->at.x, view->eye.y - view->at.y, view->eye.z - view->at.z);
    Randomizer_ShotNormalize(&dir);
    distance = Randomizer_ShotDistance(side, CLOSE_IN_FRAMING);
    t = Randomizer_ShotEase((f32)sCloseFrame / CLOSE_IN_FRAMES);
    func_8000E88C(&view->eye, view->eye.x + ((side->middle.x + (dir.x * distance) - view->eye.x) * t),
                  view->eye.y + ((side->middle.y + (dir.y * distance) - view->eye.y) * t),
                  view->eye.z + ((side->middle.z + (dir.z * distance) - view->eye.z) * t));
    func_8000E88C(&view->at, view->at.x + ((side->middle.x - view->at.x) * t),
                  view->at.y + ((side->middle.y - view->at.y) * t), view->at.z + ((side->middle.z - view->at.z) * t));
    view->fovy += (sWideFovy - view->fovy) * t; // the zoom let go of as the camera moves in
    if (sCloseFrame < CLOSE_IN_FRAMES) {
        sCloseFrame++;
    }
}

/*
 * Switching: the view while the Pokemon called back goes and the new one comes out
 * (SWITCH_VIEW), or SWITCH_ENDED as the script ends
 */
static s32 Randomizer_BattleCameraSwitch(RandomizerShotView* own, RandomizerShotView* view) {
    unk_D_84390240_000* director = D_84390240.unk_00;
    s32 step = ((director != NULL) && (director->unk_1C == CAMERA_MODE_MOVE) && (director->unk_38 == SCRIPT_SWITCH))
                   ? director->unk_20
                   : -1;
    s32 last = sSwitchStep;
    f32 rise;
    f32 lower;
    Vec3f spot;

    sSwitchStep = step;
    if (step == STEP_RECALL) {
        if (last != STEP_RECALL) {
            sSwitchFrame = 0;
        }
        // Up after the light, held there, and back down
        rise = Randomizer_ShotEase(MAX(sSwitchFrame - RISE_START, 0) / (f32)RISE_FRAMES);
        lower = Randomizer_ShotEase(MAX(sSwitchFrame - (RISE_START + RISE_FRAMES + RISE_HOLD), 0) / (f32)LOWER_FRAMES);
        rise *= 1.0f - lower;
        if ((sSwitchFrame < RISE_START + RISE_FRAMES + RISE_HOLD + LOWER_FRAMES) &&
            (director->unk_14 > RECALL_FRAMES - 2) && ((sClip == NULL) || (sClipMoment != CLIP_MOMENT_RECALL))) {
            director->unk_14 = RECALL_FRAMES - 2; // the throw waits for the camera
        }
        sSwitchFrame++;
        *view = *own;
        view->at.y += rise * RISE *
                      sqrtf(SQ(own->at.x - own->eye.x) + SQ(own->at.y - own->eye.y) + SQ(own->at.z - own->eye.z));
        // Down at the spot, from where the camera is, zooming in
        sWideFovy = view->fovy;
        if (lower > 0.0f) {
            spot = D_84390010[(director->unk_2C != 0) ? 1 : 0]->unk_000.unk_024;
            func_8000E88C(&view->at, view->at.x + ((spot.x - view->at.x) * lower),
                          view->at.y + ((spot.y + SPOT_RAISE - view->at.y) * lower),
                          view->at.z + ((spot.z - view->at.z) * lower));
            view->fovy *= 1.0f - ((1.0f - LOWER_ZOOM) * lower);
        }
        sSendOutView = *view;
        return SWITCH_VIEW;
    }
    if (step > STEP_RECALL) {
        *view = sSendOutView;
        if (step >= STEP_APPEARS) {
            Randomizer_BattleCameraCloseIn(view, last < STEP_APPEARS);
        }
        return SWITCH_VIEW;
    }
    return (last >= STEP_RECALL) ? SWITCH_ENDED : SWITCH_NONE;
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
    s32 wiping = (director != NULL) && (director->unk_30 >= WIPE_ASKED);
    s32 circle = wiping && (director->unk_2E == 0);
    s32 ready = (director != NULL) && (director->unk_1C == CAMERA_MODE_MENUS) && (director->unk_30 == 0) &&
                (D_84390010[0]->unk_654.unk_10 == MENU_READY);
    RandomizerShotView next;

    if (wiping && !sWiping) {
        sCircle = circle;
        if (Randomizer_BattleCameraKeepsWipe() || (circle && Randomizer_BattleCameraFainted())) {
            Randomizer_BattleCameraWipeShown(TRUE);
            sMoving = MOVE_NONE;
        } else {
            // From where it is: the camera may already be on its way to both, since the pick
            // (the circle is done with in Randomizer_BattleCameraNoCircle)
            Randomizer_BattleCameraWipeShown(circle);
            sMoving = MOVE_IN;
            sMoveFrame = 0;
            sMoveFrom = sDrawnView;
            sMoveMirror = (sDrawnView.eye.z < FIELD_LINE_Z); // both and the opening are made on z's + side
        }
    } else if (ready && (sMoving == MOVE_NONE) && !sMovedReady) {
        // The player has picked: on to both while the battle gets the turn ready
        sMoving = MOVE_IN;
        sMoveFrame = 0;
        sMoveFrom = sDrawnView;
        sMoveMirror = (sDrawnView.eye.z < FIELD_LINE_Z);
        sMovedReady = TRUE;
    }
    if (!ready) {
        sMovedReady = FALSE;
    }
    sWiping = wiping;
    if (sMoving == MOVE_NONE) {
        return FALSE;
    }
    if ((sMoving == MOVE_IN) && (wiping ? (director->unk_30 >= (circle ? CIRCLE_OPENS : WIPE_OPENING)) : !ready)) {
        sMoving = MOVE_OUT;
        sOutFrame = 0;
    }

    if (wiping && (director->unk_44 == 2)) {
        Randomizer_BattleCameraOpening(&next);
    } else {
        Randomizer_BattleCameraBoth(&next);
    }
    if (sMoveMirror) {
        Randomizer_BattleCameraMirror(&next); // on the side the camera was when it set off
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
    return sMoving || (sHandover > 0) || (sClip != NULL) || (side == sShot.side) || (sShot.kind == SHOT_SHOULDER) ||
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
 * The moment the battle is at (randomizer_camera_clips.h), with its Pokemon (side) and, for a
 * big hit, how many times it has landed (take), from the director's mode, script and step;
 * not a faint, which starts as a Pokemon faints. An attack's hit lands, for the clips, as for
 * this file's shots (Randomizer_BattleCameraFilming): as the battle's camera (own) turns from
 * the attacker to the defender, or at the hit step.
 */
static s32 Randomizer_ClipMoment(RandomizerShotView* own, s32* side, s32* take) {
    unk_D_84390240_000* director = D_84390240.unk_00;
    RandomizerShotSide* attacker;
    RandomizerShotSide* defender;
    s32 script;
    s32 step;

    *side = 0;
    *take = 0;
    if ((director == NULL) || (director->unk_1C != CAMERA_MODE_MOVE)) {
        sClipAttacker = NULL;
        return ((director != NULL) && (director->unk_1C == CAMERA_MODE_MENUS) && (director->unk_30 == 0))
                   ? CLIP_MOMENT_MENUS
                   : CLIP_MOMENT_NONE;
    }
    script = director->unk_38;
    step = director->unk_20;
    if ((script != SCRIPT_ATTACK) && (script != SCRIPT_ATTACK_ALT)) {
        sClipAttacker = NULL;
    }
    switch (script) {
        case SCRIPT_OPENING:
            *side = director->unk_2A & 1; // the first to attack
            return CLIP_MOMENT_OPENING;
        case SCRIPT_TURN_END:
            return CLIP_MOMENT_TURN_END;
        case SCRIPT_SWITCH:
        case SCRIPT_RECALL:
            *side = (director->unk_2C != 0) ? 1 : 0;
            return (step <= STEP_RECALL) ? CLIP_MOMENT_RECALL : CLIP_MOMENT_SEND_OUT;
        case SCRIPT_SEND_OUT:
            *side = (director->unk_2C != 0) ? 1 : 0;
            return CLIP_MOMENT_SEND_OUT;
        case SCRIPT_ATTACK:
        case SCRIPT_ATTACK_ALT:
            break;
        default:
            return CLIP_MOMENT_NONE;
    }
    if ((D_84390204 == NULL) || (D_84390200 == NULL) || (D_843C5238 == NULL) || (D_843C5238->unk_44.unk_00 == 0) ||
        (step > STEP_HIT)) {
        return CLIP_MOMENT_NONE;
    }
    if ((D_84390204 != sClipAttacker) || (step < sClipStep)) {
        // A new attack
        sClipAttacker = D_84390204;
        sClipAttacks++;
        sClipSawAttacker = FALSE;
        sClipHitLanded = FALSE;
        sClipTake = 0;
    }
    sClipStep = step;
    *side = Randomizer_BattleCameraSideOf(D_84390204);
    if (D_843C5238->unk_44.unk_02 == 0) {
        return CLIP_MOMENT_STATUS;
    }
    attacker = &sSides[*side];
    defender = &sSides[*side ^ 1];
    if (ABS(own->at.x - defender->middle.x) >= ABS(own->at.x - attacker->middle.x)) {
        sClipSawAttacker = TRUE;
    } else if (sClipSawAttacker) {
        sClipHitLanded = TRUE;
    }
    if (step == STEP_HIT) {
        sClipHitLanded = TRUE;
    }
    if (!sClipHitLanded) {
        return (step == STEP_ATTACKER) ? CLIP_MOMENT_WINDUP : CLIP_MOMENT_ANIMATION;
    }
    // A big hit lands REPLAY_TIMES times (sReplay counting them, 0 again after the last)
    if (sReplay > sClipTake) {
        sClipTake = sReplay;
    }
    *take = sClipTake;
    return (sClipTake > 1) ? CLIP_MOMENT_REPLAY : CLIP_MOMENT_HIT;
}

// Whether a clip is for a moment, its Pokemon's side and, in an attack, the move
static s32 Randomizer_ClipFits(RandomizerClip* clip, s32 moment, s32 side) {
    s32 power;
    s32 type;

    if ((clip->moment != moment) || (clip->mode == CLIP_MODE_OFF) || (clip->numKeys == 0)) {
        return FALSE;
    }
    if ((clip->side != CLIP_SIDE_ANY) && (moment != CLIP_MOMENT_MENUS) && (moment != CLIP_MOMENT_TURN_END) &&
        (side != (clip->side - CLIP_SIDE_PLAYER))) {
        return FALSE;
    }
    if ((moment < CLIP_MOMENT_WINDUP) || (moment > CLIP_MOMENT_STATUS)) {
        return TRUE; // no move
    }
    power = D_843C5238->unk_44.unk_02;
    type = D_843C5238->unk_44.unk_03;
    if ((clip->type != CLIP_TYPE_ANY) && (clip->type != type)) {
        return FALSE;
    }
    switch (clip->category) {
        case CLIP_CATEGORY_PHYSICAL:
            return (power != 0) && (type < TYPE_FIRE);
        case CLIP_CATEGORY_SPECIAL:
            return (power != 0) && (type >= TYPE_FIRE);
        case CLIP_CATEGORY_STATUS:
            return power == 0;
    }
    return TRUE;
}

/*
 * The clip for a moment that starts: one of those always played for it, or one from the pool
 * by weight, against CLIP_AUTO_WEIGHT for the automatic shots (NULL)
 */
static RandomizerClip* Randomizer_ClipPick(s32 moment, s32 side) {
    s32 count = MIN(sClips.numClips, CLIP_MAX);
    s32 always = 0;
    s32 pool = 0;
    s32 n;
    s32 i;

    for (i = 0; i < count; i++) {
        RandomizerClip* clip = &sClips.clips[i];

        if (Randomizer_ClipFits(clip, moment, side)) {
            if (clip->mode == CLIP_MODE_ALWAYS) {
                always++;
            } else if (clip->mode == CLIP_MODE_POOL) {
                pool += clip->weight;
            }
        }
    }
    if (always > 0) {
        n = Randomizer_BattleCameraBelow(always);
    } else if (pool > 0) {
        n = Randomizer_BattleCameraBelow(CLIP_AUTO_WEIGHT + pool) - CLIP_AUTO_WEIGHT;
    } else {
        return NULL;
    }
    for (i = 0; (i < count) && (n >= 0); i++) {
        RandomizerClip* clip = &sClips.clips[i];

        if (!Randomizer_ClipFits(clip, moment, side)) {
            continue;
        }
        if (always > 0) {
            if ((clip->mode == CLIP_MODE_ALWAYS) && (n-- == 0)) {
                return clip;
            }
        } else if (clip->mode == CLIP_MODE_POOL) {
            n -= clip->weight;
            if (n < 0) {
                return clip;
            }
        }
    }
    return NULL;
}

/*
 * Where a keyframe's anchor is (the point on it), how tall it is, and the yaw from which the
 * camera sees its front: a Pokemon's, or between the two (or the middle of the field), as tall
 * as the taller, facing as the moment's Pokemon does; or the arena itself, the same whichever
 * side is which
 */
static void Randomizer_ClipAnchor(s32 anchor, s32 point, Vec3f* out, f32* height, s16* facing) {
    RandomizerShotSide* side;
    Vec3f* feet0 = &D_84390010[0]->unk_000.unk_024;
    Vec3f* feet1 = &D_84390010[1]->unk_000.unk_024;
    s32 i;

    if (anchor == CLIP_ANCHOR_FIXED) {
        func_8000E88C(out, 0.0f, 0.0f, 0.0f);
        *height = CLIP_FIXED_HEIGHT;
        *facing = 0x4000;
        return;
    }
    switch (anchor) {
        case CLIP_ANCHOR_ATTACKER:
            i = (D_84390204 != NULL) ? Randomizer_BattleCameraSideOf(D_84390204) : 0;
            break;
        case CLIP_ANCHOR_DEFENDER:
            i = (D_84390200 != NULL) ? Randomizer_BattleCameraSideOf(D_84390200) : 1;
            break;
        case CLIP_ANCHOR_SUBJECT:
            i = sClipSide;
            break;
        default:
            i = -1;
            break;
    }
    if (i >= 0) {
        side = &sSides[i];
        *height = side->height;
        *facing = (i == 0) ? 0x4000 : -0x4000;
        if (point == CLIP_POINT_FEET) {
            *out = D_84390010[i]->unk_000.unk_024;
        } else if (point == CLIP_POINT_HEAD) {
            Randomizer_ShotHead(side, out);
        } else {
            *out = side->middle;
        }
        return;
    }
    *height = MAX(sSides[0].height, sSides[1].height);
    *facing = (sClipSide == 0) ? 0x4000 : -0x4000;
    if (point == CLIP_POINT_FEET) {
        func_8000E88C(out, (feet0->x + feet1->x) * 0.5f, (feet0->y + feet1->y) * 0.5f, (feet0->z + feet1->z) * 0.5f);
    } else {
        func_8000E88C(out, (sSides[0].middle.x + sSides[1].middle.x) * 0.5f,
                      (sSides[0].middle.y + sSides[1].middle.y) * 0.5f,
                      (sSides[0].middle.z + sSides[1].middle.z) * 0.5f);
    }
    if (anchor == CLIP_ANCHOR_CENTRE) {
        out->x = 0.0f;
        out->z = 0.0f;
    }
}

/*
 * Where a keyframe puts the camera: yaw (tenths of a degree) round its anchor's middle from its
 * front, toward the camera's side of the field for either side's Pokemon, pitch above it,
 * distance (hundredths of its height) away. Exactly there: the editor places it
 */
static void Randomizer_ClipEye(s32 anchor, f32 yaw, f32 pitch, f32 distance, Vec3f* eye) {
    Vec3f point;
    f32 height;
    s16 facing;
    s16 turn;

    Randomizer_ClipAnchor(anchor, CLIP_POINT_MIDDLE, &point, &height, &facing);
    turn = (s32)DEG_TO_ANGLE(yaw);
    func_80010354(&point, eye, height * distance / 100.0f, (s32)DEG_TO_ANGLE(pitch),
                  (facing > 0) ? (facing - turn) : (facing + turn));
}

// What a keyframe looks at: the point on its anchor, offset along its facing, up and toward
// the camera's side of the field (hundredths of its height)
static void Randomizer_ClipLook(RandomizerClipKey* key, Vec3f* at) {
    f32 height;
    s16 facing;
    f32 scale;

    Randomizer_ClipAnchor(key->lookAnchor, key->lookPoint, at, &height, &facing);
    scale = height / 100.0f;
    at->x += SINS(facing) * key->lookOffset[0] * scale;
    at->y += key->lookOffset[1] * scale;
    at->z += ((COSS(facing) * key->lookOffset[0]) + key->lookOffset[2]) * scale;
}

// A cubic Bezier curve from 0 to 1 through control values a and b, at s
static f32 Randomizer_ClipCurve(f32 s, f32 a, f32 b) {
    f32 r = 1.0f - s;

    return (3.0f * r * s * ((r * a) + (s * b))) + (s * s * s);
}

/*
 * How far (0 to 1) from a keyframe to the next its easing has gone, t of the time between them:
 * the easings are Bezier curves (x1, y1, x2, y2 in thousandths, as CSS's cubic-bezier()), the
 * curve's x found for t by halving
 */
static f32 Randomizer_ClipEase(RandomizerClipKey* key, f32 t) {
    static const s16 sCurves[][4] = {
        { 0, 0, 1000, 1000 },   // linear
        { 420, 0, 1000, 1000 }, // ease in
        { 0, 0, 580, 1000 },    // ease out
        { 420, 0, 580, 1000 },  // ease in and out
    };
    const s16* curve = (key->easing <= CLIP_EASE_IN_OUT) ? sCurves[key->easing] : sCurves[CLIP_EASE_LINEAR];
    f32 lo = 0.0f;
    f32 hi = 1.0f;
    f32 s = t;
    s32 i;

    if (key->easing == CLIP_EASE_HOLD) {
        return 0.0f;
    }
    if (key->easing == CLIP_EASE_BEZIER) {
        curve = key->bezier;
    }
    for (i = 0; i < 12; i++) {
        s = (lo + hi) * 0.5f;
        if (Randomizer_ClipCurve(s, curve[0] / 1000.0f, curve[2] / 1000.0f) < t) {
            lo = s;
        } else {
            hi = s;
        }
    }
    return Randomizer_ClipCurve(s, curve[1] / 1000.0f, curve[3] / 1000.0f);
}

/*
 * The clip playing's view this frame, between the keyframes either side of it: the camera
 * going round its anchor where both are round the same one (unless the first goes straight),
 * straight across otherwise; what it looks at, its tilt (mirrored for the other side's
 * Pokemon, like the yaw, unless the camera is fixed) and its field of view straight across
 */
static void Randomizer_ClipView(RandomizerShotView* view) {
    RandomizerClip* clip = sClip;
    s32 last = MIN(clip->numKeys, CLIP_MAX_KEYS) - 1;
    RandomizerClipKey* from;
    RandomizerClipKey* to;
    f32 t = 0.0f;
    f32 roll;
    Vec3f eye;
    Vec3f at;
    s32 i;

    for (i = 0; (i < last) && (sClipFrame >= clip->keys[i + 1].frame); i++) {}
    from = &clip->keys[i];
    to = &clip->keys[(i < last) ? (i + 1) : i];
    if ((to->frame > from->frame) && (sClipFrame > from->frame)) {
        t = Randomizer_ClipEase(from, (f32)(sClipFrame - from->frame) / (to->frame - from->frame));
    }
    if ((from->eyeAnchor == to->eyeAnchor) && !(from->flags & CLIP_KEY_STRAIGHT)) {
        Randomizer_ClipEye(from->eyeAnchor, from->yaw + ((to->yaw - from->yaw) * t),
                           from->pitch + ((to->pitch - from->pitch) * t),
                           from->distance + (((f32)to->distance - from->distance) * t), &view->eye);
    } else {
        Randomizer_ClipEye(from->eyeAnchor, from->yaw, from->pitch, from->distance, &view->eye);
        Randomizer_ClipEye(to->eyeAnchor, to->yaw, to->pitch, to->distance, &eye);
        func_8000E88C(&view->eye, view->eye.x + ((eye.x - view->eye.x) * t), view->eye.y + ((eye.y - view->eye.y) * t),
                      view->eye.z + ((eye.z - view->eye.z) * t));
    }
    Randomizer_ClipLook(from, &view->at);
    Randomizer_ClipLook(to, &at);
    func_8000E88C(&view->at, view->at.x + ((at.x - view->at.x) * t), view->at.y + ((at.y - view->at.y) * t),
                  view->at.z + ((at.z - view->at.z) * t));
    roll = from->roll + ((to->roll - from->roll) * t);
    view->roll = (s32)DEG_TO_ANGLE(((sClipSide == 0) || (from->eyeAnchor == CLIP_ANCHOR_FIXED)) ? roll : -roll);
    view->fovy = (from->fovy + (((f32)to->fovy - from->fovy) * t)) / 100.0f;
    if (view->fovy < 1.0f) {
        view->fovy = CAMERA_FOVY;
    }
}

// The clip playing stopped, the camera moving from it into what comes next over its blend out
// (a cut if 0); a move in place of the wipe going on under it sets off again from where the clip
// ended, on its side
static void Randomizer_ClipStop(void) {
    if (sClip != NULL) {
        sHandover = sHandFrames = sClip->blendOut;
        sHandFrom = sDrawnView;
        sClipHandover = TRUE;
        if (sMoving != MOVE_NONE) {
            sMoveFrom = sDrawnView;
            sMoveFrame = 0;
            sMoveMirror = (sDrawnView.eye.z < FIELD_LINE_Z);
        }
        sClip = NULL;
    }
}

/*
 * The clips, every frame, before the view is chosen: a clip picked as a moment starts (the
 * battle getting to it, a big hit landing again, a Pokemon fainting, or, in the menus, where
 * the battle's camera cuts to another shot), playing until its last keyframe or until the
 * moment it's for is over (a faint's and the menus' until another starts). own is the battle's
 * view, cut whether it has just cut. Returns whether a clip is playing.
 */
static s32 Randomizer_BattleCameraClips(RandomizerShotView* own, s32 cut) {
    unk_D_84390240_000* director = D_84390240.unk_00;
    s32 side;
    s32 take;
    s32 moment = Randomizer_ClipMoment(own, &side, &take);
    s32 key = moment | (side << 8) | (take << 12) | (sClipAttacks << 16);
    s32 start = CLIP_MOMENT_NONE;
    s32 fainted;
    s32 i;
    RandomizerClip* clip;
    RandomizerShotView first;

    if (key != sMomentKey) {
        sMomentKey = key;
        if ((moment != CLIP_MOMENT_NONE) && (moment != CLIP_MOMENT_MENUS)) {
            start = moment;
        }
        if ((sClip != NULL) && ((start != CLIP_MOMENT_NONE) ||
                                ((sClipMoment != CLIP_MOMENT_FAINT) && (sClipMoment != CLIP_MOMENT_MENUS)) ||
                                ((sClipMoment == CLIP_MOMENT_MENUS) && (moment != CLIP_MOMENT_MENUS)))) {
            Randomizer_ClipStop();
        }
    }
    for (i = 0; i < SIDES; i++) {
        fainted = (D_84390010[i]->unk_654.unk_2D == FAINTED) || (D_84390010[i]->unk_654.unk_2D == FAINTED_ALT);
        if (fainted && !sFainted[i]) {
            start = CLIP_MOMENT_FAINT;
            side = i;
        }
        sFainted[i] = fainted;
    }
    if ((moment == CLIP_MOMENT_MENUS) && cut && (sClip == NULL)) {
        start = CLIP_MOMENT_MENUS;
    }
    if (start != CLIP_MOMENT_NONE) {
        clip = Randomizer_ClipPick(start, side);
        if (clip != NULL) {
            sClip = clip;
            sClipMoment = start;
            sClipSide = side;
            sClipFrame = 0;
            sClipFrom = sDrawnView;
            sHandover = 0;
            Randomizer_ClipView(&first);
            sClipMirror = Randomizer_BattleCameraOtherSide(&first);
        }
    }
    if (sClip == NULL) {
        return FALSE;
    }
    if (sClipFrame > sClip->keys[MIN(sClip->numKeys, CLIP_MAX_KEYS) - 1].frame) {
        Randomizer_ClipStop();
        return FALSE;
    }
    // The Poke Ball is thrown once the recall's clip has played (Randomizer_BattleCameraSwitch)
    if ((sClipMoment == CLIP_MOMENT_RECALL) && (director != NULL) && (director->unk_20 == STEP_RECALL) &&
        (director->unk_14 > RECALL_FRAMES - 2)) {
        director->unk_14 = RECALL_FRAMES - 2;
    }
    return TRUE;
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
    s32 switching;
    s32 clip;
    s32 ownDrawn = FALSE; // what's drawn this frame is the battle's own camera
    RandomizerShotView switchView;

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
#ifdef BATTLE_CAMERA_TEST_FAINT
    // The other side's Pokemon left with 1 HP (D_800FCB18's unk_0C) in the menus, so that the
    // turn's first hit on it makes it faint, and the next comes out
    if ((director != NULL) && (director->unk_1C == CAMERA_MODE_MENUS) && (D_800FCB18[1] != NULL) &&
        (D_800FCB18[1]->unk_0C > 1)) {
        D_800FCB18[1]->unk_0C = 1;
    }
#endif
#ifdef BATTLE_CAMERA_TEST_LOG
    // For reading from a savestate: frames the hook has run, and the clip playing
    ((u32*)CLIP_TABLE_ADDRESS)[1] = sFrame;
    ((u32*)CLIP_TABLE_ADDRESS)[2] = (sClip != NULL) ? ((sClipMoment << 16) | (u16)sClipFrame) : 0;
    ((u32*)CLIP_TABLE_ADDRESS)[3] = sMomentKey;
#endif

    Randomizer_BattleCameraOwnView(&own, camera);
    // The battle's mode, its script in a move, and the attacker: a change of any is a cut the
    // camera may smooth
    key = (director == NULL) ? 0
                             : ((director->unk_1C << 16) |
                                ((director->unk_1C == CAMERA_MODE_MOVE) ? (director->unk_38 << 8) : 0) |
                                (D_84390204 == D_84390010[1]));
    jump = (sqrtf(SQ(own.eye.x - sLastOwn.eye.x) + SQ(own.eye.y - sLastOwn.eye.y) + SQ(own.eye.z - sLastOwn.eye.z)) >
            CUT_DISTANCE) ||
           (sqrtf(SQ(own.at.x - sLastOwn.at.x) + SQ(own.at.y - sLastOwn.at.y) + SQ(own.at.z - sLastOwn.at.z)) >
            CUT_DISTANCE);
    // The battle's own camera mirrored or not, decided as it cuts to another shot, and whenever
    // it isn't what's drawn, so that it's on the side the camera is on as it's handed back to;
    // it goes across the line as it likes in between
    if (!sOwnDrawn || jump || (key != sLastKey)) {
        sOwnMirror = Randomizer_BattleCameraOtherSide(&own);
    }
    sLastOwn = own; // as the battle has it
    if (sOwnMirror) {
        Randomizer_BattleCameraMirror(&own);
    }
    if (key != sLastKey) {
        sLastKey = key;
        sKeyAge = 0;
    } else if (sKeyAge < 0x7FFF) {
        sKeyAge++;
    }
    switching = Randomizer_BattleCameraSwitch(&own, &switchView);

    moving = Randomizer_BattleCameraMove(&own, &view);
    if (moving && sCircle) {
        Randomizer_BattleCameraNoCircle(camera);
    }
    filming = Randomizer_BattleCameraFilming(camera);
    clip = Randomizer_BattleCameraClips(&own, !moving && (sHandover == 0) && (sKeyAge >= CUT_AFTER_CHANGE) && jump);
    if (filming) {
        // An attack filmed takes over from the move, the camera going to its first shot from
        // where it was
        if (!sWasFilming) {
            sHandover = sHandFrames = HANDOVER_FRAMES;
            sHandFrom = sDrawnView;
        }
        sMoving = MOVE_NONE;
        Randomizer_BattleCameraShoot(&view);
        if (sHitLanded && !sClipHandover) {
            sHandover = 0; // the hit's shots cut or swing to the defender themselves
        }
        if (sHandover > 0) {
            sHandover--;
            Randomizer_BattleCameraSwing(&view, &sHandFrom, &view,
                                         Randomizer_ShotEase(1.0f - ((f32)sHandover / sHandFrames)));
        }
    } else if (!moving && sIdleShot) {
        if (sShot.kind != SHOT_SPLIT) {
            Randomizer_BattleCameraShoot(&view);
        } else {
            view = own; // drawn only if the split screen ends this frame (below)
        }
    } else if (!moving && (switching == SWITCH_VIEW)) {
        view = switchView;
        sHandover = 0;
    } else if (!moving) {
        // Into the battle's camera after a filmed attack, and where it cuts soon after going on
        // to something else, by a quick move (again from where it is, if it cuts again), and
        // after a switch, by a longer one
        if (sWasFilming || ((sKeyAge < CUT_AFTER_CHANGE) && jump) || (switching == SWITCH_ENDED)) {
            sHandover = sHandFrames = (switching == SWITCH_ENDED) ? SEND_OUT_FRAMES : HANDOVER_FRAMES;
            sHandFrom = sDrawnView;
        }
        if (sHandover > 0) {
            sHandover--;
            Randomizer_BattleCameraSwing(&view, &sHandFrom, &own,
                                         Randomizer_ShotEase(1.0f - ((f32)sHandover / sHandFrames)));
        } else {
            view = own;
            ownDrawn = TRUE;
        }
    } else {
        sHandover = 0;
    }
    // In the menus, where the battle's camera cuts to another shot, sometimes one of the
    // title's instead, for as long as the title's last
    if ((director == NULL) || (director->unk_1C != CAMERA_MODE_MENUS) || (director->unk_30 != 0) || moving ||
        filming || clip || (sIdleShot && (sShot.frame >= IDLE_FRAMES))) {
        sIdleShot = FALSE;
    } else if (!sIdleShot && (sHandover == 0) && (sKeyAge >= CUT_AFTER_CHANGE) && jump &&
               (Randomizer_BattleCameraBelow(IDLE_CHANCE) == 0
#ifdef BATTLE_CAMERA_TEST_IDLE
                || TRUE
#endif
                )) {
        sIdleShot = TRUE;
        ownDrawn = FALSE;
        Randomizer_BattleCameraIdleCut();
        if (sShot.kind != SHOT_SPLIT) {
            Randomizer_BattleCameraShoot(&view);
        }
    }
    sWasFilming = filming;
    if (sIdleShot && (sShot.kind == SHOT_SPLIT)) {
        sOwnDrawn = FALSE;
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
    sOwnDrawn = ownDrawn && !clip;
    if (sHandover == 0) {
        sClipHandover = FALSE;
    }
    // This file's shot going on under a clip follows it across the line, so that it carries on
    // from the side the clip ends on (decided again next frame, from its view this frame)
    if (clip && (filming || sIdleShot) && ((sLastView.eye.z < FIELD_LINE_Z) != (sDrawnView.eye.z < FIELD_LINE_Z))) {
        sShotMirror = !sShotMirror;
    }
    if (clip) {
        Randomizer_ClipView(&view);
        if (sClipMirror) {
            Randomizer_BattleCameraMirror(&view);
        }
        // Its blend in: from the view drawn as it started (a cut if 0)
        if (sClipFrame < sClip->blendIn) {
            Randomizer_BattleCameraSwing(&view, &sClipFrom, &view,
                                         Randomizer_ShotEase((f32)sClipFrame / sClip->blendIn));
        }
        sClipFrame += CLIP_TICKS;
    }
    // Wherever it came from, moves included: not on the line between them (unless a clip lets it)
    if (!clip || !(sClip->flags & CLIP_FLAG_FREE)) {
        Randomizer_BattleCameraOffLine(&view);
    }
    sDrawnView = view;
#ifdef BATTLE_CAMERA_TEST_LOG
    // The view drawn and both Pokemon as the camera has them (middle, height), the clip's
    // Pokemon and the attacker's side, at 0x8000036C on, for comparing with the editor
    {
        f32* log = (f32*)(CLIP_TABLE_ADDRESS + 0x10);
        s32 i;

        log[0] = view.eye.x, log[1] = view.eye.y, log[2] = view.eye.z;
        log[3] = view.at.x, log[4] = view.at.y, log[5] = view.at.z;
        for (i = 0; i < SIDES; i++) {
            log[6 + i * 4] = sSides[i].middle.x, log[7 + i * 4] = sSides[i].middle.y;
            log[8 + i * 4] = sSides[i].middle.z, log[9 + i * 4] = sSides[i].height;
        }
        ((u8*)&log[14])[0] = sClipSide;
        ((u8*)&log[14])[1] = (D_84390204 != NULL) ? Randomizer_BattleCameraSideOf(D_84390204) : 0xFF;
        ((s16*)&log[14])[1] = view.roll;
        log[15] = view.fovy;
    }
#endif

    sBattleCamera.eye = camera->unk_60.eye;
    sBattleCamera.at = camera->unk_60.at;
    sBattleCamera.up = camera->unk_60.up;
    sBattleCamera.fovy = camera->unk_24.fovy;
    camera->unk_60.eye = view.eye;
    camera->unk_60.at = view.at;
    Randomizer_ShotUp(&view, &camera->unk_60.up);
    camera->unk_24.fovy = view.fovy;
    sShowing = filming || moving || sIdleShot || clip || (sHandover > 0);
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
    sHandFrames = HANDOVER_FRAMES;
    sCircle = FALSE;
    sSwitchStep = -1;
    sIdleShot = FALSE;
    sSplitHalf = -1;
    sWasFilming = FALSE;
    sLastKey = -1;
    sKeyAge = 0;
    sOwnDrawn = FALSE;
    sOwnMirror = sShotMirror = sMoveMirror = sClipMirror = sClipHandover = FALSE;
    sDrawnView.eye.z = FIELD_LINE_Z + 1.0f; // the battle opens on z's + side
    sSpecies[0] = sSpecies[1] = -1;
    sIdleAnim[0] = sIdleAnim[1] = -1;
    sRandom = osGetCount();
    sCustom = !gRandomizerState.originalCamera;
    gRandomizerState.battleCameraHook = Randomizer_BattleCamera;

    // The camera clips, from the ROM, none if they aren't there; where they are for the live
    // link (randomizer_camera_clips.h)
    sClip = NULL;
    sMomentKey = -1;
    sClipAttacker = NULL;
    sFainted[0] = sFainted[1] = FALSE;
    func_80003B30((u32)sClipBuffer, (u32)randomizer_clips_ROM_START, (u32)randomizer_clips_ROM_END, 0);
    if ((((u32*)sClipBuffer)[0] != 0x43414D43) || (((u32*)sClipBuffer)[1] != 0x4C495031) || // "CAMCLIP1"
        (sClips.version != CLIP_VERSION) || (sClips.clipSize != sizeof(RandomizerClip))) {
        sClips.numClips = 0;
    }
    *(u32*)CLIP_TABLE_ADDRESS = (u32)sClipBuffer;
}

#endif
