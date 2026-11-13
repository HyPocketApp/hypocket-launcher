#include "game.h"

#include <math.h>
#include <string.h>

// Tuning, in pixels and seconds.
#define WALK_SPEED 80.0f
#define BEETLE_SPEED 30.0f
#define SLIME_SPEED 36.0f
#define SLIME_SIGHT 88.0f     // slimes follow the player inside this distance
#define KNOCKBACK 190.0f
#define KNOCK_FADE 10.0f      // how fast knockback slows down
#define HURT_TIME 1.0f
#define ATTACK_COOLDOWN 0.30f
#define ATTACK_REACH 16.0f

#define DYING_TIME 1.2f
#define LEVEL_DONE_TIME 1.6f
#define MENU_DELAY 0.4f  // ignore presses right after a screen appears

// --- Helpers ----------------------------------------------------------------

TileType game_tile_at(const Game *g, int tx, int ty)
{
    if (tx < 0 || ty < 0 || tx >= g->w || ty >= g->h) return TILE_WALL;
    return (TileType)g->tiles[ty][tx];
}

bool game_tile_blocks(TileType t)
{
    return t == TILE_WALL || t == TILE_TREE || t == TILE_WATER || t == TILE_DOOR;
}

static bool enemy_blocked_by(TileType t)
{
    return game_tile_blocks(t) || t == TILE_THORNS || t == TILE_PORTAL;
}

bool game_portal_open(const Game *g) { return g->seeds >= g->level_seeds; }

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

