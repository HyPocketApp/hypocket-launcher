#include "game.h"
#include "story.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

// Tuning, in pixels and seconds.
#define WALK_SPEED 72.0f
#define FOE_SPEED 22.0f
#define SAFE_TIME 2.0f       // after fleeing or waking up, monsters leave him be
#define MSG_TIME 1.1f        // how long a battle message stays
#define MSG_MIN_TIME 0.2f    // a press skips a message only after this
#define TOAST_TIME 2.2f
#define MENU_DELAY 0.4f      // ignore presses right after a screen appears
#define PAGE_DELAY 0.15f     // between pages of a story scene

// --- Tables -------------------------------------------------------------------

static const SpeciesInfo species_table[SPECIES_COUNT] = {
    [SPECIES_SLIME] = { .max_hp = 18, .atk = 9, .def = 1, .xp = 5, .spd = 5, .wits = 15, .herb_drop = 8 },
    [SPECIES_BEETLE] = { .max_hp = 28, .atk = 11, .def = 4, .xp = 8, .spd = 4, .wits = 55, .herb_drop = 10 },
    [SPECIES_WOLF] = { .max_hp = 32, .atk = 12, .def = 2, .xp = 12, .spd = 13, .wits = 75, .herb_drop = 12 },
    [SPECIES_EMBER] = { .max_hp = 9, .atk = 7, .def = 0, .xp = 6, .spd = 8, .wits = 45, .herb_drop = 10 },
    [SPECIES_ELDER_STUMP] = { .max_hp = 300, .atk = 19, .def = 8, .xp = 100, .spd = 7, .wits = 90, .wooden = true },
};

const SpeciesInfo *species_info(Species s) { return &species_table[s]; }

int encounter_party(Encounter e, Species out[MAX_BATTLE_ENEMIES])
{
    switch (e) {
    case ENCOUNTER_SLIME: out[0] = SPECIES_SLIME; return 1;
    case ENCOUNTER_SLIMES: out[0] = out[1] = SPECIES_SLIME; return 2;
    case ENCOUNTER_BEETLE: out[0] = SPECIES_BEETLE; out[1] = SPECIES_SLIME; return 2;
    case ENCOUNTER_WOLF: out[0] = SPECIES_WOLF; return 1;
    case ENCOUNTER_WOLVES: out[0] = out[1] = SPECIES_WOLF; return 2;
    case ENCOUNTER_BOSS: out[0] = SPECIES_ELDER_STUMP; return 1;
    case ENCOUNTER_EMBERS: out[0] = out[1] = out[2] = SPECIES_EMBER; return 3;
    default: return 0;
    }
}

int scene_page_count(Scene s)
{
    return story_page_count(s);
}

// Encounters that stay where they are on the map.
static bool encounter_stays(Encounter e) { return e == ENCOUNTER_BOSS || e == ENCOUNTER_EMBERS; }

int xp_to_next_level(int level) { return level * 15; }

int skill_cost(SkillItem s) { return s == SKILL_SPLIT ? SKILL_SPLIT_COST : s == SKILL_SPIN ? SKILL_SPIN_COST : 0; }

int spell_cost(SpellItem s) { return s == SPELL_BEAM ? SPELL_BEAM_COST : s == SPELL_LIGHT ? SPELL_LIGHT_COST : 0; }

// --- Helpers ------------------------------------------------------------------

TileType game_tile_at(const Game *g, int tx, int ty)
{
    if (tx < 0 || ty < 0 || tx >= g->w || ty >= g->h) return TILE_WALL;
    return (TileType)g->tiles[ty][tx];
}

bool game_tile_blocks(TileType t)
{
    return t == TILE_WALL || t == TILE_TREE || t == TILE_ROCK || t == TILE_WATER || t == TILE_CHEST ||
           t == TILE_CHEST_AXE || t == TILE_CHEST_OPEN || t == TILE_FIRE || t == TILE_GODDESS || t == TILE_HOUSE ||
           t == TILE_DOOR || t == TILE_WOODPILE || t == TILE_BUSH;
}

static bool foe_blocked_by(TileType t) { return game_tile_blocks(t) || t == TILE_EXIT || t == TILE_BRIDGE; }

static int tile_floor(float v) { return (int)floorf(v / TILE); }

static bool overlaps(float ax, float ay, float aw, float ah, float bx, float by, float bw, float bh)
{
    return ax < bx + bw && bx < ax + aw && ay < by + bh && by < ay + ah;
}

static unsigned next_random(Game *g)
{
    // xorshift32: small, fast and the same on every platform.
    unsigned x = g->rng ? g->rng : 0x9e3779b9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return g->rng = x;
}

static float random_unit(Game *g) { return (float)(next_random(g) % 10000u) / 10000.0f; }
static bool chance(Game *g, int percent) { return (int)(next_random(g) % 100u) < percent; }

// Moves a box by (dx, dy), one axis at a time, stopping at tiles for which
// `blocks` is true. Returns true when it bumped into something.
static bool move_box(const Game *g, float *x, float *y, float dx, float dy, bool (*blocks)(TileType))
{
    const float size = WALKER_SIZE;
    bool hit = false;
    if (dx != 0) {
        *x += dx;
        int top = tile_floor(*y + 0.01f), bottom = tile_floor(*y + size - 0.01f);
        int tx = dx > 0 ? tile_floor(*x + size - 0.01f) : tile_floor(*x);
        for (int ty = top; ty <= bottom; ty++) {
            if (blocks(game_tile_at(g, tx, ty))) {
                *x = dx > 0 ? tx * TILE - size : (tx + 1) * TILE;
                hit = true;
                break;
            }
        }
    }
    if (dy != 0) {
        *y += dy;
        int left = tile_floor(*x + 0.01f), right = tile_floor(*x + size - 0.01f);
        int ty = dy > 0 ? tile_floor(*y + size - 0.01f) : tile_floor(*y);
        for (int tx = left; tx <= right; tx++) {
            if (blocks(game_tile_at(g, tx, ty))) {
                *y = dy > 0 ? ty * TILE - size : (ty + 1) * TILE;
                hit = true;
                break;
            }
        }
    }
    return hit;
}

static void show_toast(Game *g, Toast t, int amount)
{
    g->toast = t;
    g->toast_amount = amount;
    g->toast_timer = TOAST_TIME;
}

// --- Loading ------------------------------------------------------------------

bool area_load_rows(Game *g, const char *const *rows, int row_count)
{
    if (row_count <= 0 || row_count > MAX_MAP_H) return false;

    memset(g->tiles, 0, sizeof g->tiles);
    memset(g->links, 0, sizeof g->links);
    g->start_x = g->start_y = -1;
    g->h = row_count;
    g->w = 0;
    g->foe_count = 0;
    bool has_end = false;

    for (int ty = 0; ty < row_count; ty++) {
        int len = (int)strlen(rows[ty]);
        if (len > MAX_MAP_W) return false;
        if (len > g->w) g->w = len;
    }
    for (int ty = 0; ty < row_count; ty++) {
        int len = (int)strlen(rows[ty]);
        for (int tx = 0; tx < g->w; tx++) {
            char c = tx < len ? rows[ty][tx] : '#';  // short rows end in cliff
            unsigned char t = TILE_FLOOR;
            int encounter = -1;
            switch (c) {
            case '#': t = TILE_WALL; break;
            case 'T': t = TILE_TREE; break;
            case 'R': t = TILE_ROCK; break;
            case '~': t = TILE_WATER; break;
            case '=': t = TILE_BRIDGE; break;
            case 'c': t = TILE_CHEST; break;
            case 'A': t = TILE_CHEST_AXE; break;
            case 'f': t = TILE_FIRE; break;
            case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
                t = TILE_EXIT;
                g->links[ty][tx] = (unsigned char)(c - '0');
                has_end = true;
                break;
            case 'M': t = TILE_GODDESS; break;
            case 'H': t = TILE_HOUSE; break;
            case 'D': t = TILE_DOOR; break;
            case ':': t = TILE_PATH; break;
            case 'W': t = TILE_WOODPILE; break;
            case 'o': t = TILE_BUSH; break;
            case ',': t = TILE_FLOWERS; break;
            case 'P':
                g->start_x = tx * TILE + (TILE - WALKER_SIZE) / 2.0f;
                g->start_y = ty * TILE + (TILE - WALKER_SIZE) / 2.0f;
                break;
            case 's': encounter = ENCOUNTER_SLIME; break;
            case 'g': encounter = ENCOUNTER_SLIMES; break;
            case 'b': encounter = ENCOUNTER_BEETLE; break;
            case 'v': encounter = ENCOUNTER_WOLF; break;
            case 'w': encounter = ENCOUNTER_WOLVES; break;
            case 'B': encounter = ENCOUNTER_BOSS; has_end = true; break;
            case 'e': encounter = ENCOUNTER_EMBERS; break;
            default: break;
            }
            if (encounter >= 0 && g->foe_count < MAX_FOES) {
                Foe *f = &g->foes[g->foe_count++];
                memset(f, 0, sizeof *f);
                f->kind = (Encounter)encounter;
                f->x = tx * TILE + (TILE - WALKER_SIZE) / 2.0f;
                f->y = ty * TILE + (TILE - WALKER_SIZE) / 2.0f;
                f->alive = true;
            }
            g->tiles[ty][tx] = t;
        }
    }
    return has_end;
}

