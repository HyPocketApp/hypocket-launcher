#include "game.h"

#include <math.h>
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

// --- Tables -------------------------------------------------------------------

static const SpeciesInfo species_table[SPECIES_COUNT] = {
    [SPECIES_SLIME] = { .max_hp = 14, .atk = 6, .def = 1, .xp = 5, .herb_drop = 20 },
    [SPECIES_BEETLE] = { .max_hp = 22, .atk = 8, .def = 4, .xp = 8, .herb_drop = 25 },
    [SPECIES_WOLF] = { .max_hp = 26, .atk = 11, .def = 2, .xp = 12, .herb_drop = 30 },
    [SPECIES_EMBER] = { .max_hp = 12, .atk = 6, .def = 0, .xp = 6, .herb_drop = 50 },
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
    case ENCOUNTER_EMBERS: out[0] = out[1] = SPECIES_EMBER; return 2;
    default: return 0;
    }
}

int scene_page_count(Scene s)
{
    static const int pages[SCENE_COUNT] = { [SCENE_INTRO] = 3, [SCENE_GODDESS] = 4, [SCENE_GODDESS_TALK] = 1 };
    return (s >= 0 && s < SCENE_COUNT) ? pages[s] : 0;
}

// Encounters that stay where they are on the map.
static bool encounter_stays(Encounter e) { return e == ENCOUNTER_BOSS || e == ENCOUNTER_EMBERS; }

int xp_to_next_level(int level) { return level * 15; }

int skill_cost(SkillItem s) { return s == SKILL_SPLIT ? SKILL_SPLIT_COST : s == SKILL_SPIN ? SKILL_SPIN_COST : 0; }

// --- Helpers ------------------------------------------------------------------

TileType game_tile_at(const Game *g, int tx, int ty)
{
    if (tx < 0 || ty < 0 || tx >= g->w || ty >= g->h) return TILE_WALL;
    return (TileType)g->tiles[ty][tx];
}

bool game_tile_blocks(TileType t)
{
    return t == TILE_WALL || t == TILE_TREE || t == TILE_ROCK || t == TILE_WATER || t == TILE_CHEST ||
           t == TILE_CHEST_AXE || t == TILE_CHEST_OPEN || t == TILE_FIRE || t == TILE_GODDESS;
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

bool level_load_rows(Game *g, const char *const *rows, int row_count)
{
    if (row_count <= 0 || row_count > MAX_MAP_H) return false;

    memset(g->tiles, 0, sizeof g->tiles);
    g->h = row_count;
    g->w = 0;
    g->foe_count = 0;
    bool has_start = false, has_end = false;

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
            case 'X': t = TILE_EXIT; has_end = true; break;
            case 'M': t = TILE_GODDESS; break;
            case 'P':
                g->start_x = tx * TILE + (TILE - WALKER_SIZE) / 2.0f;
                g->start_y = ty * TILE + (TILE - WALKER_SIZE) / 2.0f;
                has_start = true;
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
    return has_start && has_end;
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
    case TILE_GODDESS:
        if (g->goddess_saved) start_scene(g, SCENE_GODDESS_TALK, STATE_MAP);
        break;
    case TILE_FIRE:
        g->stats.hp = g->stats.max_hp;
        g->stats.sp = g->stats.max_sp;
        g->respawn_x = w->x;
        g->respawn_y = w->y;
        g->events |= EVENT_REST;
        show_toast(g, TOAST_REST, 0);
        break;
    default:
        break;
    }
}

static void update_walker(Game *g, const GameInput *in, float dt)
{
    Walker *w = &g->walker;
    float mx = in->move_x, my = in->move_y, len = sqrtf(mx * mx + my * my);
    if (len > 1.0f) { mx /= len; my /= len; len = 1.0f; }
    if (len > 0.2f) {
        if (fabsf(mx) > fabsf(my)) { w->face_x = mx > 0 ? 1 : -1; w->face_y = 0; }
        else { w->face_x = 0; w->face_y = my > 0 ? 1 : -1; }
        w->walk_anim += len * WALK_SPEED * dt;
    } else {
        w->walk_anim = 0;
    }
    if (w->safe_timer > 0) w->safe_timer -= dt;
    move_box(g, &w->x, &w->y, mx * WALK_SPEED * dt, my * WALK_SPEED * dt, game_tile_blocks);

    if (in->confirm_pressed) map_action(g);

    int cx = tile_floor(w->x + WALKER_SIZE / 2.0f), cy = tile_floor(w->y + WALKER_SIZE / 2.0f);
    if (game_tile_at(g, cx, cy) == TILE_EXIT) {
        g->events |= EVENT_EXIT;
        game_enter_level(g, g->level + 1 < level_count() ? g->level + 1 : g->level);
    }
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
            game_start_battle(g, i);
            return;
        }
    }
}

