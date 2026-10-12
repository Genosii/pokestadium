#ifndef _FRAGMENT62_RANDOMIZER_CAMERA_CLIPS_H_
#define _FRAGMENT62_RANDOMIZER_CAMERA_CLIPS_H_

#include "global.h"

/*
 * The camera clips (docs/camera-editor.md): camera moves made of keyframes, each for one
 * moment of a battle, which the battle camera (randomizer_battle_camera.c) plays in place of
 * its own shots. The table is in a block of its own in the ROM (randomizer_block.ld), a fixed
 * CLIP_TABLE_SIZE so that writing clips into a ROM never moves anything, and starts with
 * CLIP_MAGIC so that the editor finds it in a ROM and the live link in RAM; the battle copies
 * it into RAM as it starts, and puts where in CLIP_TABLE_ADDRESS.
 *
 * Everything is big-endian, as the N64 reads it, with no pointers. Angles are in tenths of a
 * degree, so that a clip can go round more than once between two keyframes; distances and
 * offsets in hundredths of the anchor's height.
 *
 * Built only with RANDOMIZER=1.
 */

#define CLIP_MAGIC "CAMCLIP1"
#define CLIP_VERSION 1
#define CLIP_TABLE_SIZE 0x2000
#define CLIP_MAX_KEYS 8
#define CLIP_MAX 30
// Low RAM the game doesn't use, after osAppNMIBuffer (0x8000031C, 64 bytes) and before its code
// (0x80000400): the clip table's address in RAM while a battle is on, 0 otherwise
#define CLIP_TABLE_ADDRESS 0x8000035C

/*
 * The battle's moments, each starting as the battle director (docs/game-engine.md, "The
 * battle's camera") gets to it. A clip lasts as long as its last keyframe, or until the next
 * moment starts; where the battle waits on the camera (the recall, before the Poke Ball is
 * thrown), it waits for the clip.
 */
typedef enum RandomizerClipMoment {
    CLIP_MOMENT_NONE,
    CLIP_MOMENT_MENUS,     // between turns, where the battle's camera cuts to another shot
    CLIP_MOMENT_OPENING,   // the turn's opening (script 1)
    CLIP_MOMENT_WINDUP,    // an attack: the camera going to the attacker (script 2 or 3, step 0),
    CLIP_MOMENT_ANIMATION, // its animation playing (step 1),
    CLIP_MOMENT_HIT,       // the hit landing, the defender reacting
    CLIP_MOMENT_REPLAY,    // a critical hit's or a Hyper Beam's hit landing again, each time
    CLIP_MOMENT_STATUS,    // a status move (no power), the whole of it
    CLIP_MOMENT_RECALL,    // switching: the one called back turning into a light that rises
    CLIP_MOMENT_SEND_OUT,  // the Poke Ball thrown and the new one coming out (after a faint too)
    CLIP_MOMENT_FAINT,     // a Pokemon fainting
    CLIP_MOMENT_TURN_END,  // the turn's end (script 4)
    CLIP_MOMENT_COUNT
} RandomizerClipMoment;

typedef enum RandomizerClipMode {
    CLIP_MODE_OFF,    // an empty slot
    CLIP_MODE_ALWAYS, // played every time, in place of the automatic shots
    CLIP_MODE_POOL,   // one of the random shots, weight against CLIP_AUTO_WEIGHT for the automatic ones
} RandomizerClipMode;
#define CLIP_AUTO_WEIGHT 100

// Which moves a clip is for (attack moments)
typedef enum RandomizerClipCategory {
    CLIP_CATEGORY_ANY,
    CLIP_CATEGORY_PHYSICAL, // the types below Fire (Gen 1's split)
    CLIP_CATEGORY_SPECIAL,
    CLIP_CATEGORY_STATUS, // no power
} RandomizerClipCategory;
#define CLIP_TYPE_ANY 0xFF // or one of Gen 1's type ids (docs/game-data.md, "Moves")
typedef enum RandomizerClipSide {
    CLIP_SIDE_ANY,
    CLIP_SIDE_PLAYER, // the moment's Pokemon (the attacker, the one switching or fainting) the player's,
    CLIP_SIDE_OTHER,  // or the other's
} RandomizerClipSide;

#define CLIP_FLAG_FREE 1 // the camera may cross the line between the two and stand on it