// --- Map ------------------------------------------------------------------------

static void start_scene(Game *g, Scene scene, GameState after)
{
    g->scene = scene;
    g->scene_page = 0;
    g->scene_return = after;
    g->state = STATE_SCENE;
    g->state_timer = MENU_DELAY;
}

static void update_camera(Game *g)
{
    const Walker *w = &g->walker;
    float cx = w->x + WALKER_SIZE / 2.0f - VIEW_W / 2.0f;
    float cy = w->y + WALKER_SIZE / 2.0f - VIEW_H / 2.0f;
    float max_x = (float)(g->w * TILE - VIEW_W), max_y = (float)(g->h * TILE - VIEW_H);
    g->camera_x = max_x <= 0 ? max_x / 2.0f : fmaxf(0.0f, fminf(cx, max_x));
    g->camera_y = max_y <= 0 ? max_y / 2.0f : fmaxf(0.0f, fminf(cy, max_y));
}

// Heals both of them; he will wake up here after losing a battle.
static void rest(Game *g, Toast toast)
{
    g->stats.hp = g->stats.max_hp;
    g->stats.sp = g->stats.max_sp;
    g->goddess.hp = g->goddess.max_hp;
    g->goddess.mp = g->goddess.max_mp;
    g->respawn_area = g->area;
    g->respawn_x = g->walker.x;
    g->respawn_y = g->walker.y;
    g->events |= EVENT_REST | EVENT_SAVE;  // resting saves the game
    show_toast(g, toast, 0);
}

// Moves a walker toward its next tile. Returns true on arrival.
static bool walker_advance(Walker *w, float speed, float dt)
{
    float dx = w->to_x - w->x, dy = w->to_y - w->y;
    float dist = sqrtf(dx * dx + dy * dy), step = speed * dt;
    if (dist <= step) {
        w->x = w->to_x;
        w->y = w->to_y;
        w->walk_anim += dist;
        return true;
    }
    w->x += dx / dist * step;
    w->y += dy / dist * step;
    w->walk_anim += step;
    return false;
}

// The goddess walks to the tile the lumberjack just left.
static void update_follower(Game *g, float dt)
{
    Walker *f = &g->follower;
    if (!f->moving) {
        f->walk_anim = 0;
        return;
    }
    float dx = f->to_x - f->x, dy = f->to_y - f->y;
    if (fabsf(dx) > fabsf(dy)) { f->face_x = dx > 0 ? 1 : -1; f->face_y = 0; }
    else if (dy != 0) { f->face_x = 0; f->face_y = dy > 0 ? 1 : -1; }
    // Hurries when she is far behind (just after joining).
    float speed = dx * dx + dy * dy > 1.5f * TILE * 1.5f * TILE ? WALK_SPEED * 2.0f : WALK_SPEED;
    if (walker_advance(f, speed, dt)) f->moving = false;
}

// She stands where he stands (after arriving in an area or waking up).
static void reset_follower(Game *g)
{
    Walker *f = &g->follower;
    memset(f, 0, sizeof *f);
    f->x = f->to_x = g->walker.x;
    f->y = f->to_y = g->walker.y;
    f->face_x = g->walker.face_x;
    f->face_y = g->walker.face_y;
}

// The other area with an exit marked `link`, or -1.
static int linked_area(int from, int link)
{
    char c = (char)('0' + link);
    for (int a = 0; a < area_count(); a++) {
        if (a == from) continue;
        int count = 0;
        const char *const *rows = area_rows(a, &count);
        for (int r = 0; r < count; r++)
            if (strchr(rows[r], c)) return a;
    }
    return -1;
}

// The action button on the tile in front of the lumberjack: chests,
// campfire, door. (Trees, bushes and rocks stay as they are.)
static void map_action(Game *g)
{
    Walker *w = &g->walker;
    float cx = w->x + WALKER_SIZE / 2.0f + w->face_x * 11.0f;
    float cy = w->y + WALKER_SIZE / 2.0f + w->face_y * 11.0f;
    int tx = tile_floor(cx), ty = tile_floor(cy);
    switch (game_tile_at(g, tx, ty)) {
    case TILE_CHEST:
        g->tiles[ty][tx] = TILE_CHEST_OPEN;
        g->stats.herbs += CHEST_HERBS;
        g->events |= EVENT_CHEST;
        show_toast(g, TOAST_HERBS, CHEST_HERBS);
        break;
    case TILE_CHEST_AXE:
        g->tiles[ty][tx] = TILE_CHEST_OPEN;
        g->stats.atk += AXE_UPGRADE_ATK;
        g->events |= EVENT_CHEST;
        show_toast(g, TOAST_AXE, AXE_UPGRADE_ATK);
        break;
    case TILE_FIRE:
        rest(g, TOAST_REST);
        break;
    case TILE_DOOR:
        rest(g, TOAST_HOME);
        break;
    default:
        break;
    }
}

// Reaching the fire creatures (within 3 tiles) plays their scene, then the
// battle; so does reaching the sleeping Elder Stump (within 4), where the
// creatures that fled wake him. Returns true when a scene started.
static bool meet_creatures(Game *g)
{
    int wx = tile_floor(g->walker.x + WALKER_SIZE / 2.0f), wy = tile_floor(g->walker.y + WALKER_SIZE / 2.0f);
    for (int i = 0; i < g->foe_count; i++) {
        const Foe *f = &g->foes[i];
        if (!f->alive) continue;
        bool embers = f->kind == ENCOUNTER_EMBERS && !g->creatures_met;
        bool elder = f->kind == ENCOUNTER_BOSS && !g->elder_woke;
        if (!embers && !elder) continue;
        int fx = tile_floor(f->x + WALKER_SIZE / 2.0f), fy = tile_floor(f->y + WALKER_SIZE / 2.0f);
        int reach = elder ? 4 : 3;
        if (abs(fx - wx) > reach || abs(fy - wy) > reach) continue;
        if (embers) g->creatures_met = true;
        else g->elder_woke = true;
        g->scene_foe = i;
        start_scene(g, embers ? SCENE_CREATURES : SCENE_ELDER, STATE_BATTLE);
        return true;
    }
    return false;
}

// Until the goddess has joined him, the lumberjack does not go deeper into
// the forest than her woods: only the way back home stays open.
static bool exit_locked(const Game *g, int tx, int ty)
{
    if (game_tile_at(g, tx, ty) != TILE_EXIT || g->goddess.joined) return false;
    return g->area >= AREA_WOODS && linked_area(g->area, g->links[ty][tx]) > g->area;
}

// Walks onto an exit: goes to the area on the other side.
static bool take_exit(Game *g)
{
    const Walker *w = &g->walker;
    int cx = tile_floor(w->x + WALKER_SIZE / 2.0f), cy = tile_floor(w->y + WALKER_SIZE / 2.0f);
    if (game_tile_at(g, cx, cy) != TILE_EXIT) return false;
    int link = g->links[cy][cx];
    int to = linked_area(g->area, link);
    if (to < 0) return false;
    g->events |= EVENT_EXIT;
    game_enter_area(g, to, link);
    return true;
}

static void update_walker(Game *g, const GameInput *in, float dt)
{
    Walker *w = &g->walker;
    if (w->safe_timer > 0) w->safe_timer -= dt;

    // Only up, down, left or right: the stronger axis of the stick wins.
    int dir_x = 0, dir_y = 0;
    float mx = in->move_x, my = in->move_y;
    if (fabsf(mx) > 0.3f || fabsf(my) > 0.3f) {
        if (fabsf(mx) > fabsf(my)) dir_x = mx > 0 ? 1 : -1;
        else dir_y = my > 0 ? 1 : -1;
    }

    if (w->moving && walker_advance(w, WALK_SPEED, dt)) {
        w->moving = false;
        if (take_exit(g)) return;
        if (meet_creatures(g)) return;
    }
    if (!w->moving && (dir_x || dir_y)) {
        w->face_x = dir_x;
        w->face_y = dir_y;
        int tx = tile_floor(w->x + WALKER_SIZE / 2.0f) + dir_x;
        int ty = tile_floor(w->y + WALKER_SIZE / 2.0f) + dir_y;
        if (exit_locked(g, tx, ty)) {
            if (g->toast != TOAST_NOT_YET || g->toast_timer <= 0) show_toast(g, TOAST_NOT_YET, 0);
        } else if (!game_tile_blocks(game_tile_at(g, tx, ty))) {
            Walker *f = &g->follower;
            f->to_x = w->x;  // she takes the tile he leaves
            f->to_y = w->y;
            f->moving = true;
            w->to_x = tx * TILE + (TILE - WALKER_SIZE) / 2.0f;
            w->to_y = ty * TILE + (TILE - WALKER_SIZE) / 2.0f;
            w->moving = true;
        }
    }
    if (!w->moving) w->walk_anim = 0;
    update_follower(g, dt);

    if (in->confirm_pressed && !w->moving) map_action(g);
}