// --- Battle: resolving a turn -------------------------------------------------

static void push_msg(Game *g, MsgType type, int actor, int target, int amount, bool crit)
{
    Battle *b = &g->battle;
    if (b->msg_count >= MAX_BATTLE_MSGS) return;
    BattleMsg *m = &b->msgs[b->msg_count++];
    m->type = type;
    m->actor = actor;
    m->target = target;
    m->amount = amount;
    m->crit = crit;
    m->enemy_hp_after = (target >= 0 && target < b->enemy_count) ? b->enemies[target].hp : 0;
    m->player_hp_after = g->stats.hp;
    m->player_sp_after = g->stats.sp;
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
        push_msg(g, MSG_LEVEL_UP, -1, -1, s->level, false);
    }
    b->outcome = OUTCOME_WON;
}

static void enemies_turn(Game *g)
{
    Battle *b = &g->battle;
    Stats *s = &g->stats;
    for (int i = 0; i < b->enemy_count && s->hp > 0; i++) {
        Combatant *e = &b->enemies[i];
        if (e->hp <= 0) continue;
        const SpeciesInfo *info = species_info(e->species);
        float mult = 1.0f;
        MsgType type = MSG_ENEMY_ATTACK;
        if (e->species == SPECIES_WOLF && chance(g, 30)) {
            mult = 1.4f;  // fierce bite
            type = MSG_ENEMY_SPECIAL;
        } else if (e->species == SPECIES_ELDER_STUMP && b->turn % 3 == 2) {
            mult = 1.7f;  // grasping roots, every third turn
            type = MSG_ENEMY_SPECIAL;
        }
        int dmg = roll_damage(g, info->atk, s->def, mult, NULL);
        if (b->defending) dmg = (dmg + 1) / 2;
        s->hp = s->hp > dmg ? s->hp - dmg : 0;
        push_msg(g, type, i, -1, dmg, false);
    }
    if (s->hp == 0) {
        push_msg(g, MSG_PLAYER_DOWN, -1, -1, 0, false);
        b->outcome = OUTCOME_LOST;
    }
}

