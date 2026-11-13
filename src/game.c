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
#define FLEE_CHANCE 65       // percent
#define ALLY_HIT_CHANCE 35   // percent of monster attacks aimed at the goddess

// --- Tables -------------------------------------------------------------------

static const SpeciesInfo species_table[SPECIES_COUNT] = {
    [SPECIES_SLIME] = { .max_hp = 14, .atk = 6, .def = 1, .xp = 5, .herb_drop = 20 },
    [SPECIES_BEETLE] = { .max_hp = 22, .atk = 8, .def = 4, .xp = 8, .herb_drop = 25 },
    [SPECIES_WOLF] = { .max_hp = 26, .atk = 11, .def = 2, .xp = 12, .herb_drop = 30 },
    [SPECIES_EMBER] = { .max_hp = 9, .atk = 5, .def = 0, .xp = 6, .herb_drop = 40 },
    [SPECIES_ELDER_STUMP] = { .max_hp = 260, .atk = 18, .def = 8, .xp = 100, .wooden = true },
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
           t == TILE_DOOR || t == TILE_WOODPILE;
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
    g->events |= EVENT_REST;
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

// Uses the axe (or hands) on the tile in front of the lumberjack.
static void map_action(Game *g)
{
    Walker *w = &g->walker;
    float cx = w->x + WALKER_SIZE / 2.0f + w->face_x * 11.0f;
    float cy = w->y + WALKER_SIZE / 2.0f + w->face_y * 11.0f;
    int tx = tile_floor(cx), ty = tile_floor(cy);
    switch (game_tile_at(g, tx, ty)) {
    case TILE_TREE:
        g->tiles[ty][tx] = TILE_STUMP;
        g->events |= EVENT_CHOP;
        break;
    case TILE_ROCK:
        g->events |= EVENT_BUMP;
        show_toast(g, TOAST_TOO_HARD, 0);
        break;
    case TILE_CHEST:
        g->tiles[ty][tx] = TILE_CHEST_OPEN;
        g->stats.herbs += 2;
        g->events |= EVENT_CHEST;
        show_toast(g, TOAST_HERBS, 2);
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
// battle. Returns true when it started.
static bool meet_creatures(Game *g)
{
    if (g->creatures_met) return false;
    int wx = tile_floor(g->walker.x + WALKER_SIZE / 2.0f), wy = tile_floor(g->walker.y + WALKER_SIZE / 2.0f);
    for (int i = 0; i < g->foe_count; i++) {
        const Foe *f = &g->foes[i];
        if (!f->alive || f->kind != ENCOUNTER_EMBERS) continue;
        int fx = tile_floor(f->x + WALKER_SIZE / 2.0f), fy = tile_floor(f->y + WALKER_SIZE / 2.0f);
        if (abs(fx - wx) > 3 || abs(fy - wy) > 3) continue;
        g->creatures_met = true;
        g->scene_foe = i;
        start_scene(g, SCENE_CREATURES, STATE_BATTLE);
        return true;
    }
    return false;
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
        if (!game_tile_blocks(game_tile_at(g, tx, ty))) {
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

// --- Battle: resolving a turn -------------------------------------------------

static void push_msg_ex(Game *g, MsgType type, int actor, int target, int amount, bool crit, bool on_ally)
{
    Battle *b = &g->battle;
    if (b->msg_count >= MAX_BATTLE_MSGS) return;
    BattleMsg *m = &b->msgs[b->msg_count++];
    m->type = type;
    m->actor = actor;
    m->target = target;
    m->amount = amount;
    m->crit = crit;
    m->on_ally = on_ally;
    m->enemy_hp_after = (target >= 0 && target < b->enemy_count) ? b->enemies[target].hp : 0;
    m->player_hp_after = g->stats.hp;
    m->player_sp_after = g->stats.sp;
    m->ally_hp_after = g->goddess.hp;
    m->ally_mp_after = g->goddess.mp;
}

static void push_msg(Game *g, MsgType type, int actor, int target, int amount, bool crit)
{
    push_msg_ex(g, type, actor, target, amount, crit, false);
}

static int roll_damage(Game *g, int atk, int def, float mult, bool *crit)
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

// The goddess's strength follows the lumberjack's level.
static void goddess_scale(Game *g, bool refill)
{
    Ally *a = &g->goddess;
    int l = g->stats.level;
    a->max_hp = 24 + 5 * l;
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

// The lumberjack hits one monster and tells what happened.
static void axe_hit(Game *g, MsgType type, int target, float mult)
{
    Battle *b = &g->battle;
    Combatant *e = &b->enemies[target];
    const SpeciesInfo *info = species_info(e->species);
    if (info->wooden) mult *= 1.25f;  // the axe was made for this
    bool crit = false;
    int dmg = roll_damage(g, g->stats.atk, info->def, mult, &crit);
    e->hp = e->hp > dmg ? e->hp - dmg : 0;
    push_msg(g, type, -1, target, dmg, crit);
    if (e->hp == 0) push_msg(g, MSG_ENEMY_DOWN, -1, target, 0, false);
}

static void finish_victory(Game *g)
{
    Battle *b = &g->battle;
    Stats *s = &g->stats;
    int xp = 0, herbs = 0;
    for (int i = 0; i < b->enemy_count; i++) {
        const SpeciesInfo *info = species_info(b->enemies[i].species);
        xp += info->xp;
        if (chance(g, info->herb_drop)) herbs++;
    }
    s->xp += xp;
    push_msg(g, MSG_VICTORY, -1, -1, xp, false);
    if (herbs > 0) {
        s->herbs += herbs;
        push_msg(g, MSG_FOUND_HERB, -1, -1, herbs, false);
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
        push_msg(g, MSG_LEVEL_UP, -1, -1, s->level, false);
    }
    b->outcome = OUTCOME_WON;
}

// The goddess's turn: the spell chosen for this round.
static void ally_act(Game *g)
{
    Battle *b = &g->battle;
    Ally *a = &g->goddess;
    Stats *s = &g->stats;
    switch (b->ally_spell) {
    case SPELL_BEAM: {
        a->mp -= SPELL_BEAM_COST;
        int t = b->enemies[b->ally_target].hp > 0 ? b->ally_target : first_alive(b);
        Combatant *e = &b->enemies[t];
        // Moonlight passes through armor: only half the defense counts.
        int dmg = roll_damage(g, a->mag, species_info(e->species)->def / 2, 1.1f, NULL);
        e->hp = e->hp > dmg ? e->hp - dmg : 0;
        push_msg(g, MSG_BEAM, -1, t, dmg, false);
        if (e->hp == 0) push_msg(g, MSG_ENEMY_DOWN, -1, t, 0, false);
        break;
    }
    case SPELL_LIGHT: {
        a->mp -= SPELL_LIGHT_COST;
        int heal = 14 + 3 * s->level;
        s->hp = min_int(s->max_hp, s->hp + heal);
        a->hp = min_int(a->max_hp, a->hp + heal);
        push_msg(g, MSG_LIGHT, -1, -1, heal, false);
        break;
    }
    case SPELL_DEFEND:
        b->ally_defending = true;
        a->mp = min_int(a->max_mp, a->mp + 2);
        push_msg(g, MSG_ALLY_DEFEND, -1, -1, 0, false);
        break;
    default:
        break;
    }
}

static void enemies_turn(Game *g)
{
    Battle *b = &g->battle;
    Stats *s = &g->stats;
    Ally *a = &g->goddess;
    for (int i = 0; i < b->enemy_count && s->hp > 0; i++) {
        Combatant *e = &b->enemies[i];
        if (e->hp <= 0) continue;
        const SpeciesInfo *info = species_info(e->species);
        bool at_ally = b->ally_in && a->hp > 0 && chance(g, ALLY_HIT_CHANCE);
        float mult = 1.0f;
        MsgType type = MSG_ENEMY_ATTACK;
        if (e->species == SPECIES_WOLF && chance(g, 30)) {
            mult = 1.4f;  // fierce bite
            type = MSG_ENEMY_SPECIAL;
        } else if (e->species == SPECIES_ELDER_STUMP && b->turn % 3 == 2) {
            mult = 1.7f;  // grasping roots, every third turn
            type = MSG_ENEMY_SPECIAL;
        }
        int dmg = roll_damage(g, info->atk, at_ally ? a->def : s->def, mult, NULL);
        if (at_ally ? b->ally_defending : b->defending) dmg = (dmg + 1) / 2;
        if (at_ally) {
            a->hp = a->hp > dmg ? a->hp - dmg : 0;
            push_msg_ex(g, type, i, -1, dmg, false, true);
            if (a->hp == 0) push_msg(g, MSG_ALLY_DOWN, -1, -1, 0, false);
        } else {
            s->hp = s->hp > dmg ? s->hp - dmg : 0;
            push_msg(g, type, i, -1, dmg, false);
        }
    }
    if (s->hp == 0) {
        push_msg(g, MSG_PLAYER_DOWN, -1, -1, 0, false);
        b->outcome = OUTCOME_LOST;
    }
}

// Plays one whole round with the choices made: the lumberjack's action, the
// goddess's spell, then the monsters'.
static void resolve_round(Game *g)
{
    Battle *b = &g->battle;
    Stats *s = &g->stats;
    Ally *a = &g->goddess;
    b->msg_count = b->msg_index = 0;
    b->defending = b->ally_defending = false;
    b->choosing = 0;
    bool enemies_act = true;
    int target = b->hero_target;

    switch (b->hero_action) {
    case MENU_ATTACK:
        axe_hit(g, MSG_ATTACK, target, 1.0f);
        break;
    case MENU_SKILLS:
        s->sp -= skill_cost((SkillItem)b->hero_skill);
        if (b->hero_skill == SKILL_SPLIT) {
            axe_hit(g, MSG_SPLIT, target, 1.5f);
        } else {
            push_msg(g, MSG_SPIN, -1, -1, 0, false);
            for (int i = 0; i < b->enemy_count; i++)
                if (b->enemies[i].hp > 0) axe_hit(g, MSG_SPIN_HIT, i, 0.7f);
        }
        break;
    case MENU_HERB: {
        s->herbs--;
        int heal = min_int(s->max_hp - s->hp, HERB_HEAL);
        s->hp += heal;
        push_msg(g, MSG_HERB, -1, -1, heal, false);
        break;
    }
    case MENU_DEFEND:
        b->defending = true;
        s->sp = min_int(s->max_sp, s->sp + 2);
        push_msg(g, MSG_DEFEND, -1, -1, 0, false);
        break;
    case MENU_FLEE:
        if (g->foes[b->foe].kind == ENCOUNTER_BOSS || g->foes[b->foe].kind == ENCOUNTER_EMBERS) {
            push_msg(g, MSG_CANT_FLEE, -1, -1, 0, false);
        } else if (chance(g, FLEE_CHANCE)) {
            push_msg(g, MSG_FLEE_OK, -1, -1, 0, false);
            b->outcome = OUTCOME_FLED;
            enemies_act = false;
        } else {
            push_msg(g, MSG_FLEE_FAIL, -1, -1, 0, false);
        }
        break;
    }

    if (enemies_act && alive_enemies(b) > 0 && b->ally_in && a->hp > 0) ally_act(g);

    if (alive_enemies(b) == 0) {
        finish_victory(g);
    } else if (enemies_act) {
        enemies_turn(g);
        if (s->sp < s->max_sp) s->sp++;  // catches his breath between rounds
        if (b->ally_in && a->hp > 0 && a->mp < a->max_mp) a->mp++;
    }
    b->turn++;
    b->phase = PHASE_MESSAGES;
    b->msg_timer = 0;
}

// --- Battle: menus and messages ---------------------------------------------------

bool battle_menu_enabled(const Game *g, MenuItem item)
{
    if (item == MENU_HERB) return g->stats.herbs > 0 && g->stats.hp < g->stats.max_hp;
    if (item == MENU_SKILLS) return g->stats.sp >= SKILL_SPLIT_COST;
    return true;
}

bool battle_skill_enabled(const Game *g, SkillItem item)
{
    return item == SKILL_BACK || g->stats.sp >= skill_cost(item);
}

bool battle_spell_enabled(const Game *g, SpellItem item)
{
    return g->goddess.mp >= spell_cost(item);
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
    Species party[MAX_BATTLE_ENEMIES];
    b->enemy_count = encounter_party(g->foes[foe].kind, party);
    for (int i = 0; i < b->enemy_count; i++) {
        b->enemies[i].species = party[i];
        b->enemies[i].hp = b->enemies[i].shown_hp = species_info(party[i])->max_hp;
        b->enemies[i].shown_alive = true;
    }
    b->ally_in = g->goddess.joined && g->goddess.hp > 0;
    b->ally_spell = -1;
    b->shown_hp = g->stats.hp;
    b->shown_sp = g->stats.sp;
    b->ally_shown_hp = g->goddess.hp;
    b->ally_shown_mp = g->goddess.mp;
    push_msg(g, MSG_START, -1, -1, g->foes[foe].kind, false);
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
        if (m->enemy_hp_after < e->shown_hp) { g->events |= EVENT_HIT; e->flash = 0.4f; }
        e->shown_hp = m->enemy_hp_after;
        if (m->type == MSG_ENEMY_DOWN) { e->shown_alive = false; g->events |= EVENT_DEFEATED; }
    }
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
            goddess_joins(g);
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

// The lumberjack's choice is made: the goddess chooses next, if she can.
static void hero_chosen(Game *g, int action, int skill, int target)
{
    Battle *b = &g->battle;
    b->hero_action = action;
    b->hero_skill = skill;
    b->hero_target = target;
    if (action != MENU_FLEE && b->ally_in && g->goddess.hp > 0) {
        b->phase = PHASE_ALLY;
        b->choosing = 1;
    } else {
        b->ally_spell = -1;
        resolve_round(g);
    }
}

static void ally_chosen(Game *g, int spell, int target)
{
    g->battle.ally_spell = spell;
    g->battle.ally_target = target;
    resolve_round(g);
}

// Asks for a target when there is a choice; `who` is 0 for him, 1 for her.
static void choose_target(Game *g, int who, int action, int skill)
{
    Battle *b = &g->battle;
    if (alive_enemies(b) > 1) {
        b->choosing = who;
        b->pending = action;
        b->skill_cursor = skill;
        b->target = first_alive(b);
        b->phase = PHASE_TARGET;
    } else if (who == 0) {
        hero_chosen(g, action, skill, first_alive(b));
    } else {
        ally_chosen(g, SPELL_BEAM, first_alive(b));
    }
}

static int wrap(int v, int n) { return ((v % n) + n) % n; }

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
        bool skip = (in->confirm_pressed || in->pick || in->target_pick) && b->msg_timer > MSG_MIN_TIME;
        if (b->msg_timer >= MSG_TIME || skip) {
            b->msg_index++;
            b->msg_timer = 0;
            if (b->msg_index >= b->msg_count) {
                b->shown_hp = g->stats.hp;
                b->shown_sp = g->stats.sp;
                b->ally_shown_hp = g->goddess.hp;
                b->ally_shown_mp = g->goddess.mp;
                if (b->outcome != OUTCOME_NONE) end_battle(g);
                else { b->phase = PHASE_MENU; b->choosing = 0; }
            }
        }
        break;
    }

    case PHASE_MENU: {
        if (in->nav_y) { b->cursor = wrap(b->cursor + in->nav_y, MENU_COUNT); g->events |= EVENT_CURSOR; }
        bool choose = in->confirm_pressed;
        if (in->pick >= 1 && in->pick <= MENU_COUNT) { b->cursor = in->pick - 1; choose = true; }
        if (!choose) break;
        if (!battle_menu_enabled(g, (MenuItem)b->cursor)) { g->events |= EVENT_DENIED; break; }
        g->events |= EVENT_CONFIRM;
        if (b->cursor == MENU_ATTACK) choose_target(g, 0, MENU_ATTACK, 0);
        else if (b->cursor == MENU_SKILLS) { b->phase = PHASE_SKILLS; b->skill_cursor = 0; }
        else hero_chosen(g, b->cursor, 0, 0);
        break;
    }

    case PHASE_SKILLS: {
        if (in->nav_y) { b->skill_cursor = wrap(b->skill_cursor + in->nav_y, SKILL_MENU_COUNT); g->events |= EVENT_CURSOR; }
        bool choose = in->confirm_pressed;
        if (in->pick >= 1 && in->pick <= SKILL_MENU_COUNT) { b->skill_cursor = in->pick - 1; choose = true; }
        if (in->cancel_pressed || (choose && b->skill_cursor == SKILL_BACK)) { b->phase = PHASE_MENU; break; }
        if (!choose) break;
        if (!battle_skill_enabled(g, (SkillItem)b->skill_cursor)) { g->events |= EVENT_DENIED; break; }
        g->events |= EVENT_CONFIRM;
        if (b->skill_cursor == SKILL_SPLIT) choose_target(g, 0, MENU_SKILLS, SKILL_SPLIT);
        else hero_chosen(g, MENU_SKILLS, SKILL_SPIN, 0);
        break;
    }

    case PHASE_ALLY: {
        if (in->nav_y) { b->ally_cursor = wrap(b->ally_cursor + in->nav_y, SPELL_MENU_COUNT); g->events |= EVENT_CURSOR; }
        bool choose = in->confirm_pressed;
        if (in->pick >= 1 && in->pick <= SPELL_MENU_COUNT) { b->ally_cursor = in->pick - 1; choose = true; }
        if (in->cancel_pressed || (choose && b->ally_cursor == SPELL_BACK)) {
            b->phase = PHASE_MENU;  // the lumberjack chooses again
            b->choosing = 0;
            break;
        }
        if (!choose) break;
        if (!battle_spell_enabled(g, (SpellItem)b->ally_cursor)) { g->events |= EVENT_DENIED; break; }
        g->events |= EVENT_CONFIRM;
        if (b->ally_cursor == SPELL_BEAM) choose_target(g, 1, MENU_ATTACK, 0);
        else ally_chosen(g, b->ally_cursor, 0);
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
            if (b->choosing == 1) b->phase = PHASE_ALLY;
            else b->phase = b->pending == MENU_SKILLS ? PHASE_SKILLS : PHASE_MENU;
            break;
        }
        if (!choose) break;
        g->events |= EVENT_CONFIRM;
        if (b->choosing == 1) ally_chosen(g, SPELL_BEAM, b->target);
        else hero_chosen(g, b->pending, b->skill_cursor, b->target);
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

void game_new(Game *g)
{
    memset(&g->stats, 0, sizeof g->stats);
    memset(&g->goddess, 0, sizeof g->goddess);
    memset(g->saved, 0, sizeof g->saved);
    g->goddess_saved = false;
    g->heard_cry = false;
    g->creatures_met = false;
    g->stats.level = 1;
    g->stats.hp = g->stats.max_hp = 40;
    g->stats.sp = g->stats.max_sp = 8;
    g->stats.atk = 9;
    g->stats.def = 3;
    g->stats.herbs = 2;
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

void game_step(Game *g, const GameInput *in)
{
    const float dt = STEP_DT;
    g->events = 0;
    if (g->state != STATE_PAUSED) g->time += dt;
    if (g->state_timer > 0) g->state_timer -= dt;
    if (g->toast_timer > 0 && g->state == STATE_MAP) g->toast_timer -= dt;

    switch (g->state) {
    case STATE_TITLE:
        if (menu_pressed(g, in)) {
            game_new(g);
            start_scene(g, SCENE_INTRO, STATE_MAP);
            g->events |= EVENT_CONFIRM;
        }
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