static void foe_pick_direction(Game *g, Foe *f)
{
    static const float dirs[5][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 }, { 0, 0 } };
    int d = (int)(next_random(g) % 5u);
    f->vx = dirs[d][0] * FOE_SPEED;
    f->vy = dirs[d][1] * FOE_SPEED;
    f->think = 1.0f + 2.0f * random_unit(g);
}

static void update_foes(Game *g, float dt)
{
    Walker *w = &g->walker;
    for (int i = 0; i < g->foe_count; i++) {
        Foe *f = &g->foes[i];
        if (!f->alive) continue;
        if (!encounter_stays(f->kind) && w->safe_timer <= 0) {
            f->think -= dt;
            if (f->think <= 0) foe_pick_direction(g, f);
            if (move_box(g, &f->x, &f->y, f->vx * dt, f->vy * dt, foe_blocked_by)) foe_pick_direction(g, f);
        }
        if (w->safe_timer <= 0 &&
            overlaps(w->x, w->y, WALKER_SIZE, WALKER_SIZE, f->x - 1, f->y - 1, WALKER_SIZE + 2, WALKER_SIZE + 2)) {
            if (f->kind == ENCOUNTER_EMBERS && !g->creatures_met) {  // their scene comes first
                g->creatures_met = true;
                g->scene_foe = i;
                start_scene(g, SCENE_CREATURES, STATE_BATTLE);
            } else {
                game_start_battle(g, i);
            }
            return;
        }
    }
}

// --- Battle: messages and dice ---------------------------------------------------

// How hard the monsters' moves hit, as times their attack. (#ifndef so the
// balance can be tried with other numbers: -DBITE_MULT=1.3f and so on.)
#ifndef FLAME_MULT
#define FLAME_MULT 0.7f    // fire creature: ignores armor
#endif
#ifndef HORN_MULT
#define HORN_MULT 1.6f     // beetle: hits 3 times in 4
#endif
#ifndef BITE_MULT
#define BITE_MULT 1.3f     // wolf
#endif
#ifndef HOWL_BOOST
#define HOWL_BOOST 1.2f    // wolves after a howl
#endif
#ifndef ROOTS_MULT
#define ROOTS_MULT 1.0f    // Elder Stump, on both
#endif
#ifndef SLAM_MULT
#define SLAM_MULT 1.6f     // Elder Stump, on one
#endif
#ifndef SAP_PERCENT
#define SAP_PERCENT 12     // Elder Stump heals this much of his health
#endif
#define SHELL_DEF 5        // a hardened shell adds this much armor
#define BUFF_ROUNDS 3      // how long a howl or a shell lasts
#define SPECIAL_COOLDOWN 2 // rounds before a monster can use a strong move again
#define HOLD_CHANCE 40     // percent: the roots hold the one they hit
#define SAP_HEALS 2

int hero_speed(int level) { return 6 + level; }
int goddess_speed(int level) { return 8 + level; }

static BattleMsg *push(Game *g, MsgType type, int actor, int target, int amount)
{
    Battle *b = &g->battle;
    static BattleMsg spare;
    if (b->msg_count >= MAX_BATTLE_MSGS) return &spare;
    BattleMsg *m = &b->msgs[b->msg_count++];
    memset(m, 0, sizeof *m);
    m->type = type;
    m->actor = actor;
    m->target = target;
    m->amount = amount;
    m->move = MOVE_ATTACK;
    m->slot = b->acting;
    m->enemy_hp_after = (target >= 0 && target < b->enemy_count) ? b->enemies[target].hp : 0;
    m->player_hp_after = g->stats.hp;
    m->player_sp_after = g->stats.sp;
    m->ally_hp_after = g->goddess.hp;
    m->ally_mp_after = g->goddess.mp;
    return m;
}

static int roll_damage(Game *g, float atk, int def, float mult, bool *crit)
{
    float base = atk * mult * (0.85f + 0.3f * random_unit(g));
    bool is_crit = crit != NULL && chance(g, 10);  // only the lumberjack lands critical hits
    if (crit) *crit = is_crit;
    if (is_crit) base *= 1.6f;
    int dmg = (int)lroundf(base - def);
    return dmg < 1 ? 1 : dmg;
}

static int alive_enemies(const Battle *b)
{
    int n = 0;
    for (int i = 0; i < b->enemy_count; i++) n += b->enemies[i].hp > 0;
    return n;
}

static int first_alive(const Battle *b)
{
    for (int i = 0; i < b->enemy_count; i++)
        if (b->enemies[i].hp > 0) return i;
    return 0;
}

static int min_int(int a, int b) { return a < b ? a : b; }
static int max_int(int a, int b) { return a > b ? a : b; }

// The goddess's strength follows the lumberjack's level.
void goddess_scale(Game *g, bool refill)
{
    Ally *a = &g->goddess;
    int l = g->stats.level;
    a->max_hp = 30 + 5 * l;
    a->max_mp = 8 + 2 * l;
    a->mag = 8 + 2 * l;
    a->def = 2 + l / 2;
    if (refill) {
        a->hp = a->max_hp;
        a->mp = a->max_mp;
    }
    a->hp = min_int(a->hp, a->max_hp);
    a->mp = min_int(a->mp, a->max_mp);
}

static bool goddess_fights(const Game *g) { return g->battle.ally_in && g->goddess.hp > 0; }

// A monster's armor now: its own, a hardened shell, and halved damage while
// it guards (applied by the caller).
static int enemy_def(const Combatant *e)
{
    return species_info(e->species)->def + (e->def_up > 0 ? SHELL_DEF : 0);
}

// Damage to a monster from one of the two, with its guard.
static void hurt_enemy(Game *g, MsgType type, int actor, int target, float atk, int def, float mult, bool can_crit)
{
    Battle *b = &g->battle;
    Combatant *e = &b->enemies[target];
    bool crit = false;
    int dmg = roll_damage(g, atk, def, mult, can_crit ? &crit : NULL);
    if (e->guarding) dmg = (dmg + 1) / 2;
    e->hp = e->hp > dmg ? e->hp - dmg : 0;
    push(g, type, actor, target, dmg)->crit = crit;
    if (e->hp == 0) push(g, MSG_ENEMY_DOWN, actor, target, 0);
}

static void axe_hit(Game *g, MsgType type, int target, float mult)
{
    const Combatant *e = &g->battle.enemies[target];
    if (species_info(e->species)->wooden) mult *= 1.25f;  // the axe was made for this
    hurt_enemy(g, type, ACTOR_HERO, target, (float)g->stats.atk, enemy_def(e), mult, true);
}

static int retarget(const Battle *b, int t)
{
    return t >= 0 && t < b->enemy_count && b->enemies[t].hp > 0 ? t : first_alive(b);
}

static void finish_victory(Game *g)
{
    Battle *b = &g->battle;
    Stats *s = &g->stats;
    int xp = 0, herbs = 0;
    b->acting = -1;
    for (int i = 0; i < b->enemy_count; i++) {
        const SpeciesInfo *info = species_info(b->enemies[i].species);
        xp += info->xp;
        if (chance(g, info->herb_drop)) herbs++;
    }
    s->xp += xp;
    push(g, MSG_VICTORY, -1, -1, xp);
    if (herbs > 0) {
        s->herbs += herbs;
        push(g, MSG_FOUND_HERB, -1, -1, herbs);
    }
    while (s->xp >= xp_to_next_level(s->level)) {
        s->xp -= xp_to_next_level(s->level);
        s->level++;
        s->max_hp += 8;
        s->max_sp += 2;
        s->atk += 2;
        s->def += 1;
        s->hp = s->max_hp;
        s->sp = s->max_sp;
        if (g->goddess.joined) goddess_scale(g, true);
        push(g, MSG_LEVEL_UP, -1, -1, s->level);
    }
    b->outcome = OUTCOME_WON;
}