// Moves a square box by (dx, dy), one axis at a time, stopping at tiles for
// which `blocks` is true. Returns true when it bumped into something.
static bool move_box(const Game *g, float *x, float *y, float size, float dx, float dy,
                     bool (*blocks)(TileType))
{
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

static void fade_knockback(float *kx, float *ky, float dt)
{
    float k = expf(-KNOCK_FADE * dt);
    *kx *= k;
    *ky *= k;
    if (fabsf(*kx) < 1.0f) *kx = 0;
    if (fabsf(*ky) < 1.0f) *ky = 0;
}

// Sets (kx, ky) to KNOCKBACK pointing from (fx, fy) to (tx, ty).
static void push_away(float *kx, float *ky, float fx, float fy, float tx, float ty)
{
    float dx = tx - fx, dy = ty - fy, len = sqrtf(dx * dx + dy * dy);
    if (len < 0.001f) { dx = 0; dy = 1; len = 1; }
    *kx = dx / len * KNOCKBACK;
    *ky = dy / len * KNOCKBACK;
}

// --- Loading ----------------------------------------------------------------

bool level_load_rows(Game *g, const char *const *rows, int row_count)
{
    if (row_count <= 0 || row_count > MAX_MAP_H) return false;

    memset(g->tiles, 0, sizeof g->tiles);
    g->h = row_count;
    g->w = 0;
    g->enemy_count = 0;
    g->level_seeds = 0;
    bool has_start = false, has_portal = false;

    for (int ty = 0; ty < row_count; ty++) {
        int len = (int)strlen(rows[ty]);
        if (len > MAX_MAP_W) return false;
        if (len > g->w) g->w = len;
    }
    for (int ty = 0; ty < row_count; ty++) {
        int len = (int)strlen(rows[ty]);
        for (int tx = 0; tx < g->w; tx++) {
            char c = tx < len ? rows[ty][tx] : '#';  // short rows end in wall
            unsigned char t = TILE_FLOOR;
            switch (c) {
            case '#': t = TILE_WALL; break;
            case 'T': t = TILE_TREE; break;
            case '~': t = TILE_WATER; break;
            case '^': t = TILE_THORNS; break;
            case 'o': t = TILE_SEED; g->level_seeds++; break;
            case 'k': t = TILE_KEY; break;
            case 'D': t = TILE_DOOR; break;
            case 'h': t = TILE_HEART; break;
            case 'X': t = TILE_PORTAL; has_portal = true; break;
            case 'P':
                g->start_x = tx * TILE + (TILE - PLAYER_SIZE) / 2.0f;
                g->start_y = ty * TILE + (TILE - PLAYER_SIZE) / 2.0f;
                has_start = true;
                break;
            case 'b':
            case 's':
                if (g->enemy_count < MAX_ENEMIES) {
                    Enemy *e = &g->enemies[g->enemy_count++];
                    memset(e, 0, sizeof *e);
                    e->kind = c == 'b' ? ENEMY_BEETLE : ENEMY_SLIME;
                    e->x = tx * TILE + (TILE - ENEMY_SIZE) / 2.0f;
                    e->y = ty * TILE + (TILE - ENEMY_SIZE) / 2.0f;
                    e->health = c == 'b' ? 1 : 2;
                    e->alive = true;
                }
                break;
            default: break;
            }
            g->tiles[ty][tx] = t;
        }
    }
    return has_start && has_portal;
}

// --- Player -----------------------------------------------------------------

void game_attack_box(const Game *g, float *x, float *y, float *w, float *h)
{
    const Player *p = &g->player;
    float reach = PLAYER_SIZE / 2.0f + ATTACK_REACH / 2.0f - 2.0f;
    float cx = p->x + PLAYER_SIZE / 2.0f + p->face_x * reach;
    float cy = p->y + PLAYER_SIZE / 2.0f + p->face_y * reach;
    *w = *h = ATTACK_REACH + 4.0f;
    *x = cx - *w / 2.0f;
    *y = cy - *h / 2.0f;
}

static void hurt_player(Game *g, float from_x, float from_y)
{
    Player *p = &g->player;
    if (p->hurt_timer > 0 || g->state != STATE_PLAYING) return;
    p->health--;
    p->hurt_timer = HURT_TIME;
    push_away(&p->kx, &p->ky, from_x, from_y, p->x + PLAYER_SIZE / 2.0f, p->y + PLAYER_SIZE / 2.0f);
    g->events |= EVENT_HURT;
    if (p->health <= 0) {
        g->state = STATE_DYING;
        g->state_timer = DYING_TIME;
    }
}

static void open_doors_near_player(Game *g)
{
    Player *p = &g->player;
    int l = tile_floor(p->x - 2), r = tile_floor(p->x + PLAYER_SIZE + 2);
    int t = tile_floor(p->y - 2), b = tile_floor(p->y + PLAYER_SIZE + 2);
    for (int ty = t; ty <= b && g->keys > 0; ty++) {
        for (int tx = l; tx <= r && g->keys > 0; tx++) {
            if (game_tile_at(g, tx, ty) == TILE_DOOR) {
                g->tiles[ty][tx] = TILE_FLOOR;
                g->keys--;
                g->events |= EVENT_DOOR;
            }
        }
    }
}

static void update_player(Game *g, const GameInput *in, float dt)
{
    Player *p = &g->player;
    float mx = in->move_x, my = in->move_y, len = sqrtf(mx * mx + my * my);
    if (len > 1.0f) { mx /= len; my /= len; len = 1.0f; }

    if (len > 0.2f) {
        // Face one of 8 directions.
        float ax = fabsf(mx) / len, ay = fabsf(my) / len;
        p->face_x = ax > 0.38f ? copysignf(1.0f, mx) : 0.0f;
        p->face_y = ay > 0.38f ? copysignf(1.0f, my) : 0.0f;
        p->walk_anim += len * WALK_SPEED * dt;
    } else {
        p->walk_anim = 0;
    }

    if (p->hurt_timer > 0) p->hurt_timer -= dt;
    if (p->attack_timer > 0) p->attack_timer -= dt;
    if (p->attack_cooldown > 0) p->attack_cooldown -= dt;
    if (in->attack_pressed && p->attack_cooldown <= 0) {
        p->attack_timer = ATTACK_TIME;
        p->attack_cooldown = ATTACK_COOLDOWN;
        g->swing++;
        g->events |= EVENT_SWING;
    }

    if (g->keys > 0) open_doors_near_player(g);

    float dx = (mx * WALK_SPEED + p->kx) * dt, dy = (my * WALK_SPEED + p->ky) * dt;
    move_box(g, &p->x, &p->y, PLAYER_SIZE, dx, dy, game_tile_blocks);
    fade_knockback(&p->kx, &p->ky, dt);

    // Tiles under the player.
    float cx = p->x + PLAYER_SIZE / 2.0f, cy = p->y + PLAYER_SIZE / 2.0f;
    int l = tile_floor(p->x), r = tile_floor(p->x + PLAYER_SIZE - 0.01f);
    int t = tile_floor(p->y), b = tile_floor(p->y + PLAYER_SIZE - 0.01f);
    for (int ty = t; ty <= b; ty++) {
        for (int tx = l; tx <= r; tx++) {
            switch (game_tile_at(g, tx, ty)) {
            case TILE_SEED:
                g->tiles[ty][tx] = TILE_FLOOR;
                g->seeds++;
                g->events |= EVENT_SEED;
                if (game_portal_open(g)) g->events |= EVENT_PORTAL;
                break;
            case TILE_KEY:
                g->tiles[ty][tx] = TILE_FLOOR;
                g->keys++;
                g->events |= EVENT_KEY;
                break;
            case TILE_HEART:
                if (p->health < MAX_HEALTH) {
                    g->tiles[ty][tx] = TILE_FLOOR;
                    p->health++;
                    g->events |= EVENT_HEART;
                }
                break;
            case TILE_THORNS:
                // Only the middle of the bush hurts, with a little mercy.
                if (overlaps(p->x, p->y, PLAYER_SIZE, PLAYER_SIZE, tx * TILE + 3.0f, ty * TILE + 3.0f,
                             TILE - 6.0f, TILE - 6.0f))
                    hurt_player(g, tx * TILE + TILE / 2.0f, ty * TILE + TILE / 2.0f);
                break;
            case TILE_PORTAL:
                if (game_portal_open(g) && tile_floor(cx) == tx && tile_floor(cy) == ty &&
                    g->state == STATE_PLAYING) {
                    g->state = STATE_LEVEL_DONE;
                    g->state_timer = LEVEL_DONE_TIME;
                    g->events |= EVENT_EXIT;
                }
                break;
            default:
                break;
            }
        }
    }
}

// --- Enemies ----------------------------------------------------------------

static void beetle_pick_direction(Game *g, Enemy *e)
{
    static const float dirs[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
    int d = (int)(next_random(g) % 4u);
    e->vx = dirs[d][0] * BEETLE_SPEED;
    e->vy = dirs[d][1] * BEETLE_SPEED;
    e->think = 1.0f + 2.0f * random_unit(g);
}

static void update_enemies(Game *g, float dt)
{
    Player *p = &g->player;
    float px = p->x + PLAYER_SIZE / 2.0f, py = p->y + PLAYER_SIZE / 2.0f;
    float ax = 0, ay = 0, aw = 0, ah = 0;
    bool attacking = p->attack_timer > 0 && g->state == STATE_PLAYING;
    if (attacking) game_attack_box(g, &ax, &ay, &aw, &ah);

    for (int i = 0; i < g->enemy_count; i++) {
        Enemy *e = &g->enemies[i];
        if (!e->alive) continue;
        if (e->hit_flash > 0) e->hit_flash -= dt;
        float ex = e->x + ENEMY_SIZE / 2.0f, ey = e->y + ENEMY_SIZE / 2.0f;

        bool stunned = fabsf(e->kx) + fabsf(e->ky) > 20.0f;
        if (!stunned) {
            if (e->kind == ENEMY_BEETLE) {
                e->think -= dt;
                if (e->think <= 0 || (e->vx == 0 && e->vy == 0)) beetle_pick_direction(g, e);
            } else {
                float dx = px - ex, dy = py - ey, d = sqrtf(dx * dx + dy * dy);
                if (d < SLIME_SIGHT && d > 1.0f) {
                    e->vx = dx / d * SLIME_SPEED;
                    e->vy = dy / d * SLIME_SPEED;
                } else {
                    e->vx = e->vy = 0;
                }
            }
        }

        float mvx = stunned ? 0 : e->vx, mvy = stunned ? 0 : e->vy;
        bool bumped = move_box(g, &e->x, &e->y, ENEMY_SIZE, (mvx + e->kx) * dt, (mvy + e->ky) * dt,
                               enemy_blocked_by);
        if (bumped && e->kind == ENEMY_BEETLE && !stunned) beetle_pick_direction(g, e);
        fade_knockback(&e->kx, &e->ky, dt);

        if (attacking && e->hit_swing != g->swing &&
            overlaps(ax, ay, aw, ah, e->x, e->y, ENEMY_SIZE, ENEMY_SIZE)) {
            e->hit_swing = g->swing;
            e->health--;
            e->hit_flash = 0.15f;
            push_away(&e->kx, &e->ky, px, py, ex, ey);
            g->events |= EVENT_HIT;
            if (e->health <= 0) {
                e->alive = false;
                g->events |= EVENT_DEFEAT;
            }
            continue;
        }

        if (overlaps(p->x, p->y, PLAYER_SIZE, PLAYER_SIZE, e->x, e->y, ENEMY_SIZE, ENEMY_SIZE))
            hurt_player(g, ex, ey);
    }
}

// --- Flow -------------------------------------------------------------------

static void update_camera(Game *g)
{
    const Player *p = &g->player;
    float cx = p->x + PLAYER_SIZE / 2.0f - VIEW_W / 2.0f;
    float cy = p->y + PLAYER_SIZE / 2.0f - VIEW_H / 2.0f;
    float max_x = (float)(g->w * TILE - VIEW_W), max_y = (float)(g->h * TILE - VIEW_H);
    g->camera_x = max_x <= 0 ? max_x / 2.0f : fmaxf(0.0f, fminf(cx, max_x));
    g->camera_y = max_y <= 0 ? max_y / 2.0f : fmaxf(0.0f, fminf(cy, max_y));
}

void game_init(Game *g)
{
    memset(g, 0, sizeof *g);
    game_start_level(g, 0);  // shown behind the title
    g->state = STATE_TITLE;
    g->state_timer = MENU_DELAY;
}

void game_start_level(Game *g, int index)
{
    int count = 0;
    const char *const *rows = level_rows(index, &count);
    level_load_rows(g, rows, count);
    g->level = index;
    g->seeds = 0;
    g->keys = 0;
    g->time = 0;
    g->rng = 0x2545f491u + (unsigned)index * 7919u;
    memset(&g->player, 0, sizeof g->player);
    g->player.x = g->start_x;
    g->player.y = g->start_y;
    g->player.face_y = 1;
    g->player.health = MAX_HEALTH;
    for (int i = 0; i < g->enemy_count; i++)
        if (g->enemies[i].kind == ENEMY_BEETLE) beetle_pick_direction(g, &g->enemies[i]);
    g->state = STATE_PLAYING;
    g->state_timer = 0;
    update_camera(g);
}

static bool menu_pressed(const Game *g, const GameInput *in)
{
    return g->state_timer <= 0 && (in->start_pressed || in->attack_pressed);
}

void game_step(Game *g, const GameInput *in)
{
    const float dt = STEP_DT;
    g->events = 0;
    if (g->state != STATE_PAUSED) g->time += dt;  // animations stand still in the pause
    if (g->state_timer > 0) g->state_timer -= dt;

    switch (g->state) {
    case STATE_TITLE:
        if (menu_pressed(g, in)) {
            g->total_seeds = 0;
            game_start_level(g, 0);
        }
        break;

    case STATE_PLAYING:
        if (in->start_pressed) { g->state = STATE_PAUSED; break; }
        update_player(g, in, dt);
        update_enemies(g, dt);
        update_camera(g);
        break;

    case STATE_PAUSED:
        if (in->start_pressed) g->state = STATE_PLAYING;
        break;

    case STATE_DYING:
        if (g->state_timer <= 0) {
            g->state = STATE_GAME_OVER;
            g->state_timer = MENU_DELAY;
        }
        break;

    case STATE_GAME_OVER:
        if (menu_pressed(g, in)) game_start_level(g, g->level);  // try this map again
        break;

    case STATE_LEVEL_DONE:
        if (g->state_timer <= 0) {
            g->total_seeds += g->seeds;
            if (g->level + 1 < level_count()) {
                game_start_level(g, g->level + 1);
            } else {
                g->state = STATE_WON;
                g->state_timer = MENU_DELAY;
            }
        }
        break;

    case STATE_WON:
        if (menu_pressed(g, in)) game_init(g);
        break;
    }
}
