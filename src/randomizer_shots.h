#ifndef _RANDOMIZER_SHOTS_H_
#define _RANDOMIZER_SHOTS_H_

/*
 * The camera shots the randomizer films attacks with, after Pokemon Battle Revolution's (its
 * first trailer, 2006): the title screen's demo battles (src/fragments/36/
 * randomizer_title_arena.c) and real battles (src/fragments/62/randomizer_battle_camera.c)
 * both include this, so that the two film them the same way. Its functions are static: each
 * of the two fragments has its own copy.
 *
 * The includer defines, before including it:
 *  - SHOT_LOOK_RAISE: how far above what it films the camera looks, of its distance, which
 *    puts that a little under the middle of the screen (the title, under its logo);
 *  - SHOT_BELOW(n): a random number from 0 to n - 1, from its own generator.
 *
 * Built only with RANDOMIZER=1.
 */

#include "src/12D80.h"
#include "src/F420.h"

#define CAMERA_FOVY 50.0f
#define TAN_HALF_FOVY 0.4663f // tan(CAMERA_FOVY / 2)
#define RAD_TO_FOVY(half) ((half) * (360.0f / 3.14159265f)) // half the field of view, in degrees

#define IDLE_FRAMES 75 // the longest a shot between turns lasts (30 frames a second)
#define MOVE_FRAMES 60 // the shots that push in or come down do it over this long
#define GROUND_EYE 22.0f // the camera's height on the ground: lower, looking up, it clips through the field
#define LOW_ROLL 3        // the low shots tilt this much less than the others

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
#define AIM_RAISE 0.06f     // the other a little under the middle,
#define AIM_LATERAL 0.12f   // and right of it, at about (191, 136)
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
#define POINT_TAIL 8
#define POINT_TOP 11

/*
 * The shots. The title has them all; a real battle the attacker's and the defender's, since
 * between turns its own camera films the field.
 *  - Between turns, a shot of the field or of one of them.
 *  - As a turn's attack starts, the attacker.
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

// Where the camera is and what it looks at, how far it's tilted and how wide it sees
typedef struct RandomizerShotView {
    /* 0x00 */ Vec3f eye;
    /* 0x0C */ Vec3f at;
    /* 0x18 */ s16 roll;
    /* 0x1C */ f32 fovy; // in degrees
} RandomizerShotView; // size = 0x20

// A Pokemon, as the shots film it
typedef struct RandomizerShotSide {
    /* 0x00 */ unk_D_86002F58_004_000* model;
    /* 0x04 */ Vec3f middle;
    /* 0x10 */ f32 height;
    /* 0x14 */ f32 width;
} RandomizerShotSide; // size = 0x18

// A shot, set up at random as it starts
typedef struct RandomizerShot {
    /* 0x00 */ u8 kind;
    /* 0x01 */ u8 side;     // the one it's on (the title's)
    /* 0x02 */ s16 frame;
    /* 0x04 */ s16 yaw;     // where a shot that goes round starts
    /* 0x06 */ s16 pitch;   // how far above what it looks at a shot is
    /* 0x08 */ s16 slant;   // how far to the side of a Pokemon's front a shot is
    /* 0x0A */ s16 turn;    // which way it goes round, or to which side it's tilted: 1 or -1
    /* 0x0C */ s16 roll;    // how far it's tilted
    /* 0x0E */ s16 focusYaw; // the side of the Pokemon the head is on
    /* 0x10 */ Vec3f focus;  // where a shot that follows a head looks, catching up with it
    /* 0x1C */ u8 focusSet;
} RandomizerShot; // size = 0x20

// 0 to 1 as t goes from 0 to 1, easing in and out, and 1 after
static f32 Randomizer_ShotEase(f32 t) {
    if (t > 1.0f) {
        t = 1.0f;
    }
    return t * t * (3.0f - (2.0f * t));
}

// How far from a Pokemon the camera is to show it framing times its height
static f32 Randomizer_ShotDistance(RandomizerShotSide* side, f32 framing) {
    f32 distance = side->height * framing;

    return (distance < MIN_DISTANCE) ? MIN_DISTANCE : distance;
}