// --- Battle: the two of them act ---------------------------------------------------

static bool can_flee(const Game *g)
{
    Encounter k = g->foes[g->battle.foe].kind;
    return k != ENCOUNTER_BOSS && k != ENCOUNTER_EMBERS;
}

// Faster than the fastest monster, easier to get away.
static int flee_chance(const Game *g)
{
    const Battle *b = &g->battle;
    int ours = hero_speed(g->stats.level), theirs = 0;
    if (goddess_fights(g)) ours = max_int(ours, goddess_speed(g->stats.level));
    for (int i = 0; i < b->enemy_count; i++)
        if (b->enemies[i].hp > 0) theirs = max_int(theirs, species_info(b->enemies[i].species)->spd);
    int c = 50 + 5 * (ours - theirs);
    return c < 15 ? 15 : c > 95 ? 95 : c;
}

static void give_herb(Game *g, int actor, int who)
{
    Stats *s = &g->stats;
    Ally *a = &g->goddess;
    if (s->herbs <= 0) return;
    if (who == 1 && !goddess_fights(g)) who = 0;
    s->herbs--;
    int heal;
    if (who == 1) {
        heal = min_int(a->max_hp - a->hp, HERB_HEAL);
        a->hp += heal;
    } else {
        heal = min_int(s->max_hp - s->hp, HERB_HEAL);
        s->hp += heal;
    }
    push(g, MSG_HERB, actor, -1, heal)->on_ally = who == 1;
}

// Plays the plan of one of the two (0 him, 1 her).
static void party_act(Game *g, int who)
{
    Battle *b = &g->battle;
    Stats *s = &g->stats;
    Ally *a = &g->goddess;
    const Plan *p = &b->plan[who];
    int actor = who == 0 ? ACTOR_HERO : ACTOR_GODDESS;
    switch (p->command) {
    case CMD_BOUND:
        if (b->held[who] > 0) b->held[who]--;
        push(g, MSG_BOUND, actor, -1, 0);
        break;
    case CMD_ATTACK:
        if (who == 0) axe_hit(g, MSG_ATTACK, retarget(b, p->target), 1.0f);
        else {
            int t = retarget(b, p->target);
            hurt_enemy(g, MSG_ATTACK, ACTOR_GODDESS, t, a->mag * 0.7f, enemy_def(&b->enemies[t]), 1.0f, false);
        }
        break;
    case CMD_SPECIAL:
        if (who == 0) {
            s->sp -= skill_cost((SkillItem)p->sub);
            if (p->sub == SKILL_SPLIT) {
                axe_hit(g, MSG_SPLIT, retarget(b, p->target), 1.5f);
            } else {
                push(g, MSG_SPIN, ACTOR_HERO, -1, 0);
                for (int i = 0; i < b->enemy_count; i++)
                    if (b->enemies[i].hp > 0) axe_hit(g, MSG_SPIN_HIT, i, 0.7f);
            }
        } else if (p->sub == SPELL_BEAM) {
            a->mp -= SPELL_BEAM_COST;
            int t = retarget(b, p->target);
            // Moonlight passes through armor: only half of it counts.
            hurt_enemy(g, MSG_BEAM, ACTOR_GODDESS, t, (float)a->mag, enemy_def(&b->enemies[t]) / 2, 1.0f, false);
        } else {
            a->mp -= SPELL_LIGHT_COST;
            int heal = 8 + 2 * s->level;
            s->hp = min_int(s->max_hp, s->hp + heal);
            a->hp = min_int(a->max_hp, a->hp + heal);
            push(g, MSG_LIGHT, ACTOR_GODDESS, -1, heal);
        }
        break;
    case CMD_DEFEND:
        if (who == 0) s->sp = min_int(s->max_sp, s->sp + 2);
        else a->mp = min_int(a->max_mp, a->mp + 2);
        push(g, MSG_DEFEND, actor, -1, 0);
        break;
    case CMD_ITEMS:
        give_herb(g, actor, p->target);
        break;
    case CMD_FLEE:
        if (!can_flee(g)) {
            push(g, MSG_CANT_FLEE, actor, -1, 0);
        } else if (chance(g, flee_chance(g))) {
            push(g, MSG_FLEE_OK, actor, -1, 0);
            b->outcome = OUTCOME_FLED;
        } else {
            push(g, MSG_FLEE_FAIL, actor, -1, 0);
        }
        break;
    default:
        break;
    }
}

// --- Battle: the monsters ------------------------------------------------------------
//
// On its turn a monster weighs every move it knows against each of the two:
// how much of what is left of them a hit would take, a big bonus for a hit
// that knocks one out (the lumberjack most: that ends the battle), the
// goddess as a target when she can heal, the wolves going for whoever the
// pack is already on, and moves for the moment (a shell when it is hurt and
// has none, a howl when the pack has none, sap when the Elder Stump is low).
// Then some noise, more for the dim ones: a slime is a slime; the Elder
// Stump plays it close to right.

typedef struct {
    int move;
    int target;  // 0 him, 1 her
    float score;
} EnemyPlan;

static int party_hp(const Game *g, int t) { return t == 0 ? g->stats.hp : g->goddess.hp; }
static int party_max_hp(const Game *g, int t) { return t == 0 ? g->stats.max_hp : g->goddess.max_hp; }
static int party_def(const Game *g, int t) { return t == 0 ? g->stats.def : g->goddess.def; }
static bool party_guarding(const Game *g, int t) { return t == 0 ? g->battle.defending : g->battle.ally_defending; }
static bool party_alive(const Game *g, int t) { return t == 0 ? g->stats.hp > 0 : goddess_fights(g); }

static float enemy_atk(const Combatant *e)
{
    return species_info(e->species)->atk * (e->atk_up > 0 ? HOWL_BOOST : 1.0f);
}

// About how much a move would take from one of them.
static float expected_hit(const Game *g, const Combatant *e, int t, float mult, bool pierce)
{
    float dmg = enemy_atk(e) * mult - (pierce ? 0 : party_def(g, t));
    if (dmg < 1) dmg = 1;
    if (party_guarding(g, t)) dmg = ceilf(dmg / 2);
    return dmg;
}

static float target_value(const Game *g, const Combatant *e, int t, float dmg)
{
    int hp = party_hp(g, t);
    if (hp <= 0) return -1000;
    float v = 60.0f * fminf(1.0f, dmg / hp) + 25.0f * dmg / party_max_hp(g, t);
    if (dmg >= hp) v += t == 0 ? 90 : 45;  // knock one out: him above all
    if (t == 1 && g->goddess.mp >= SPELL_LIGHT_COST) v += 10;  // the healer
    if (e->species == SPECIES_WOLF && g->battle.focus == t) v += 18;  // the pack hunts together
    return v;
}

static void consider(EnemyPlan *best, int move, int target, float score, Game *g, int wits, int last)
{
    if (move == last && move != MOVE_ATTACK) score -= wits * 0.3f;  // clever ones mix it up
    score += (random_unit(g) - 0.5f) * (100 - wits) * 0.9f;
    if (score > best->score) {
        best->move = move;
        best->target = target;
        best->score = score;
    }
}

