/* HyPocket Blocks: the shared input and statistics block. The app writes
 * input and settings, the game writes what it shows. In the emulated and the
 * native Android builds it is a file both map (BlocksSession.kt reads it, so
 * offsets must match); the PC version fills it itself (platform_sdl.c). */
#ifndef BLOCKS_SHARED_IO_H
#define BLOCKS_SHARED_IO_H

#include <stdint.h>

#define IO_MAGIC 0x49425048u /* "HPBI" */
#define IO_SIZE 512

typedef struct {
    uint32_t magic;
    uint32_t version;
    /* Written by the app. */
    float move_x;      /* strafe, -1 (left) to 1 (right) */
    float move_y;      /* -1 (back) to 1 (forward) */
    float look_yaw;    /* radians, accumulated since start */
    float look_pitch;  /* radians, accumulated since start */
    int32_t jump;      /* 1 while the jump button is held */
    int32_t breaks;    /* incremented on each tap: break the aimed block */
    int32_t places;    /* incremented on each place press */
    int32_t quit;      /* 1: exit */
    float look_rate_yaw;   /* radians per second, from a controller stick */
    float look_rate_pitch; /* radians per second */
    int32_t selected;  /* index into the building palette (palette[] below) */
    int32_t crafts;    /* incremented on each request to make blocks with craft[] */
    uint8_t craft[8];  /* palette index and amount of up to two inputs (255: none), then of the output, then
                        * the palette index of the workstation it needs nearby and of the tool it needs (255: none) */
    /* Written by the game. */
    int32_t ready;     /* 1 once the world is built and frames are shown */
    int32_t frames;
    float fps;         /* over the last second */
    float frame_ms;    /* CPU + GPU time of one frame, over the last second */
    int32_t triangles; /* in the last frame */
    int32_t chunks;    /* drawn in the last frame */
    float build_ms;    /* time to generate and mesh the world */
    int32_t width;
    int32_t height;
    int32_t scale_percent; /* dynamic resolution: share of width and height drawn */
    int32_t creatures;     /* drawn in the last frame */
    int32_t saves;         /* times the world was written to its save file */
    int32_t clock_minutes; /* time of day in the game, minutes after midnight */
    int32_t day;           /* days passed in this world, from 1 */
    int32_t finished;      /* 1 once the game has saved and is about to exit normally */
    int32_t reserved_out;
    int16_t counts[16];    /* the first 16 of all_counts below */
    int32_t health_tenths; /* the player's health, 0-100 (MAX_HEALTH is 10 points) */
    int32_t deaths;        /* times the player fainted and woke up at the start */
    int32_t hurts;         /* incremented each time the player is hurt */
    int32_t night_near;    /* night creatures within 24 blocks */
    /* Written by the app: the pause menu and the in-game settings. */
    int32_t paused;        /* 1: the world stands still (it is still drawn, at 30 fps) */
    int32_t view_distance; /* blocks; 0: the mode's default */
    int32_t volume_percent;
    int32_t fps_setting;   /* 0: the command line's limit, -1: none, else the limit */
    int32_t scale_percent_max; /* 3D resolution, 50-100 (0: 100); dynamic resolution stays below it */
    /* Written by the game: the beacons (the world's goal). */
    int32_t beacons;       /* in the world (0 in the benchmark) */
    int32_t beacons_lit;
    int32_t beacon_angle;  /* degrees from where the player looks to the nearest unlit beacon, right positive */
    int32_t beacon_distance; /* blocks to it */
    int32_t goal_done;     /* 1 once every beacon is lit */
    int32_t play_seconds;  /* time played in this world */
    int32_t done_seconds;  /* time played when the last beacon was lit */
    int32_t mist_percent;  /* how thick the mist is where the player stands */
    int32_t aiming_beacon; /* 1: the crosshair is on an unlit beacon */
    /* Written by the game: the story (grabs, shelters, notes, the other side). */
    int32_t story_flags;   /* STORY_* bits below */
    int32_t grab_percent;  /* while grabbed: how far the player has got free, 0-100 */
    int32_t notes_opened;  /* incremented each time the player opens a note */
    int32_t note_id;       /* the note opened last */
    int32_t notes_found;   /* one bit per note found */
    int32_t takes;         /* times the player was dragged away */
    /* Written by the game: survival (from offset 256, see Survival). */
    int16_t all_counts[48]; /* blocks and items carried, in palette order (unlimited in creative mode) */
    int32_t food_tenths;   /* how full the player is, 0-100 */
    int32_t warmth_tenths; /* how warm, 0-100 */
    int32_t survival_flags; /* SURVIVAL_* bits below */
    int32_t dig_percent;   /* how far the aimed block is from breaking, 0-100 (0: not started) */
    int32_t picks;         /* incremented each time something is picked up or hunted */
    int32_t picked;        /* palette index of what was picked up last */
    int32_t needs;         /* incremented each time something needs a tool the player does not carry */
    int32_t needed;        /* palette index of that tool */
    int32_t eats;          /* incremented each time the player eats */
    uint8_t reserved[124];
} SharedIo;

#define STORY_GRABBED 1   /* a night creature holds the player */
#define STORY_SHELTERED 2 /* in a closed, lit room */
#define STORY_AIM_NOTE 4  /* the crosshair is on a note */
#define STORY_AWAY 8      /* on the other side */
#define STORY_AIM_PICKUP 16 /* the crosshair is on something lying on the ground */

#define SURVIVAL_NEAR_KILN 1     /* a kiln within reach: its recipes can be made */
#define SURVIVAL_NEAR_STOVE 2
#define SURVIVAL_NEAR_CAMPFIRE 4
#define SURVIVAL_COLD 8          /* losing warmth */
#define SURVIVAL_WARMING 16      /* by a fire */

#endif