// The length of a vector, which is made a unit long (if it isn't 0)
static f32 Randomizer_ShotNormalize(Vec3f* v) {
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
static void Randomizer_ShotAxes(Vec3f* forward, Vec3f* right, Vec3f* up) {
    func_8000E88C(right, -forward->z, 0.0f, forward->x);
    if (Randomizer_ShotNormalize(right) < 0.001f) {
        func_8000E88C(right, 1.0f, 0.0f, 0.0f); // straight up or down
    }
    func_8000E88C(up, (right->y * forward->z) - (right->z * forward->y),
                  (right->z * forward->x) - (right->x * forward->z), (right->x * forward->y) - (right->y * forward->x));
}

/*
 * The camera at eye framing a point: looking at a point above it, by raise of its distance
 * (SHOT_LOOK_RAISE puts it a little under the middle of the screen from any height), and to
 * its left, by lateral of its distance, which puts it right of the middle; tilted by roll
 */
static void Randomizer_ShotAimAt(RandomizerShotView* view, Vec3f* eye, Vec3f* point, f32 raise, f32 lateral,
                                 s16 roll) {
    Vec3f forward;
    Vec3f right;
    Vec3f up;
    f32 distance;

    func_8000E88C(&forward, point->x - eye->x, point->y - eye->y, point->z - eye->z);
    distance = Randomizer_ShotNormalize(&forward);
    Randomizer_ShotAxes(&forward, &right, &up);
    view->eye = *eye;
    func_8000E88C(&view->at, point->x + (((up.x * raise) - (right.x * lateral)) * distance),
                  point->y + (((up.y * raise) - (right.y * lateral)) * distance),
                  point->z + (((up.z * raise) - (right.z * lateral)) * distance));
    view->roll = roll;
    view->fovy = CAMERA_FOVY;
}

// The same, with the point a little under the middle of the screen
static void Randomizer_ShotAim(RandomizerShotView* view, Vec3f* eye, Vec3f* point, s16 roll) {
    Randomizer_ShotAimAt(view, eye, point, SHOT_LOOK_RAISE, 0.0f, roll);
}

// The camera distance away from a point, pitch above it, at yaw round it, framing it
static void Randomizer_ShotAimFrom(RandomizerShotView* view, Vec3f* point, f32 distance, s16 pitch, s16 yaw,
                                   s16 roll) {
    Vec3f eye;

    func_80010354(point, &eye, distance, pitch, yaw);
    Randomizer_ShotAim(view, &eye, point, roll);
}

// The camera on a level with height, distance away from a point across the ground, at yaw
static void Randomizer_ShotAimLevel(RandomizerShotView* view, Vec3f* point, f32 distance, f32 height, s16 yaw,
                                    s16 roll) {
    Vec3f eye;

    func_8000E88C(&eye, point->x + (distance * SINS(yaw)), height, point->z + (distance * COSS(yaw)));
    Randomizer_ShotAim(view, &eye, point, roll);
}

// The camera's up for a view: the screen's, tilted by its roll
static void Randomizer_ShotUp(RandomizerShotView* view, Vec3f* out) {
    Vec3f forward;
    Vec3f right;
    Vec3f up;

    func_8000E88C(&forward, view->at.x - view->eye.x, view->at.y - view->eye.y, view->at.z - view->eye.z);
    Randomizer_ShotNormalize(&forward);
    Randomizer_ShotAxes(&forward, &right, &up);
    func_8000E88C(out, (up.x * COSS(view->roll)) + (right.x * SINS(view->roll)),
                  (up.y * COSS(view->roll)) + (right.y * SINS(view->roll)),
                  (up.z * COSS(view->roll)) + (right.z * SINS(view->roll)));
}

/*
 * Where a side's head is (see POINT_HEAD), from the frame before: its point 7, or halfway up
 * to its point 11 when that's above it, as the top of the head is; its middle if it has none
 */
static void Randomizer_ShotHead(RandomizerShotSide* side, Vec3f* out) {
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
 * (see POINT_HEAD), but its tail, followed smoothly, from where it is the first time
 * (tracked FALSE); until it's drawn, where it was. Some fly their idle animation far above
 * where the battle puts them: Pidgeotto's body is 80 to 120 units up, its card's middle 27.
 * A long tail would pull the middle off the body: Raichu's is 50 units to one side.
 */
static void Randomizer_ShotTrack(RandomizerShotSide* side, u8* tracked) {
    unk_D_86002F58_004_000* model = side->model;
    Vec3f lo;
    Vec3f hi;
    s32 found = FALSE;
    s32 i;

    for (i = 0; i < model->unk_0A7; i++) {
        Vec3f* point = &model->unk_0A8[i].unk_04;

        if (model->unk_0A8[i].unk_00 == POINT_TAIL) {
            continue;
        }
        if (!found) {
            lo = hi = *point;
            found = TRUE;
        }
        lo.x = MIN(lo.x, point->x);
        lo.y = MIN(lo.y, point->y);
        lo.z = MIN(lo.z, point->z);
        hi.x = MAX(hi.x, point->x);
        hi.y = MAX(hi.y, point->y);
        hi.z = MAX(hi.z, point->z);
    }
    if (!found) {
        return;
    }
    lo.x = (lo.x + hi.x) * 0.5f;
    lo.y = (lo.y + hi.y) * 0.5f;
    lo.z = (lo.z + hi.z) * 0.5f;
    if (!*tracked) {
        side->middle = lo;
        *tracked = TRUE;
    }
    side->middle.x += (lo.x - side->middle.x) * TRACK_FOLLOW;
    side->middle.y += (lo.y - side->middle.y) * TRACK_FOLLOW;
    side->middle.z += (lo.z - side->middle.z) * TRACK_FOLLOW;
}

// Follows a side's head (the shot's focus): from where it is the first time, then catching up
// with it, so that the camera doesn't shake
static void Randomizer_ShotFollow(RandomizerShot* shot, RandomizerShotSide* side) {
    Vec3f point;

    Randomizer_ShotHead(side, &point);
    if (!shot->focusSet) {
        shot->focus = point;
        shot->focusSet = TRUE;
    }
    shot->focus.x += (point.x - shot->focus.x) * FACE_FOLLOW;
    shot->focus.y += (point.y - shot->focus.y) * FACE_FOLLOW;
    shot->focus.z += (point.z - shot->focus.z) * FACE_FOLLOW;
}

// A new shot of a side's Pokemon (or both), set up at random
static void Randomizer_ShotCut(RandomizerShot* shot, s32 kind, s32 side) {
    shot->kind = kind;
    shot->side = side;
    shot->frame = 0;
    shot->yaw = SHOT_BELOW(0x10000);
    shot->slant = 0x0C00 + SHOT_BELOW(0x1800); // 17 to 50 degrees
    shot->turn = SHOT_BELOW(2) ? 1 : -1;
    // Half of them tilted, up to 10 degrees
    shot->roll = SHOT_BELOW(2) ? (0x200 + SHOT_BELOW(0x500)) * shot->turn : 0;
    // For the head: from below, level or above (-11, 6 or 22 degrees)
    shot->pitch = ((SHOT_BELOW(3) - 1) * 0x0C00) + 0x0400;
    shot->focusSet = FALSE;
}

/*
 * A shot of the attacker's or the defender's, of kind (shot's, or the one a swing of the
 * camera goes to) on one of its frames: side the one it's on and other the other one; left
 * the one on the left, the player's in a battle, and right the other, for the Game Boy view.
 * FALSE for a shot of the field, which this doesn't do.
 */
static s32 Randomizer_ShotCompose(RandomizerShotView* view, s32 kind, RandomizerShot* shot, s32 frame,
                                  RandomizerShotSide* side, RandomizerShotSide* other, RandomizerShotSide* left,
                                  RandomizerShotSide* right) {
    // The way it faces along x: the one on the left faces right, even past the other as an
    // attack carries it across the field
    f32 dir = (side == left) ? 1.0f : -1.0f;
    s16 facing = (dir > 0.0f) ? 0x4000 : -0x4000;              // the yaw from which the camera sees its face
    f32 idle = Randomizer_ShotEase((f32)frame / IDLE_FRAMES);
    f32 move = Randomizer_ShotEase((f32)frame / MOVE_FRAMES);
    f32 distance;
    f32 start;
    s16 yaw;
    Vec3f point;
    Vec3f eye;

    switch (kind) {
        case SHOT_LOW:
            // From the ground in front of it, to one side, looking up, pushing in and going round
            Randomizer_ShotAimLevel(view, &side->middle, Randomizer_ShotDistance(side, 2.3f) * (1.15f - (0.25f * move)),
                                    GROUND_EYE, facing + (shot->slant * shot->turn) + (frame * 0x10 * shot->turn),
                                    shot->roll / LOW_ROLL);
            break;

        case SHOT_PUSH:
            // In front of it, about its middle's height, pushing in fast
            Randomizer_ShotAimLevel(view, &side->middle, Randomizer_ShotDistance(side, 2.9f - (0.8f * move)),
                                    side->middle.y + (side->height * 0.1f),
                                    facing + ((shot->slant / 2) * shot->turn), shot->roll / 2);
            break;

        case SHOT_SHOULDER:
            // From behind it, over its shoulder, at the other, moving in a little
            eye.x = side->middle.x - (dir * (Randomizer_ShotDistance(side, 1.2f) + 40.0f)) +
                    (dir * move * (other->middle.x - side->middle.x) * dir * 0.15f);
            eye.y = side->middle.y + (side->height * 0.6f) + 10.0f;
            eye.z = MAX(side->width * 1.6f, side->height * 0.7f) * shot->turn; // clear of long ones' tails
            Randomizer_ShotAim(view, &eye, &other->middle, shot->roll);
            break;

        case SHOT_GAZE:
        case SHOT_FACE:
            // Close on its head, from the side it's on (in front, or behind for a Snorlax lying
            // on its back), a little to one side, from below, level or above, the camera
            // turning after the head as it moves (smoothly, so it doesn't shake), and coming
            // slowly closer
            if (!shot->focusSet) {
                Randomizer_ShotHead(side, &point);
                shot->focusYaw = facing;
                if ((SQ(point.x - side->middle.x) + SQ(point.z - side->middle.z)) > SQ(side->height * 0.2f)) {
                    func_800102A4(&side->middle, &point, &start, &yaw, &shot->focusYaw);
                }
            }
            Randomizer_ShotFollow(shot, side);
            distance = MAX(side->height * FACE_FRAMING, FACE_MIN) * (1.1f - (0.15f * idle));
            func_80010354(&shot->focus, &eye, distance, shot->pitch, shot->focusYaw + (shot->slant * shot->turn));
            if (eye.y < GROUND_EYE) {
                eye.y = GROUND_EYE;
            }
            Randomizer_ShotAim(view, &eye, &shot->focus, shot->roll / 2);
            break;

        case SHOT_TRACK:
            // Beside it, a little in front, the camera moving with it (its middle, followed
            // smoothly), so that it stays where it is on the screen as it lunges or jumps, as
            // Colosseum and XD film attacks
            distance = Randomizer_ShotDistance(side, ACTION_FRAMING);
            func_8000E88C(&eye, side->middle.x + (dir * distance * 0.35f), side->middle.y + (distance * 0.15f),
                          side->middle.z + (distance * 0.9f * shot->turn));
            point = side->middle;
            point.x += dir * side->height * 0.3f; // room in front of it
            Randomizer_ShotAim(view, &eye, &point, shot->roll / 2);
            break;

        case SHOT_GAMEBOY:
        case SHOT_RBY:
            // The Game Boy games' view of a battle (Red, Blue and Yellow): behind the one on the
            // left (the player's in the battle) and to its right, its back at the bottom left,
            // the other far off at the top right; then, quickly, zooming in on the other, which
            // comes to a little under the middle, easing in and out. Zoomed in, the field of
            // view is narrower (start is the tangent of half of it), and so are the angles that
            // put the other where it is on the screen
            side = left;
            other = right;
            func_8000E88C(&eye, side->middle.x - (((side->height * 0.6f) + 40.0f) * RBY_FAR),
                          side->middle.y + (((side->height * 0.6f) + 10.0f) * RBY_FAR),
                          ((side->height * 0.8f) + 30.0f) * RBY_FAR);
            distance = sqrtf(SQ(other->middle.x - eye.x) + SQ(other->middle.y - eye.y) + SQ(other->middle.z - eye.z));
            start = other->height * RBY_ZOOM_FRAMING / distance;
            start = CLAMP(start, RBY_ZOOM_MIN, TAN_HALF_FOVY);
            move = Randomizer_ShotEase((f32)MAX(frame - RBY_HOLD, 0) / RBY_ZOOM_FRAMES);
            start = TAN_HALF_FOVY + ((start - TAN_HALF_FOVY) * move);
            Randomizer_ShotAimAt(view, &eye, &other->middle,
                                 (RBY_RAISE + ((SHOT_LOOK_RAISE - RBY_RAISE) * move)) * start / TAN_HALF_FOVY,
                                 RBY_LATERAL * (1.0f - move) * start / TAN_HALF_FOVY, 0);
            view->fovy = RAD_TO_FOVY(start / (1.0f + (0.28f * start * start))); // atan, closely enough
            break;

        case SHOT_SIGHTS:
        case SHOT_AIM:
            // As a game played over the shoulder aims: close behind its head, a little above it
            // and to its right, its back big at the left of the screen, at the other, a little
            // right of the middle; following the head and pushing in a little
            Randomizer_ShotFollow(shot, side);
            distance = ((side->height * AIM_BACK) + AIM_BACK_MIN) * (1.0f - (0.15f * idle));
            func_8000E88C(&eye, shot->focus.x - (dir * distance), shot->focus.y + (side->height * AIM_ABOVE),
                          shot->focus.z + (dir * distance * AIM_BESIDE));
            Randomizer_ShotAimAt(view, &eye, &other->middle, AIM_RAISE, AIM_LATERAL, 0);
            break;

        case SHOT_HIT:
            // Close in front of it to one side, tilted, pulling back
            Randomizer_ShotAimLevel(view, &side->middle,
                                    Randomizer_ShotDistance(side, 2.4f) *
                                        (0.95f + (0.2f * Randomizer_ShotEase(frame / 45.0f))),
                                    side->middle.y, facing + (0x1400 * shot->turn), shot->roll);
            break;

        case SHOT_HIT_LOW:
            // From the ground, farther and more to the side, tilted
            Randomizer_ShotAimLevel(view, &side->middle,
                                    Randomizer_ShotDistance(side, 3.4f) *
                                        (1.0f + (0.1f * Randomizer_ShotEase(frame / 45.0f))),
                                    GROUND_EYE, facing + (0x2000 * shot->turn), shot->roll / LOW_ROLL);
            break;

        default:
            return FALSE;
    }
    return TRUE;
}

#endif