static EnemyPlan enemy_think(Game *g, int i)
{
    Battle *b = &g->battle;
    Combatant *e = &b->enemies[i];
    const SpeciesInfo *info = species_info(e->species);
    int wits = info->wits;
    float hp_frac = (float)e->hp / info->max_hp;
    EnemyPlan best = { MOVE_ATTACK, 0, -1e9f };
    int allies = alive_enemies(b) - 1;

    for (int t = 0; t < 2; t++) {
        if (!party_alive(g, t)) continue;
        consider(&best, MOVE_ATTACK, t, target_value(g, e, t, expected_hit(g, e, t, 1.0f, false)), g, wits, e->last_special);
        switch (e->species) {
        case SPECIES_EMBER:
            if (e->cooldown == 0)
                consider(&best, MOVE_FLAME, t, target_value(g, e, t, expected_hit(g, e, t, FLAME_MULT, true)) + (t == 1 ? 8 : 0), g, wits, e->last_special);
            break;
        case SPECIES_BEETLE:
            if (e->cooldown == 0)
                consider(&best, MOVE_HORN, t, 0.75f * target_value(g, e, t, expected_hit(g, e, t, HORN_MULT, false)), g, wits, e->last_special);
            break;
        case SPECIES_WOLF:
            if (e->cooldown == 0)
                consider(&best, MOVE_BITE, t, target_value(g, e, t, expected_hit(g, e, t, BITE_MULT, false)), g, wits, e->last_special);
            break;
        case SPECIES_ELDER_STUMP:
            if (e->cooldown == 0)
                consider(&best, MOVE_SLAM, t, target_value(g, e, t, expected_hit(g, e, t, SLAM_MULT, false)), g, wits, e->last_special);
            break;
        default:
            break;
        }
    }
    // Moves for the moment.
    switch (e->species) {
    case SPECIES_BEETLE:
        if (e->def_up == 0) consider(&best, MOVE_SHELL, 0, 22 + (hp_frac < 0.7f ? 25 : 0) + g->stats.atk * 0.8f, g, wits, e->last_special);
        break;
    case SPECIES_WOLF: {
        bool howled = false;
        for (int k = 0; k < b->enemy_count; k++) howled |= b->enemies[k].hp > 0 && b->enemies[k].atk_up > 0;
        if (!howled) consider(&best, MOVE_HOWL, 0, allies > 0 ? 38 + 12 * allies : (b->turn <= 1 ? 25 : 0), g, wits, e->last_special);
        break;
    }
    case SPECIES_ELDER_STUMP:
        if (e->cooldown == 0 && party_alive(g, 0) && party_alive(g, 1)) {
            float v = 0.85f * (target_value(g, e, 0, expected_hit(g, e, 0, ROOTS_MULT, false)) +
                               target_value(g, e, 1, expected_hit(g, e, 1, ROOTS_MULT, false))) + 15;
            consider(&best, MOVE_ROOTS, 0, v, g, wits, e->last_special);
        } else if (e->cooldown == 0 && party_alive(g, 0)) {
            consider(&best, MOVE_ROOTS, 0, target_value(g, e, 0, expected_hit(g, e, 0, ROOTS_MULT, false)) + 15, g, wits, e->last_special);
        }
        if (hp_frac < 0.4f && e->saps < SAP_HEALS)
            consider(&best, MOVE_SAP, 0, 80 + (0.4f - hp_frac) * 150, g, wits, e->last_special);
        break;
    default:
        break;
    }
    // Guard when badly hurt, if someone else can carry on meanwhile.
    if (hp_frac < 0.3f && allies > 0 && e->species != SPECIES_ELDER_STUMP)
        consider(&best, MOVE_DEFEND, 0, 10 + (0.3f - hp_frac) * 120, g, wits, e->last_special);
    return best;
}

// Damage from a monster to one of the two, with their guard.
static void hurt_party(Game *g, int i, int t, float mult, bool pierce, int move)
{
    Battle *b = &g->battle;
    Combatant *e = &b->enemies[i];
    int dmg = roll_damage(g, enemy_atk(e), pierce ? 0 : party_def(g, t), mult, NULL);
    if (party_guarding(g, t)) dmg = (dmg + 1) / 2;
    if (t == 1) g->goddess.hp = g->goddess.hp > dmg ? g->goddess.hp - dmg : 0;
    else g->stats.hp = g->stats.hp > dmg ? g->stats.hp - dmg : 0;
    BattleMsg *m = push(g, MSG_ENEMY_ATTACK, i, -1, dmg);
    m->on_ally = t == 1;
    m->move = move;
    if (t == 1 && g->goddess.hp == 0) push(g, MSG_ALLY_DOWN, i, -1, 0);
}

static void enemy_turn(Game *g, int i)
{
    Battle *b = &g->battle;
    Combatant *e = &b->enemies[i];
    e->guarding = false;  // a guard lasts until its next turn
    EnemyPlan p = enemy_think(g, i);
    BattleMsg *m;
    switch (p.move) {
    case MOVE_DEFEND:
        e->guarding = true;
        push(g, MSG_ENEMY_DEFEND, i, -1, 0);
        break;
    case MOVE_SHELL:
        e->def_up = BUFF_ROUNDS;
        m = push(g, MSG_ENEMY_BUFF, i, -1, 0);
        m->move = MOVE_SHELL;
        break;
    case MOVE_HOWL:
        for (int k = 0; k < b->enemy_count; k++)
            if (b->enemies[k].hp > 0 && b->enemies[k].species == SPECIES_WOLF) b->enemies[k].atk_up = BUFF_ROUNDS;
        m = push(g, MSG_ENEMY_BUFF, i, -1, 0);
        m->move = MOVE_HOWL;
        break;
    case MOVE_SAP: {
        int heal = species_info(e->species)->max_hp * SAP_PERCENT / 100;
        e->hp = min_int(species_info(e->species)->max_hp, e->hp + heal);
        e->saps++;
        push(g, MSG_ENEMY_HEAL, i, i, heal)->move = MOVE_SAP;
        break;
    }
    case MOVE_HORN:
        e->cooldown = SPECIAL_COOLDOWN;
        if (!chance(g, 75)) {
            m = push(g, MSG_ENEMY_MISS, i, -1, 0);
            m->move = MOVE_HORN;
            m->on_ally = p.target == 1;
            break;
        }
        hurt_party(g, i, p.target, HORN_MULT, false, MOVE_HORN);
        break;
    case MOVE_BITE:
        e->cooldown = SPECIAL_COOLDOWN;
        hurt_party(g, i, p.target, BITE_MULT, false, MOVE_BITE);
        break;
    case MOVE_FLAME:
        e->cooldown = SPECIAL_COOLDOWN;
        hurt_party(g, i, p.target, FLAME_MULT, true, MOVE_FLAME);
        break;
    case MOVE_SLAM:
        e->cooldown = SPECIAL_COOLDOWN;
        hurt_party(g, i, p.target, SLAM_MULT, false, MOVE_SLAM);
        break;
    case MOVE_ROOTS: {
        e->cooldown = SPECIAL_COOLDOWN;
        int held = -1;
        for (int t = 0; t < 2; t++) {
            if (!party_alive(g, t)) continue;
            hurt_party(g, i, t, ROOTS_MULT, false, MOVE_ROOTS);
            if (held < 0 && party_alive(g, t) && chance(g, HOLD_CHANCE)) held = t;
        }
        if (held >= 0 && g->stats.hp > 0) {
            b->held[held] = 1;
            push(g, MSG_HELD, i, -1, 0)->on_ally = held == 1;
        }
        break;
    }
    default:
        hurt_party(g, i, p.target, 1.0f, false, MOVE_ATTACK);
        break;
    }
    if (p.move != MOVE_ATTACK) e->last_special = p.move;
    if (p.target >= 0 && p.target < 2) b->focus = p.target;
}

// --- Battle: rounds --------------------------------------------------------------------

// Rolls the turn order for the next round: speed, give or take a fifth.
static void roll_order(Game *g)
{
    Battle *b = &g->battle;
    int ids[MAX_ACTORS];
    float init[MAX_ACTORS];
    int n = 0;
    ids[n] = ACTOR_HERO;
    init[n++] = hero_speed(g->stats.level) * (0.8f + 0.4f * random_unit(g));
    if (goddess_fights(g)) {
        ids[n] = ACTOR_GODDESS;
        init[n++] = goddess_speed(g->stats.level) * (0.8f + 0.4f * random_unit(g));
    }
    for (int i = 0; i < b->enemy_count; i++) {
        if (b->enemies[i].hp <= 0) continue;
        ids[n] = i;
        init[n++] = species_info(b->enemies[i].species)->spd * (0.8f + 0.4f * random_unit(g));
    }
    for (int a = 1; a < n; a++)  // fastest first
        for (int k = a; k > 0 && init[k] > init[k - 1]; k--) {
            float tf = init[k]; init[k] = init[k - 1]; init[k - 1] = tf;
            int ti = ids[k]; ids[k] = ids[k - 1]; ids[k - 1] = ti;
        }
    memcpy(b->order, ids, sizeof ids);
    b->order_count = n;
}

// The first of the two who can choose, or -1.
static int next_chooser(const Game *g, int from)
{
    const Battle *b = &g->battle;
    for (int who = from; who < 2; who++) {
        if (who == 1 && !goddess_fights(g)) continue;
        if (b->held[who] > 0) continue;
        return who;
    }
    return -1;
}

static void resolve_round(Game *g);

static void start_choosing(Game *g, int who)
{
    Battle *b = &g->battle;
    b->choosing = who;
    b->phase = PHASE_COMMAND;
    b->cursor = b->plan[who].command >= 0 && b->plan[who].command < CMD_COUNT ? b->plan[who].command : CMD_ATTACK;
    if (!battle_command_enabled(g, (Command)b->cursor)) b->cursor = CMD_ATTACK;
}

static void new_round(Game *g)
{
    Battle *b = &g->battle;
    roll_order(g);
    for (int who = 0; who < 2; who++)
        if (b->held[who] > 0) b->plan[who].command = CMD_BOUND;
    int who = next_chooser(g, 0);
    if (who < 0) resolve_round(g);
    else start_choosing(g, who);
}