// What a keyframe's camera is placed round, and what it looks at
typedef enum RandomizerClipAnchor {
    CLIP_ANCHOR_ATTACKER,
    CLIP_ANCHOR_DEFENDER,
    CLIP_ANCHOR_SUBJECT, // the moment's own: the one switching or fainting, the attacker otherwise
    CLIP_ANCHOR_MIDDLE,  // halfway between the two
    CLIP_ANCHOR_CENTRE,  // the middle of the field, on the ground
    CLIP_ANCHOR_COUNT
} RandomizerClipAnchor;

// The point on an anchor a camera looks at
typedef enum RandomizerClipPoint {
    CLIP_POINT_FEET,
    CLIP_POINT_MIDDLE,
    CLIP_POINT_HEAD, // the middle, for the middle of the field and between the two
} RandomizerClipPoint;

// From a keyframe to the next
typedef enum RandomizerClipEasing {
    CLIP_EASE_LINEAR,
    CLIP_EASE_IN,
    CLIP_EASE_OUT,
    CLIP_EASE_IN_OUT,
    CLIP_EASE_HOLD,   // the keyframe's view until the next
    CLIP_EASE_BEZIER, // the keyframe's own curve (bezier), as in After Effects' graph editor
} RandomizerClipEasing;

/*
 * A keyframe: the camera yaw round its anchor (0 in front of it, the way it faces; turning
 * toward the camera's side of the field, z above 0, for either side's Pokemon, so that a clip
 * films the other side as in a mirror), pitch above it, distance away; looking at a point on
 * an anchor, offset along the anchor's facing (x), up (y) and toward the camera's side (z).
 */
typedef struct RandomizerClipKey {
    /* 0x00 */ s16 frame; // from the clip's start, 60 a second
    /* 0x02 */ u8 eyeAnchor;
    /* 0x03 */ u8 lookAnchor;
    /* 0x04 */ s16 yaw;      // tenths of a degree
    /* 0x06 */ s16 pitch;    // tenths of a degree
    /* 0x08 */ u16 distance; // hundredths of the anchor's height
    /* 0x0A */ u8 lookPoint;
    /* 0x0B */ u8 easing;
    /* 0x0C */ s16 lookOffset[3]; // hundredths of the look anchor's height
    /* 0x12 */ s16 roll;          // tenths of a degree, clockwise
    /* 0x14 */ u16 fovy;          // field of view, hundredths of a degree (the battle's is 30)
    /* 0x16 */ u8 pad16[2];
    /* 0x18 */ s16 bezier[4]; // x1, y1, x2, y2, thousandths, as CSS's cubic-bezier()
} RandomizerClipKey; // size = 0x20

typedef struct RandomizerClip {
    /* 0x00 */ u8 moment;
    /* 0x01 */ u8 mode;
    /* 0x02 */ u8 weight; // 1 to 100, in the pool
    /* 0x03 */ u8 flags;
    /* 0x04 */ u8 category;
    /* 0x05 */ u8 type;
    /* 0x06 */ u8 side;
    /* 0x07 */ u8 numKeys;
    /* 0x08 */ RandomizerClipKey keys[CLIP_MAX_KEYS];
} RandomizerClip; // size = 0x108

typedef struct RandomizerClipTable {
    /* 0x00 */ char magic[8];
    /* 0x08 */ u16 version;
    /* 0x0A */ u16 numClips;
    /* 0x0C */ u16 clipSize;  // sizeof(RandomizerClip), for the editor
    /* 0x0E */ u8 forceMoment; // the test switch: set by the live link (a RandomizerClipMoment, 0 none)
    /* 0x0F */ u8 pad0F;
    /* 0x10 */ RandomizerClip clips[CLIP_MAX];
    /* 0x1F00 */ u8 pad1F00[CLIP_TABLE_SIZE - 0x1F00];
} RandomizerClipTable; // size = CLIP_TABLE_SIZE

#ifdef __GNUC__
// Checked by the build's GCC syntax pass: the editor and the live link read this layout
typedef char RandomizerClipLayoutCheck[((sizeof(RandomizerClipKey) == 0x20) && (sizeof(RandomizerClip) == 0x108) &&
                                        (sizeof(RandomizerClipTable) == CLIP_TABLE_SIZE))
                                           ? 1
                                           : -1];
#endif

// The defaults (randomizer_camera_clips.c), in the ROM, not loaded with any fragment
extern RandomizerClipTable gRandomizerClipDefaults;
extern u8 randomizer_clips_ROM_START[];
extern u8 randomizer_clips_ROM_END[];

#endif // _FRAGMENT62_RANDOMIZER_CAMERA_CLIPS_H_
