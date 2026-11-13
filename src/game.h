// Game rules: maps, exploring, turn-based battles, stats and the game flow.
//
// Nothing in this file or in game.c depends on raylib or on any platform, so
// the rules can be tested on any machine (see tests/) and the same code runs
// on PC and Android. main.c turns input into a GameInput, calls game_step()
// at a fixed 60 steps per second, and draws what is in the Game.
//
// The player is a lumberjack. On the map (seen from above) he walks, chops
// trees out of the way with his axe, opens chests and rests at campfires.
// Touching a monster starts a turn-based battle: he picks an action, then
// every monster still standing takes its turn. Winning gives experience and
// levels. The last map ends with a boss.
#ifndef TIMBERHEART_GAME_H
#define TIMBERHEART_GAME_H

#include <stdbool.h>

// Size of one tile and of the virtual screen, in pixels. The game draws at
// this small resolution and the picture is scaled up to the window, which
// keeps the GPU work tiny on low-end phones.
#define TILE 16
#define VIEW_W 320
#define VIEW_H 180

#define MAX_MAP_W 96
#define MAX_MAP_H 64
#define MAX_FOES 48
#define MAX_BATTLE_ENEMIES 3
#define MAX_BATTLE_MSGS 32

#define STEP_DT (1.0f / 60.0f)
#define WALKER_SIZE 10  // hitbox of the lumberjack and of monsters on the map

// --- Map ----------------------------------------------------------------------

typedef enum {
    TILE_FLOOR = 0,  // '.' grass
    TILE_WALL,       // '#' cliff, blocks
    TILE_TREE,       // 'T' tree, blocks until chopped
    TILE_STUMP,      //     what is left of a chopped tree
    TILE_ROCK,       // 'R' rock, blocks, too hard to chop
    TILE_WATER,      // '~' water, blocks
    TILE_BRIDGE,     // '=' bridge over water
    TILE_CHEST,      // 'c' chest with herbs
    TILE_CHEST_AXE,  // 'A' chest with a sharper axe head
    TILE_CHEST_OPEN,
    TILE_FIRE,       // 'f' campfire: rest to heal; you come back here if you fall
    TILE_EXIT,       // 'X' path to the next map
} TileType;

// --- Monsters -------------------------------------------------------------------

typedef enum {
    SPECIES_SLIME,
    SPECIES_BEETLE,
    SPECIES_WOLF,
    SPECIES_ELDER_STUMP,  // the boss
    SPECIES_COUNT,
} Species;

typedef struct {
    int max_hp, atk, def, xp;
    bool wooden;     // an axe hits wooden things harder
    int herb_drop;   // chance in percent of leaving a herb
} SpeciesInfo;

const SpeciesInfo *species_info(Species s);

// A group of monsters walking on the map. Touching it starts a battle
// against the monsters listed for its kind (see encounter_party()).
typedef enum {
    ENCOUNTER_SLIME,   // 's': one slime
    ENCOUNTER_SLIMES,  // 'g': two slimes
    ENCOUNTER_BEETLE,  // 'b': a beetle and a slime
    ENCOUNTER_WOLF,    // 'v': one wolf
    ENCOUNTER_WOLVES,  // 'w': two wolves
    ENCOUNTER_BOSS,    // 'B': the Elder Stump; does not move
    ENCOUNTER_COUNT,
} Encounter;

// Fills `out` with the species in that encounter and returns how many.
int encounter_party(Encounter e, Species out[MAX_BATTLE_ENEMIES]);

typedef struct {
    Encounter kind;
    float x, y, vx, vy;
    float think;  // time until it picks a new direction
    bool alive;
} Foe;

// --- The lumberjack ---------------------------------------------------------------

typedef struct {
    int level, xp;
    int hp, max_hp;
    int sp, max_sp;  // stamina, spent on skills
    int atk, def;
    int herbs;
} Stats;

#define HERB_HEAL 30
#define SKILL_SPLIT_COST 4  // Splitting Blow: one strong hit
#define SKILL_SPIN_COST 5   // Whirlwind Chop: hits every monster
#define AXE_UPGRADE_ATK 3

int xp_to_next_level(int level);

typedef struct {
    float x, y;            // top-left corner of the hitbox, in pixels
    int face_x, face_y;    // one of the 4 directions
    float walk_anim;
    float safe_timer;      // monsters ignore him while above zero
} Walker;

// --- Battle -------------------------------------------------------------------

typedef enum {
    MENU_ATTACK,
    MENU_SKILLS,
    MENU_HERB,
    MENU_DEFEND,
    MENU_FLEE,
    MENU_COUNT,
} MenuItem;

typedef enum {
    SKILL_SPLIT,
    SKILL_SPIN,
    SKILL_BACK,  // the last entry of the skills menu goes back
    SKILL_MENU_COUNT,
} SkillItem;

typedef enum {
    PHASE_MENU,     // choose an action
    PHASE_SKILLS,   // choose a skill
    PHASE_TARGET,   // choose which monster
    PHASE_MESSAGES, // the results play one message at a time
} BattlePhase;