// Plays the round: everyone in the turn order.
static void resolve_round(Game *g)
{
    Battle *b = &g->battle;
    Stats *s = &g->stats;
    Ally *a = &g->goddess;
    b->msg_count = b->msg_index = 0;
    b->defending = b->plan[0].command == CMD_DEFEND;  // a guard holds from the start of the round
    b->ally_defending = goddess_fights(g) && b->plan[1].command == CMD_DEFEND;
    b->focus = -1;
    for (int slot = 0; slot < b->order_count && b->outcome == OUTCOME_NONE; slot++) {
        int who = b->order[slot];
        b->acting = slot;
        if (who == ACTOR_HERO) party_act(g, 0);
        else if (who == ACTOR_GODDESS) { if (goddess_fights(g)) party_act(g, 1); }
        else if (b->enemies[who].hp > 0) enemy_turn(g, who);
        if (alive_enemies(b) == 0) finish_victory(g);
        else if (s->hp == 0) {
            b->acting = -1;
            push(g, MSG_PLAYER_DOWN, ACTOR_HERO, -1, 0);
            b->outcome = OUTCOME_LOST;
        }
    }
    b->acting = -1;
    if (b->outcome == OUTCOME_NONE) {
        // Between rounds: breath and magic come back a little; spells and
        // shells wear off.
        if (s->sp < s->max_sp) s->sp++;
        if (goddess_fights(g) && a->mp < a->max_mp) a->mp++;
        for (int i = 0; i < b->enemy_count; i++) {
            Combatant *e = &b->enemies[i];
            if (e->def_up > 0) e->def_up--;
            if (e->atk_up > 0) e->atk_up--;
            if (e->cooldown > 0) e->cooldown--;
        }
    }
    b->turn++;
    b->phase = PHASE_MESSAGES;
    b->msg_timer = 0;
}

// --- Battle: menus -----------------------------------------------------------------------

int battle_herbs_left(const Game *g)
{
    const Battle *b = &g->battle;
    int n = g->stats.herbs;
    if (b->choosing == 1 && b->plan[0].command == CMD_ITEMS) n--;  // he already promised one
    return n;
}

static bool someone_hurt(const Game *g)
{
    return g->stats.hp < g->stats.max_hp || (goddess_fights(g) && g->goddess.hp < g->goddess.max_hp);
}

bool battle_command_enabled(const Game *g, Command c)
{
    const Battle *b = &g->battle;
    switch (c) {
    case CMD_SPECIAL:
        return b->choosing == 0 ? g->stats.sp >= SKILL_SPLIT_COST : g->goddess.mp >= SPELL_BEAM_COST;
    case CMD_ITEMS: return battle_herbs_left(g) > 0 && someone_hurt(g);
    case CMD_FLEE: return can_flee(g);
    default: return c >= 0 && c < CMD_COUNT;
    }
}

int battle_special_count(const Game *g)
{
    return g->battle.choosing == 0 ? SKILL_MENU_COUNT : SPELL_MENU_COUNT;
}

bool battle_special_enabled(const Game *g, int entry)
{
    if (g->battle.choosing == 0) return entry == SKILL_BACK || g->stats.sp >= skill_cost((SkillItem)entry);
    return entry == SPELL_BACK || g->goddess.mp >= spell_cost((SpellItem)entry);
}

bool battle_item_enabled(const Game *g, ItemEntry e)
{
    return e == ITEM_BACK || (battle_herbs_left(g) > 0 && someone_hurt(g));
}

const BattleMsg *battle_current_msg(const Game *g)
{
    const Battle *b = &g->battle;
    if (b->phase != PHASE_MESSAGES || b->msg_index >= b->msg_count) return NULL;
    return &b->msgs[b->msg_index];
}

void game_start_battle(Game *g, int foe)
{
    Battle *b = &g->battle;
    memset(b, 0, sizeof *b);
    b->foe = foe;
    b->acting = -1;
    b->focus = -1;
    Species party[MAX_BATTLE_ENEMIES];
    b->enemy_count = encounter_party(g->foes[foe].kind, party);
    for (int i = 0; i < b->enemy_count; i++) {
        b->enemies[i].species = party[i];
        b->enemies[i].hp = b->enemies[i].shown_hp = species_info(party[i])->max_hp;
        b->enemies[i].shown_alive = true;
    }
    b->ally_in = g->goddess.joined && g->goddess.hp > 0;
    b->shown_hp = g->stats.hp;
    b->shown_sp = g->stats.sp;
    b->ally_shown_hp = g->goddess.hp;
    b->ally_shown_mp = g->goddess.mp;
    push(g, MSG_START, -1, -1, g->foes[foe].kind);
    b->phase = PHASE_MESSAGES;
    g->state = STATE_BATTLE;
    g->events |= EVENT_ENCOUNTER;
}

// Shows the effects of the message that starts playing now.
static void play_msg(Game *g, const BattleMsg *m)
{
    Battle *b = &g->battle;
    if (m->player_hp_after < b->shown_hp) { g->events |= EVENT_HURT; b->player_flash = 0.4f; }
    if (m->player_hp_after > b->shown_hp) g->events |= EVENT_HEAL;
    if (m->ally_hp_after < b->ally_shown_hp) { g->events |= EVENT_HURT; b->ally_flash = 0.4f; }
    if (m->ally_hp_after > b->ally_shown_hp) g->events |= EVENT_HEAL;
    b->shown_hp = m->player_hp_after;
    b->shown_sp = m->player_sp_after;
    b->ally_shown_hp = m->ally_hp_after;
    b->ally_shown_mp = m->ally_mp_after;
    if (m->target >= 0 && m->target < b->enemy_count) {
        Combatant *e = &b->enemies[m->target];
        if (m->enemy_hp_after < e->shown_hp) {
            g->events |= EVENT_HIT;
            if (m->actor == ACTOR_HERO && (m->type == MSG_ATTACK || m->type == MSG_SPLIT || m->type == MSG_SPIN_HIT))
                g->events |= EVENT_AXE;
            if (m->type == MSG_BEAM) g->events |= EVENT_MOONBEAM;
            e->flash = 0.4f;
        }
        if (m->enemy_hp_after > e->shown_hp) g->events |= EVENT_HEAL;
        e->shown_hp = m->enemy_hp_after;
        if (m->type == MSG_ENEMY_DOWN) { e->shown_alive = false; g->events |= EVENT_DEFEATED; }
    }
    if (m->type == MSG_ENEMY_MISS || m->type == MSG_CANT_FLEE) g->events |= EVENT_DENIED;
    if (m->type == MSG_VICTORY) g->events |= EVENT_VICTORY;
    if (m->type == MSG_LEVEL_UP) g->events |= EVENT_LEVEL_UP;
    if (m->type == MSG_FLEE_OK) g->events |= EVENT_FLEE;
    if (m->type == MSG_PLAYER_DOWN) g->events |= EVENT_PLAYER_DOWN;
}

// She joins: the waiting goddess on the map becomes the one following him.
static void goddess_joins(Game *g)
{
    g->goddess_saved = true;
    g->goddess.joined = true;
    goddess_scale(g, true);
    reset_follower(g);
    for (int y = 0; y < g->h; y++)
        for (int x = 0; x < g->w; x++)
            if (g->tiles[y][x] == TILE_GODDESS) {
                g->tiles[y][x] = TILE_FLOOR;
                g->follower.x = g->follower.to_x = x * TILE + (TILE - WALKER_SIZE) / 2.0f;
                g->follower.y = g->follower.to_y = y * TILE + (TILE - WALKER_SIZE) / 2.0f;
            }
}

static void end_battle(Game *g)
{
    Battle *b = &g->battle;
    if (g->goddess.joined && g->goddess.hp == 0) g->goddess.hp = 1;  // she gets back up
    switch (b->outcome) {
    case OUTCOME_WON:
        g->foes[b->foe].alive = false;
        if (g->foes[b->foe].kind == ENCOUNTER_BOSS) {
            g->state = STATE_WON;
            g->state_timer = MENU_DELAY;
            return;
        }
        g->walker.safe_timer = 0.5f;
        g->state = STATE_MAP;
        if (g->foes[b->foe].kind == ENCOUNTER_EMBERS && !g->goddess.joined) {
            // The three around her are one group: they all fall back.
            for (int i = 0; i < g->foe_count; i++)
                if (g->foes[i].kind == ENCOUNTER_EMBERS) g->foes[i].alive = false;
            g->goddess_saved = true;  // she gets up and joins when the scene ends
            start_scene(g, SCENE_GODDESS, STATE_MAP);
        }
        break;
    case OUTCOME_FLED:
        g->walker.safe_timer = SAFE_TIME;
        g->state = STATE_MAP;
        break;
    case OUTCOME_LOST:
        g->state = STATE_GAME_OVER;
        g->state_timer = MENU_DELAY;
        break;
    default:
        break;
    }
}

