// Game rules: maps, movement, enemies, items and the game flow.
//
// Nothing in this file or in game.c depends on raylib or on any platform, so
// the rules can be tested on any machine (see tests/) and the same code runs
// on PC and Android. main.c turns input into a GameInput, calls game_step()
// at a fixed 60 steps per second, and draws what is in the Game.
//
// The game is seen from above: the player walks in any direction through a
// map, collects every seed to open the portal, and walks into the portal to
// reach the next map. Keys open doors, enemies are fought with a swing.
#ifndef MOSSLING_GAME_H
#define MOSSLING_GAME_H

#include <stdbool.h>

// Size of one tile and of the virtual screen, in pixels. The game draws at
// this small resolution and the picture is scaled up to the window, which
// keeps the GPU work tiny on low-end phones.
#define TILE 16
#define VIEW_W 320
#define VIEW_H 180

#define MAX_MAP_W 96
#define MAX_MAP_H 64
#define MAX_ENEMIES 48

#define STEP_DT (1.0f / 60.0f)
#define MAX_HEALTH 3
#define ATTACK_TIME 0.18f  // how long a swing lasts

typedef enum {
    TILE_FLOOR = 0, // '.' or ' ': grass
    TILE_WALL,      // '#': stone wall
    TILE_TREE,      // 'T': tree, blocks like a wall
    TILE_WATER,     // '~': water, blocks
    TILE_THORNS,    // '^': can be walked on, but hurts
    TILE_SEED,      // 'o': collect all of them to open the portal
    TILE_KEY,       // 'k': opens one door
    TILE_DOOR,      // 'D': blocks until opened with a key
    TILE_HEART,     // 'h': gives back one heart
    TILE_PORTAL,    // 'X': the exit, open once every seed is collected
} TileType;

typedef enum {
    ENEMY_BEETLE, // 'b': wanders, turns when blocked
    ENEMY_SLIME,  // 's': follows the player when near; takes two hits
} EnemyKind;

typedef enum {
    STATE_TITLE,
    STATE_PLAYING,
    STATE_PAUSED,
    STATE_DYING,      // short pause after losing the last heart
    STATE_GAME_OVER,  // then the map starts again
    STATE_LEVEL_DONE, // short pause after entering the portal
    STATE_WON,
} GameState;

// Things that happened during the last step, for sounds and effects.
enum {
    EVENT_SWING = 1 << 0,
    EVENT_HIT = 1 << 1,      // an enemy was hit
    EVENT_DEFEAT = 1 << 2,   // an enemy was defeated
    EVENT_HURT = 1 << 3,     // the player lost a heart
    EVENT_SEED = 1 << 4,
    EVENT_KEY = 1 << 5,
    EVENT_DOOR = 1 << 6,
    EVENT_HEART = 1 << 7,
    EVENT_PORTAL = 1 << 8,   // the portal just opened
    EVENT_EXIT = 1 << 9,     // the player entered the portal
};

typedef struct {
    float move_x, move_y;  // direction, each from -1 to 1
    bool attack_pressed;   // attack button went down this step
    bool start_pressed;    // confirm / start / pause
} GameInput;

typedef struct {
    float x, y;            // top-left corner of the hitbox, in pixels
    float kx, ky;          // knockback speed, fades quickly
    float face_x, face_y;  // last direction walked (one of 8)
    int health;
    float hurt_timer;      // invulnerable (and blinking) while above zero
    float attack_timer;    // the swing is active while above zero
    float attack_cooldown;
    float walk_anim;
} Player;

typedef struct {
    EnemyKind kind;
    float x, y;
    float vx, vy;
    float kx, ky;
    int health;
    bool alive;
    float think;       // time until a beetle picks a new direction
    float hit_flash;
    unsigned hit_swing;  // the swing that last hit it, so one swing hits once
} Enemy;

typedef struct {
    GameState state;
    float state_timer;

    int level;  // index into the level list
    int w, h;   // map size in tiles
    unsigned char tiles[MAX_MAP_H][MAX_MAP_W];
    float start_x, start_y;

    Player player;
    Enemy enemies[MAX_ENEMIES];
    int enemy_count;

    int seeds, level_seeds;  // collected / total in this map
    int keys;
    int total_seeds;         // kept from finished maps
    unsigned swing;          // counts swings
    unsigned rng;

    float camera_x, camera_y;
    float time;       // seconds since the map started, for animation
    unsigned events;  // EVENT_* flags set by the last step
} Game;

// Hitboxes, smaller than a tile so the player fits through 1-tile paths.
#define PLAYER_SIZE 10
#define ENEMY_SIZE 10

// Maps, defined in levels.c.
int level_count(void);
const char *const *level_rows(int index, int *row_count);

// Fills `g` from a map written as text rows (see levels.c). Returns false
// when the map is too big or has no start ('P') or portal ('X').
bool level_load_rows(Game *g, const char *const *rows, int row_count);

void game_init(Game *g);  // title screen
void game_start_level(Game *g, int index);
void game_step(Game *g, const GameInput *in);

TileType game_tile_at(const Game *g, int tx, int ty);
bool game_tile_blocks(TileType t);
bool game_portal_open(const Game *g);

// The area the current swing hits, valid while player.attack_timer > 0.
void game_attack_box(const Game *g, float *x, float *y, float *w, float *h);

#endif