// Plays one whole round: the lumberjack's action, then the monsters'.
static void resolve_round(Game *g, int action, int skill, int target)
{
    Battle *b = &g->battle;
    Stats *s = &g->stats;
    b->msg_count = b->msg_index = 0;
    b->defending = false;
    bool enemies_act = true;

    switch (action) {
    case MENU_ATTACK:
        axe_hit(g, MSG_ATTACK, target, 1.0f);
        break;
    case MENU_SKILLS:
        s->sp -= skill_cost((SkillItem)skill);
        if (skill == SKILL_SPLIT) {
            axe_hit(g, MSG_SPLIT, target, 1.5f);
        } else {
            push_msg(g, MSG_SPIN, -1, -1, 0, false);
            for (int i = 0; i < b->enemy_count; i++)
                if (b->enemies[i].hp > 0) axe_hit(g, MSG_SPIN_HIT, i, 0.7f);
        }
        break;
    case MENU_HERB: {
        s->herbs--;
        int heal = s->max_hp - s->hp < HERB_HEAL ? s->max_hp - s->hp : HERB_HEAL;
        s->hp += heal;
        push_msg(g, MSG_HERB, -1, -1, heal, false);
        break;
    }
    case MENU_DEFEND:
        b->defending = true;
        s->sp = s->sp + 2 < s->max_sp ? s->sp + 2 : s->max_sp;
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

    if (alive_enemies(b) == 0) {
        finish_victory(g);
    } else if (enemies_act) {
        enemies_turn(g);
        if (s->sp < s->max_sp) s->sp++;  // catches his breath between rounds
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
    b->shown_hp = g->stats.hp;
    b->shown_sp = g->stats.sp;
    push_msg(g, MSG_START, -1, -1, g->foes[foe].kind, false);
    if (g->foes[foe].kind == ENCOUNTER_EMBERS) push_msg(g, MSG_EMBERS_TAUNT, 0, -1, 0, false);
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
    b->shown_hp = m->player_hp_after;
    b->shown_sp = m->player_sp_after;
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

static void end_battle(Game *g)
{
    Battle *b = &g->battle;
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
        if (g->foes[b->foe].kind == ENCOUNTER_EMBERS) {
            g->goddess_saved = true;
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

static void choose_target_or_resolve(Game *g, int action, int skill)
{
    Battle *b = &g->battle;
    if (alive_enemies(b) > 1) {
        b->pending = action == MENU_ATTACK ? MENU_ATTACK : MENU_SKILLS;
        b->skill_cursor = skill;
        b->target = first_alive(b);
        b->phase = PHASE_TARGET;
    } else {
        resolve_round(g, action, skill, first_alive(b));
    }
}

static int wrap(int v, int n) { return ((v % n) + n) % n; }

static void update_battle(Game *g, const GameInput *in, float dt)
{
    Battle *b = &g->battle;
    for (int i = 0; i < b->enemy_count; i++)
        if (b->enemies[i].flash > 0) b->enemies[i].flash -= dt;
    if (b->player_flash > 0) b->player_flash -= dt;

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
                if (b->outcome != OUTCOME_NONE) end_battle(g);
                else b->phase = PHASE_MENU;
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
        if (b->cursor == MENU_ATTACK) choose_target_or_resolve(g, MENU_ATTACK, 0);
        else if (b->cursor == MENU_SKILLS) { b->phase = PHASE_SKILLS; b->skill_cursor = 0; }
        else resolve_round(g, b->cursor, 0, 0);
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
        if (b->skill_cursor == SKILL_SPLIT) choose_target_or_resolve(g, MENU_SKILLS, SKILL_SPLIT);
        else resolve_round(g, MENU_SKILLS, SKILL_SPIN, 0);
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
        if (in->cancel_pressed) { b->phase = b->pending == MENU_SKILLS ? PHASE_SKILLS : PHASE_MENU; break; }
        if (!choose) break;
        g->events |= EVENT_CONFIRM;
        resolve_round(g, b->pending, b->skill_cursor, b->target);
        break;
    }
    }
}

// --- Flow -------------------------------------------------------------------------

void game_enter_level(Game *g, int index)
{
    int count = 0;
    const char *const *rows = level_rows(index, &count);
    level_load_rows(g, rows, count);
    g->level = index;
    memset(&g->walker, 0, sizeof g->walker);
    g->walker.x = g->respawn_x = g->start_x;
    g->walker.y = g->respawn_y = g->start_y;
    g->walker.face_y = 1;
    g->walker.safe_timer = 1.0f;
    g->rng = 0x2545f491u + (unsigned)index * 7919u;
    for (int i = 0; i < g->foe_count; i++) foe_pick_direction(g, &g->foes[i]);
    g->state = STATE_MAP;
    g->state_timer = 0;
    show_toast(g, TOAST_MAP, index + 1);
    update_camera(g);
}

void game_new(Game *g)
{
    memset(&g->stats, 0, sizeof g->stats);
    g->goddess_saved = false;
    g->stats.level = 1;
    g->stats.hp = g->stats.max_hp = 40;
    g->stats.sp = g->stats.max_sp = 8;
    g->stats.atk = 9;
    g->stats.def = 3;
    g->stats.herbs = 2;
    game_enter_level(g, 0);
}

void game_init(Game *g)
{
    memset(g, 0, sizeof *g);
    game_new(g);  // the first map is shown behind the title
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
            g->state = g->scene_return;
            g->state_timer = 0;
            g->events |= EVENT_CONFIRM;
        }
        break;

    case STATE_MAP:
        if (in->pause_pressed) { g->state = STATE_PAUSED; break; }
        update_walker(g, in, dt);
        if (g->state == STATE_MAP) update_foes(g, dt);
        update_camera(g);
        break;

    case STATE_PAUSED:
        if (in->pause_pressed || in->cancel_pressed) g->state = STATE_MAP;
        break;

    case STATE_BATTLE:
        update_battle(g, in, dt);
        break;

    case STATE_GAME_OVER:
        if (menu_pressed(g, in)) {
            // Wakes up at the last campfire, rested. Nothing else is lost.
            g->stats.hp = g->stats.max_hp;
            g->stats.sp = g->stats.max_sp;
            g->walker.x = g->respawn_x;
            g->walker.y = g->respawn_y;
            g->walker.safe_timer = SAFE_TIME;
            g->state = STATE_MAP;
            update_camera(g);
        }
        break;

    case STATE_WON:
        if (menu_pressed(g, in)) game_init(g);
        break;
    }
}