// The one choosing has decided: the other chooses next, or the round plays.
static void plan_done(Game *g, int command, int sub, int target)
{
    Battle *b = &g->battle;
    int who = b->choosing;
    b->plan[who] = (Plan){ command, sub, target };
    g->events |= EVENT_CONFIRM;
    int next = command == CMD_FLEE ? -1 : next_chooser(g, who + 1);
    if (command == CMD_FLEE && who == 0 && goddess_fights(g) && b->held[1] == 0)
        b->plan[1] = (Plan){ CMD_DEFEND, 0, 0 };  // she covers the escape
    if (next >= 0) start_choosing(g, next);
    else resolve_round(g);
}

// Asks for a monster when there is a choice.
static void ask_target(Game *g, int command, int sub)
{
    Battle *b = &g->battle;
    b->plan[b->choosing].command = command;
    b->plan[b->choosing].sub = sub;
    if (alive_enemies(b) > 1) {
        b->target = retarget(b, b->target);
        b->phase = PHASE_TARGET;
        g->events |= EVENT_CONFIRM;
    } else {
        plan_done(g, command, sub, first_alive(b));
    }
}

static int wrap(int v, int n) { return ((v % n) + n) % n; }

static bool menu_input(Game *g, const GameInput *in, int *cursor, int count)
{
    if (in->nav_y) { *cursor = wrap(*cursor + in->nav_y, count); g->events |= EVENT_CURSOR; }
    bool choose = in->confirm_pressed;
    if (in->pick >= 1 && in->pick <= count) { *cursor = in->pick - 1; choose = true; }
    return choose;
}

static void update_battle(Game *g, const GameInput *in, float dt)
{
    Battle *b = &g->battle;
    for (int i = 0; i < b->enemy_count; i++)
        if (b->enemies[i].flash > 0) b->enemies[i].flash -= dt;
    if (b->player_flash > 0) b->player_flash -= dt;
    if (b->ally_flash > 0) b->ally_flash -= dt;

    switch (b->phase) {
    case PHASE_MESSAGES: {
        if (b->msg_timer == 0 && b->msg_index < b->msg_count) play_msg(g, &b->msgs[b->msg_index]);
        b->msg_timer += dt;
        bool skip = (in->confirm_pressed || in->pick || in->target_pick || in->ally_pick) && b->msg_timer > MSG_MIN_TIME;
        if (b->msg_timer >= MSG_TIME || skip || b->msg_index >= b->msg_count) {
            b->msg_index++;
            b->msg_timer = 0;
            if (b->msg_index >= b->msg_count) {
                b->shown_hp = g->stats.hp;
                b->shown_sp = g->stats.sp;
                b->ally_shown_hp = g->goddess.hp;
                b->ally_shown_mp = g->goddess.mp;
                if (b->outcome != OUTCOME_NONE) end_battle(g);
                else new_round(g);
            }
        }
        break;
    }

    case PHASE_COMMAND: {
        if (in->cancel_pressed && b->choosing == 1 && next_chooser(g, 0) == 0) {
            start_choosing(g, 0);  // he changes his mind
            g->events |= EVENT_CURSOR;
            break;
        }
        if (!menu_input(g, in, &b->cursor, CMD_COUNT)) break;
        if (!battle_command_enabled(g, (Command)b->cursor)) { g->events |= EVENT_DENIED; break; }
        switch (b->cursor) {
        case CMD_ATTACK: ask_target(g, CMD_ATTACK, 0); break;
        case CMD_SPECIAL: b->phase = PHASE_SPECIAL; b->sub_cursor = 0; g->events |= EVENT_CONFIRM; break;
        case CMD_ITEMS: b->phase = PHASE_ITEMS; b->sub_cursor = 0; g->events |= EVENT_CONFIRM; break;
        default: plan_done(g, b->cursor, 0, 0); break;
        }
        break;
    }

    case PHASE_SPECIAL: {
        int count = battle_special_count(g), back = count - 1;
        bool choose = menu_input(g, in, &b->sub_cursor, count);
        if (in->cancel_pressed || (choose && b->sub_cursor == back)) { b->phase = PHASE_COMMAND; break; }
        if (!choose) break;
        if (!battle_special_enabled(g, b->sub_cursor)) { g->events |= EVENT_DENIED; break; }
        bool aimed = b->choosing == 0 ? b->sub_cursor == SKILL_SPLIT : b->sub_cursor == SPELL_BEAM;
        if (aimed) ask_target(g, CMD_SPECIAL, b->sub_cursor);
        else plan_done(g, CMD_SPECIAL, b->sub_cursor, 0);
        break;
    }

    case PHASE_ITEMS: {
        bool choose = menu_input(g, in, &b->sub_cursor, ITEM_MENU_COUNT);
        if (in->cancel_pressed || (choose && b->sub_cursor == ITEM_BACK)) { b->phase = PHASE_COMMAND; break; }
        if (!choose) break;
        if (!battle_item_enabled(g, (ItemEntry)b->sub_cursor)) { g->events |= EVENT_DENIED; break; }
        b->plan[b->choosing].command = CMD_ITEMS;
        b->plan[b->choosing].sub = b->sub_cursor;
        if (goddess_fights(g)) {
            // For whom: the one with less health left, to start with.
            float his = (float)g->stats.hp / g->stats.max_hp, hers = (float)g->goddess.hp / g->goddess.max_hp;
            b->target = hers < his ? 1 : 0;
            b->phase = PHASE_ALLY_TARGET;
            g->events |= EVENT_CONFIRM;
        } else {
            plan_done(g, CMD_ITEMS, b->sub_cursor, 0);
        }
        break;
    }

    case PHASE_ALLY_TARGET: {
        if (in->nav_x || in->nav_y) { b->target = 1 - b->target; g->events |= EVENT_CURSOR; }
        bool choose = in->confirm_pressed;
        if (in->ally_pick == 1 || in->ally_pick == 2) { b->target = in->ally_pick - 1; choose = true; }
        if (in->cancel_pressed) { b->phase = PHASE_ITEMS; break; }
        if (choose) plan_done(g, CMD_ITEMS, b->plan[b->choosing].sub, b->target);
        break;
    }

    case PHASE_TARGET: {
        int step = in->nav_x ? in->nav_x : in->nav_y;
        if (step) {
            // Move to the next monster still standing.
            for (int k = 0; k < b->enemy_count; k++) {
                b->target = wrap(b->target + step, b->enemy_count);
                if (b->enemies[b->target].hp > 0) break;
            }
            g->events |= EVENT_CURSOR;
        }
        bool choose = in->confirm_pressed;
        int t = in->target_pick - 1;
        if (t >= 0 && t < b->enemy_count && b->enemies[t].hp > 0) { b->target = t; choose = true; }
        if (in->cancel_pressed) {
            b->phase = b->plan[b->choosing].command == CMD_SPECIAL ? PHASE_SPECIAL : PHASE_COMMAND;
            break;
        }
        if (choose) plan_done(g, b->plan[b->choosing].command, b->plan[b->choosing].sub, b->target);
        break;
    }
    }
}

// --- Flow -------------------------------------------------------------------------

static void save_area(Game *g)
{
    if (g->area < 0 || g->area >= MAX_AREAS) return;
    g->saved[g->area].visited = true;
    memcpy(g->saved[g->area].tiles, g->tiles, sizeof g->tiles);
    memcpy(g->saved[g->area].foes, g->foes, sizeof g->foes);
    g->saved[g->area].foe_count = g->foe_count;
}

