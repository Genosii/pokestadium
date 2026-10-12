/*
 * The camera clips the ROM comes with (randomizer_camera_clips.h): none, so that the battle is
 * filmed as before until clips are made in the editor (docs/camera-editor.md) and their export
 * pasted here. Linked into the ROM on its own (randomizer_block.ld), not with a fragment: the
 * battle camera copies it into RAM as a battle starts.
 *
 * BATTLE_CAMERA_SAMPLE_CLIPS (a test build's) puts in four, to see clips played before there's
 * an editor: the hit, the send-out, an attack's wind-up and a faint, each always played.
 *
 * Built only with RANDOMIZER=1.
 */
#include "randomizer_camera_clips.h"

#ifdef RANDOMIZER

// #define BATTLE_CAMERA_SAMPLE_CLIPS

#ifdef BATTLE_CAMERA_SAMPLE_CLIPS
#define SAMPLE_CLIPS 4
#else
#define SAMPLE_CLIPS 0
#endif

// A keyframe: frame, the camera's anchor, yaw, pitch and distance (heights x 100), what it looks
// at, its field of view and roll, and the easing on to the next
#define KEY(frame, eyeAnchor, yaw, pitch, distance, lookAnchor, lookPoint, fovy, roll, easing) \
    { (frame), (eyeAnchor), (lookAnchor), (yaw), (pitch), (distance), (lookPoint), (easing), { 0, 0, 0 }, (roll), \
      (fovy), { 0, 0 }, { 0, 0, 0, 0 } }

RandomizerClipTable gRandomizerClipDefaults = {
    CLIP_MAGIC,
    CLIP_VERSION,
    SAMPLE_CLIPS,
    sizeof(RandomizerClip),
    0,
    0,
    {
#ifdef BATTLE_CAMERA_SAMPLE_CLIPS
        // The hit: round the defender from in front and to the side to square on, rising a
        // little and pulling back, easing in and out
        { CLIP_MOMENT_HIT, CLIP_MODE_ALWAYS, 100, 0, CLIP_CATEGORY_ANY, CLIP_TYPE_ANY, CLIP_SIDE_ANY, 2,
          {
              KEY(0, CLIP_ANCHOR_DEFENDER, 250, 30, 240, CLIP_ANCHOR_DEFENDER, CLIP_POINT_MIDDLE, 3000, 0,
                  CLIP_EASE_IN_OUT),
              KEY(70, CLIP_ANCHOR_DEFENDER, 900, 120, 300, CLIP_ANCHOR_DEFENDER, CLIP_POINT_MIDDLE, 3000, 0,
                  CLIP_EASE_HOLD),
          } },
        // The send-out: low and far, pushing in on the new one and coming up to its head
        { CLIP_MOMENT_SEND_OUT, CLIP_MODE_ALWAYS, 100, 0, CLIP_CATEGORY_ANY, CLIP_TYPE_ANY, CLIP_SIDE_ANY, 2,
          {
              KEY(0, CLIP_ANCHOR_SUBJECT, 200, -40, 420, CLIP_ANCHOR_SUBJECT, CLIP_POINT_MIDDLE, 3000, 0,
                  CLIP_EASE_OUT),
              KEY(100, CLIP_ANCHOR_SUBJECT, 380, 60, 230, CLIP_ANCHOR_SUBJECT, CLIP_POINT_HEAD, 3000, 0,
                  CLIP_EASE_HOLD),
          } },
        // An attack's wind-up: behind the attacker and to the side, over its shoulder at the
        // defender, pushing slowly past it
        { CLIP_MOMENT_WINDUP, CLIP_MODE_ALWAYS, 100, 0, CLIP_CATEGORY_ANY, CLIP_TYPE_ANY, CLIP_SIDE_ANY, 2,
          {
              KEY(0, CLIP_ANCHOR_ATTACKER, 1450, 150, 210, CLIP_ANCHOR_DEFENDER, CLIP_POINT_MIDDLE, 3000, 0,
                  CLIP_EASE_IN_OUT),
              KEY(60, CLIP_ANCHOR_ATTACKER, 1300, 120, 160, CLIP_ANCHOR_DEFENDER, CLIP_POINT_MIDDLE, 3000, 0,
                  CLIP_EASE_HOLD),
          } },
        // A faint: from the side of its front, pulling up and away and looking down at its feet,
        // on a curve that starts slowly and ends gently
        { CLIP_MOMENT_FAINT, CLIP_MODE_ALWAYS, 100, 0, CLIP_CATEGORY_ANY, CLIP_TYPE_ANY, CLIP_SIDE_ANY, 2,
          {
              { 0, CLIP_ANCHOR_SUBJECT, CLIP_ANCHOR_SUBJECT, 450, 100, 250, CLIP_POINT_MIDDLE, CLIP_EASE_BEZIER,
                { 0, 0, 0 }, 0, 3000, { 0, 0 }, { 400, 0, 200, 1000 } },
              KEY(110, CLIP_ANCHOR_SUBJECT, 550, 450, 480, CLIP_ANCHOR_SUBJECT, CLIP_POINT_FEET, 3000, 0,
                  CLIP_EASE_HOLD),
          } },
#else
        { 0 },
#endif
    },
    { 0 },
};

#endif