// One line of the battle story. Effects are applied to the rules at once;
// the screen shows them when the message plays, using the *_after values.
typedef enum {
    MSG_START,           // amount = encounter
    MSG_ATTACK,          // target, amount, crit
    MSG_SPLIT,           // target, amount, crit
    MSG_SPIN,            // then one MSG_SPIN_HIT per monster
    MSG_SPIN_HIT,        // target, amount, crit
    MSG_HERB,            // amount healed
    MSG_DEFEND,
    MSG_FLEE_OK,
    MSG_FLEE_FAIL,
    MSG_CANT_FLEE,
    MSG_ENEMY_ATTACK,    // actor, amount
    MSG_ENEMY_SPECIAL,   // actor, amount (wolf bite, boss roots)
    MSG_ENEMY_DOWN,      // target
    MSG_VICTORY,         // amount = xp
    MSG_FOUND_HERB,      // amount = herbs
    MSG_LEVEL_UP,        // amount = new level
    MSG_PLAYER_DOWN,
} MsgType;

typedef struct {
    MsgType type;
    int actor, target, amount;
    bool crit;
    int enemy_hp_after;   // hp of `target` after this message
    int player_hp_after, player_sp_after;
} BattleMsg;

typedef struct {
    Species species;
    int hp;
    int shown_hp;      // what the screen shows, catches up with the messages
    bool shown_alive;
    float flash;       // blinks after being hit
} Combatant;

typedef enum { OUTCOME_NONE, OUTCOME_WON, OUTCOME_FLED, OUTCOME_LOST } Outcome;

typedef struct {
    int foe;  // index of the map foe that started it
    Combatant enemies[MAX_BATTLE_ENEMIES];
    int enemy_count;
    BattlePhase phase;
    int cursor;        // in the main menu
    int skill_cursor;  // in the skills menu
    int target;        // chosen monster
    int pending;       // MENU_ATTACK or SKILL_SPLIT waiting for a target
    bool defending;
    int turn;
    BattleMsg msgs[MAX_BATTLE_MSGS];
    int msg_count, msg_index;
    float msg_timer;
    int shown_hp, shown_sp;
    float player_flash;
    Outcome outcome;
} Battle;

// --- Game -----------------------------------------------------------------------

typedef enum {
    STATE_TITLE,
    STATE_MAP,
    STATE_PAUSED,
    STATE_BATTLE,
    STATE_GAME_OVER,  // then he wakes up at the last campfire
    STATE_WON,
} GameState;

// Short notes shown on the map.
typedef enum {
    TOAST_NONE,
    TOAST_HERBS,     // amount = herbs found
    TOAST_AXE,       // the axe got sharper
    TOAST_REST,      // rested at the campfire
    TOAST_TOO_HARD,  // tried to chop a rock
    TOAST_MAP,       // arrived at a new map
} Toast;

// Things that happened during the last step, for sounds and effects.
enum {
    EVENT_CHOP = 1 << 0,
    EVENT_CHEST = 1 << 1,
    EVENT_REST = 1 << 2,
    EVENT_ENCOUNTER = 1 << 3,
    EVENT_CURSOR = 1 << 4,     // a menu cursor moved
    EVENT_CONFIRM = 1 << 5,
    EVENT_DENIED = 1 << 6,     // tried something not possible now
    EVENT_HIT = 1 << 7,        // a monster was hurt
    EVENT_HURT = 1 << 8,       // the lumberjack was hurt
    EVENT_HEAL = 1 << 9,
    EVENT_DEFEATED = 1 << 10,  // a monster fell
    EVENT_VICTORY = 1 << 11,
    EVENT_LEVEL_UP = 1 << 12,
    EVENT_FLEE = 1 << 13,
    EVENT_PLAYER_DOWN = 1 << 14,
    EVENT_EXIT = 1 << 15,
    EVENT_BUMP = 1 << 16,      // swung the axe at something that does not break
};

typedef struct {
    float move_x, move_y;  // direction on the map, each from -1 to 1
    int nav_x, nav_y;      // menu moves this step: -1, 0 or 1
    bool confirm_pressed;  // action on the map, choose in menus
    bool cancel_pressed;   // back in menus
    bool pause_pressed;
    int pick;              // touch: menu entry tapped (1-based), 0 = none
    int target_pick;       // touch: monster tapped (1-based), 0 = none
} GameInput;

typedef struct {
    GameState state;
    float state_timer;

    int level;  // index into the map list
    int w, h;   // map size in tiles
    unsigned char tiles[MAX_MAP_H][MAX_MAP_W];
    float start_x, start_y;
    float respawn_x, respawn_y;

    Walker walker;
    Foe foes[MAX_FOES];
    int foe_count;
    Stats stats;
    Battle battle;

    Toast toast;
    int toast_amount;
    float toast_timer;

    unsigned rng;
    float camera_x, camera_y;
    float time;       // seconds since the game started, for animation
    unsigned events;  // EVENT_* flags set by the last step
} Game;

// Maps, defined in levels.c.
int level_count(void);
const char *const *level_rows(int index, int *row_count);

// Fills the map part of `g` from text rows (see levels.c). Returns false when
// the map is too big or has no start ('P'), or neither an exit nor a boss.
bool level_load_rows(Game *g, const char *const *rows, int row_count);

void game_init(Game *g);       // title screen
void game_new(Game *g);        // a new lumberjack on the first map
void game_enter_level(Game *g, int index);
void game_start_battle(Game *g, int foe);
void game_step(Game *g, const GameInput *in);

TileType game_tile_at(const Game *g, int tx, int ty);
bool game_tile_blocks(TileType t);
int skill_cost(SkillItem s);
bool battle_menu_enabled(const Game *g, MenuItem item);
bool battle_skill_enabled(const Game *g, SkillItem item);
// The message playing now, or NULL.
const BattleMsg *battle_current_msg(const Game *g);

#endif