void game_enter_area(Game *g, int area, int link)
{
    save_area(g);
    if (area < 0 || area >= area_count() || area >= MAX_AREAS) area = 0;
    int count = 0;
    const char *const *rows = area_rows(area, &count);
    area_load_rows(g, rows, count);
    if (g->saved[area].visited) {
        memcpy(g->tiles, g->saved[area].tiles, sizeof g->tiles);
        memcpy(g->foes, g->saved[area].foes, sizeof g->foes);
        g->foe_count = g->saved[area].foe_count;
    }
    g->area = area;

    // Where he arrives: next to the exit he came through, or at the start.
    if (link == 0 && g->start_x < 0) {
        for (int y = 0; y < g->h && !link; y++)
            for (int x = 0; x < g->w && !link; x++) link = g->links[y][x];
    }
    float x = g->start_x, y = g->start_y;
    int face_x = 0, face_y = 1;
    if (link > 0) {
        static const int dirs[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
        bool found = false;
        for (int ty = 0; ty < g->h && !found; ty++)
            for (int tx = 0; tx < g->w && !found; tx++) {
                if (g->links[ty][tx] != link) continue;
                for (int d = 0; d < 4 && !found; d++) {
                    int nx = tx + dirs[d][0], ny = ty + dirs[d][1];
                    TileType t = game_tile_at(g, nx, ny);
                    if (game_tile_blocks(t) || t == TILE_EXIT) continue;
                    x = nx * TILE + (TILE - WALKER_SIZE) / 2.0f;
                    y = ny * TILE + (TILE - WALKER_SIZE) / 2.0f;
                    face_x = dirs[d][0];
                    face_y = dirs[d][1];
                    found = true;
                }
            }
    }
    memset(&g->walker, 0, sizeof g->walker);
    g->walker.x = x;
    g->walker.y = y;
    g->walker.face_x = face_x;
    g->walker.face_y = face_y;
    g->walker.safe_timer = 1.0f;
    reset_follower(g);
    for (int i = 0; i < g->foe_count; i++) foe_pick_direction(g, &g->foes[i]);
    g->state = STATE_MAP;
    g->state_timer = 0;
    show_toast(g, TOAST_MAP, area + 1);
    update_camera(g);
    if (area == AREA_WOODS && !g->heard_cry) {
        g->heard_cry = true;
        start_scene(g, SCENE_WOODS, STATE_MAP);
    }
}

void game_place_walker(Game *g, int tx, int ty, int face_x, int face_y)
{
    memset(&g->walker, 0, sizeof g->walker);
    g->walker.x = tx * TILE + (TILE - WALKER_SIZE) / 2.0f;
    g->walker.y = ty * TILE + (TILE - WALKER_SIZE) / 2.0f;
    g->walker.face_x = face_x;
    g->walker.face_y = face_y;
    g->walker.safe_timer = 1.0f;
    reset_follower(g);
    update_camera(g);
}

void game_new(Game *g)
{
    memset(&g->stats, 0, sizeof g->stats);
    memset(&g->goddess, 0, sizeof g->goddess);
    memset(g->saved, 0, sizeof g->saved);
    g->goddess_saved = false;
    g->heard_cry = false;
    g->creatures_met = false;
    g->elder_woke = false;
    g->save_slot = 0;
    g->stats.level = 1;
    g->stats.hp = g->stats.max_hp = 40;
    g->stats.sp = g->stats.max_sp = 8;
    g->stats.atk = 9;
    g->stats.def = 3;
    g->stats.herbs = 1;
    g->rng = 0x2545f491u;
    g->area = -1;
    game_enter_area(g, AREA_HOME, 0);
    g->respawn_area = AREA_HOME;
    g->respawn_x = g->walker.x;
    g->respawn_y = g->walker.y;
}

void game_init(Game *g)
{
    memset(g, 0, sizeof *g);
    game_new(g);  // the home is shown behind the title
    g->toast = TOAST_NONE;
    g->state = STATE_TITLE;
    g->state_timer = MENU_DELAY;
}

static bool menu_pressed(const Game *g, const GameInput *in)
{
    return g->state_timer <= 0 && (in->confirm_pressed || in->pause_pressed || in->pick || in->target_pick);
}

// The title menu: New game, Load game, Options, Quit. The load page lists
// the saves (the front end reads them; the rules only know how many), with
// Back as the last entry. The options page only sends events: the language
// and the sound belong to the front end.
static void title_step(Game *g, const GameInput *in)
{
    if (g->state_timer > 0) return;
    int count = g->title_page == PAGE_OPTIONS ? OPTION_COUNT
              : g->title_page == PAGE_LOAD ? g->save_count + 1 : TITLE_COUNT;
    if (in->nav_y) {
        g->title_cursor = wrap(g->title_cursor + in->nav_y, count);
        g->events |= EVENT_CURSOR;
    }
    bool choose = in->confirm_pressed;
    if (in->pick >= 1 && in->pick <= count) {
        g->title_cursor = in->pick - 1;
        choose = true;
    }
    if (g->title_page != PAGE_MAIN) {
        int back = g->title_page == PAGE_OPTIONS ? OPTION_BACK : g->save_count;
        if (in->cancel_pressed || (choose && g->title_cursor == back)) {
            g->title_cursor = g->title_page == PAGE_OPTIONS ? TITLE_OPTIONS : TITLE_LOAD;
            g->title_page = PAGE_MAIN;
            g->events |= EVENT_CURSOR;
        } else if (g->title_page == PAGE_LOAD) {
            if (choose) g->events |= EVENT_LOAD | EVENT_CONFIRM;
        } else if ((choose || in->nav_x) && g->title_cursor == OPTION_LANGUAGE) {
            g->events |= EVENT_LANGUAGE | EVENT_CURSOR;
        } else if ((choose || in->nav_x) && g->title_cursor == OPTION_SOUND) {
            g->events |= EVENT_SOUND | EVENT_CURSOR;
        }
        return;
    }
    if (!choose) return;
    switch (g->title_cursor) {
    case TITLE_NEW:
        game_new(g);
        start_scene(g, SCENE_INTRO, STATE_MAP);
        g->events |= EVENT_CONFIRM | EVENT_SAVE;  // a new save, at home
        break;
    case TITLE_LOAD:
        if (g->save_count == 0) { g->events |= EVENT_DENIED; break; }
        g->title_page = PAGE_LOAD;
        g->title_cursor = 0;
        g->events |= EVENT_CONFIRM;
        break;
    case TITLE_OPTIONS:
        g->title_page = PAGE_OPTIONS;
        g->title_cursor = OPTION_LANGUAGE;
        g->events |= EVENT_CONFIRM;
        break;
    default:
        g->events |= EVENT_QUIT;
        break;
    }
}

void game_loaded(Game *g)
{
    g->state = STATE_MAP;
    g->state_timer = MENU_DELAY;
    g->title_page = PAGE_MAIN;
    g->title_cursor = 0;
    g->events = 0;
    memset(&g->battle, 0, sizeof g->battle);
    if (g->area >= 0) show_toast(g, TOAST_MAP, g->area + 1);
}

void game_step(Game *g, const GameInput *in)
{
    const float dt = STEP_DT;
    g->events = 0;
    if (g->state != STATE_PAUSED) g->time += dt;
    if (g->state_timer > 0) g->state_timer -= dt;
    if (g->toast_timer > 0 && g->state == STATE_MAP) g->toast_timer -= dt;

    switch (g->state) {
    case STATE_TITLE:
        title_step(g, in);
        break;

    case STATE_SCENE:
        if (g->state_timer > 0) break;
        if (in->cancel_pressed) {
            g->scene_page = scene_page_count(g->scene);  // skip the rest
        } else if (in->confirm_pressed || in->pause_pressed || in->pick || in->target_pick) {
            g->scene_page++;
            g->state_timer = PAGE_DELAY;
            g->events |= EVENT_CURSOR;
        }
        if (g->scene_page >= scene_page_count(g->scene)) {
            g->state_timer = 0;
            g->events |= EVENT_CONFIRM;
            if (g->scene_return == STATE_BATTLE) game_start_battle(g, g->scene_foe);
            else g->state = g->scene_return;
            if (g->scene == SCENE_GODDESS && !g->goddess.joined) goddess_joins(g);
        }
        break;

    case STATE_MAP:
        if (in->pause_pressed) { g->state = STATE_PAUSED; break; }
        update_walker(g, in, dt);
        if (g->state == STATE_MAP) update_foes(g, dt);
        update_camera(g);
        break;

    case STATE_PAUSED:
        // Any of the buttons resumes: the screen says Enter or a tap.
        if (in->pause_pressed || in->cancel_pressed || in->confirm_pressed || in->pick || in->target_pick)
            g->state = STATE_MAP;
        break;

    case STATE_BATTLE:
        update_battle(g, in, dt);
        break;

    case STATE_GAME_OVER:
        if (menu_pressed(g, in)) {
            // Wakes up where he last rested, rested. Nothing else is lost.
            if (g->area != g->respawn_area) game_enter_area(g, g->respawn_area, 0);
            g->stats.hp = g->stats.max_hp;
            g->stats.sp = g->stats.max_sp;
            g->goddess.hp = g->goddess.max_hp;
            g->goddess.mp = g->goddess.max_mp;
            g->walker.x = g->respawn_x;
            g->walker.y = g->respawn_y;
            g->walker.moving = false;
            g->walker.safe_timer = SAFE_TIME;
            reset_follower(g);
            g->state = STATE_MAP;
            update_camera(g);
        }
        break;

    case STATE_WON:
        if (menu_pressed(g, in)) game_init(g);
        break;
    }
}
