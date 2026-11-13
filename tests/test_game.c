// Tests for the game rules (game.c, levels.c). They need no window or GPU.
#include "game.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            printf("  FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            failures++;                                                 \
        }                                                               \
    } while (0)

#define COUNT(a) (int)(sizeof(a) / sizeof((a)[0]))

static Game g;
static const GameInput NONE = { 0 };
static const GameInput RIGHT = { .move_x = 1 };
static const GameInput SWING = { .attack_pressed = true };
static const GameInput START = { .start_pressed = true };

static void load(const char *const *rows, int count)
{
    memset(&g, 0, sizeof g);
    CHECK(level_load_rows(&g, rows, count));
    g.player.x = g.start_x;
    g.player.y = g.start_y;
    g.player.face_y = 1;
    g.player.health = MAX_HEALTH;
    g.state = STATE_PLAYING;
}

static unsigned run(const GameInput *in, int steps)
{
    unsigned events = 0;
    for (int i = 0; i < steps; i++) {
        game_step(&g, in);
        events |= g.events;
    }
    return events;
}

static void freeze_enemies(void)
{
    for (int i = 0; i < g.enemy_count; i++) {
        g.enemies[i].vx = g.enemies[i].vy = 0;
        g.enemies[i].think = 100;
    }
}

// --- Maps -------------------------------------------------------------------

// Walks the map from the start like a player would: through everything but
// walls, trees, water and thorns, opening a door whenever a key is in hand.
// Returns false and names what cannot be reached.
static bool map_is_completable(const Game *m)
{
    static bool seen[MAX_MAP_H][MAX_MAP_W], open[MAX_MAP_H][MAX_MAP_W];
    static int queue[MAX_MAP_H * MAX_MAP_W];
    static const int dirs[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
    memset(open, 0, sizeof open);
    int opened = 0;
    for (;;) {
        memset(seen, 0, sizeof seen);
        int sx = (int)(m->start_x / TILE), sy = (int)(m->start_y / TILE);
        int head = 0, tail = 0, keys = 0, door = -1;
        queue[tail++] = sy * MAX_MAP_W + sx;
        seen[sy][sx] = true;
        while (head < tail) {
            int x = queue[head] % MAX_MAP_W, y = queue[head] / MAX_MAP_W;
            head++;
            if (game_tile_at(m, x, y) == TILE_KEY) keys++;
            for (int i = 0; i < 4; i++) {
                int nx = x + dirs[i][0], ny = y + dirs[i][1];
                if (nx < 0 || ny < 0 || nx >= m->w || ny >= m->h || seen[ny][nx]) continue;
                TileType t = game_tile_at(m, nx, ny);
                if (t == TILE_DOOR) {
                    if (!open[ny][nx]) {
                        if (door < 0) door = ny * MAX_MAP_W + nx;
                        continue;
                    }
                } else if (game_tile_blocks(t) || t == TILE_THORNS) {
                    continue;
                }
                seen[ny][nx] = true;
                queue[tail++] = ny * MAX_MAP_W + nx;
            }
        }
        if (door < 0 || keys <= opened) break;
        open[door / MAX_MAP_W][door % MAX_MAP_W] = true;
        opened++;
    }
    bool ok = true;
    for (int y = 0; y < m->h; y++) {
        for (int x = 0; x < m->w; x++) {
            TileType t = game_tile_at(m, x, y);
            if ((t == TILE_SEED || t == TILE_KEY || t == TILE_PORTAL) && !seen[y][x]) {
                printf("  tile %d at (%d, %d) cannot be reached\n", t, x, y);
                ok = false;
            }
        }
    }
    return ok;
}

static void test_maps_are_valid(void)
{
    CHECK(level_count() >= 3);
    for (int i = 0; i < level_count(); i++) {
        int count = 0;
        const char *const *rows = level_rows(i, &count);
        memset(&g, 0, sizeof g);
        CHECK(level_load_rows(&g, rows, count));
        CHECK(g.level_seeds > 0);
        CHECK(g.enemy_count > 0);
        for (int r = 0; r < count; r++) {
            CHECK((int)strlen(rows[r]) == g.w);
            CHECK(rows[r][0] == '#' && rows[r][g.w - 1] == '#');
        }
        for (int x = 0; x < g.w; x++) CHECK(rows[0][x] == '#' && rows[count - 1][x] == '#');
        CHECK(map_is_completable(&g));
    }
}

static void test_bad_map_is_rejected(void)
{
    static const char *const no_portal[] = { "#####", "#P..#", "#####" };
    memset(&g, 0, sizeof g);
    CHECK(!level_load_rows(&g, no_portal, 3));
}

static void test_enemies_stay_on_open_ground(void)
{
    for (int level = 0; level < level_count(); level++) {
        game_init(&g);
        game_start_level(&g, level);
        g.player.hurt_timer = 1e9f;  // nobody gets hurt in this test
        run(&NONE, 60 * 30);
        for (int i = 0; i < g.enemy_count; i++) {
            const Enemy *e = &g.enemies[i];
            for (int c = 0; c < 4; c++) {
                float x = e->x + (c & 1 ? ENEMY_SIZE - 0.1f : 0), y = e->y + (c & 2 ? ENEMY_SIZE - 0.1f : 0);
                TileType t = game_tile_at(&g, (int)floorf(x / TILE), (int)floorf(y / TILE));
                CHECK(!game_tile_blocks(t) && t != TILE_THORNS);
            }
        }
    }
}

// --- Movement ---------------------------------------------------------------

static const char *const room[] = {
    "##########",
    "#........#",
    "#.P......#",
    "#........#",
    "#.......X#",
    "##########",
};

static void test_walls_stop_the_player(void)
{
    load(room, COUNT(room));
    run(&RIGHT, 300);
    CHECK(fabsf(g.player.x + PLAYER_SIZE - 9 * TILE) < 0.01f);
    GameInput up = { .move_y = -1 };
    run(&up, 300);
    CHECK(fabsf(g.player.y - TILE) < 0.01f);
}

static void test_diagonal_is_not_faster(void)
{
    load(room, COUNT(room));
    float x0 = g.player.x, y0 = g.player.y;
    GameInput diag = { .move_x = 1, .move_y = 1 };
    run(&diag, 10);
    float dx = g.player.x - x0, dy = g.player.y - y0;
    float straight = 80.0f * 10 * STEP_DT;
    CHECK(fabsf(sqrtf(dx * dx + dy * dy) - straight) < 0.1f);
    CHECK(g.player.face_x == 1 && g.player.face_y == 1);
}

static void test_pause(void)
{
    load(room, COUNT(room));
    run(&START, 1);
    CHECK(g.state == STATE_PAUSED);
    float x = g.player.x;
    run(&RIGHT, 30);
    CHECK(g.player.x == x);
    run(&START, 1);
    CHECK(g.state == STATE_PLAYING);
}

// --- Items ------------------------------------------------------------------

static void test_seeds_open_the_portal(void)
{
    static const char *const rows[] = { "#######", "#P.o.X#", "#######" };
    load(rows, COUNT(rows));
    CHECK(!game_portal_open(&g));
    unsigned events = 0;
    for (int i = 0; i < 300 && g.state == STATE_PLAYING; i++) events |= run(&RIGHT, 1);
    CHECK(events & EVENT_SEED);
    CHECK(events & EVENT_PORTAL);
    CHECK(events & EVENT_EXIT);
    CHECK(g.state == STATE_LEVEL_DONE);
}

static void test_closed_portal_does_nothing(void)
{
    static const char *const rows[] = { "#######", "#P..X.#", "#..o..#", "#######" };
    load(rows, COUNT(rows));
    run(&RIGHT, 300);
    CHECK(g.state == STATE_PLAYING);
}

static void test_key_opens_door(void)
{
    static const char *const rows[] = { "########", "#Pk.D.X#", "########" };
    load(rows, COUNT(rows));
    unsigned events = run(&RIGHT, 300);
    CHECK(events & EVENT_KEY);
    CHECK(events & EVENT_DOOR);
    CHECK(g.keys == 0);
    CHECK(game_tile_at(&g, 4, 1) == TILE_FLOOR);
}

static void test_door_without_key_blocks(void)
{
    static const char *const rows[] = { "########", "#P..D.X#", "########" };
    load(rows, COUNT(rows));
    run(&RIGHT, 300);
    CHECK(fabsf(g.player.x + PLAYER_SIZE - 4 * TILE) < 0.01f);
}

static void test_heart_heals(void)
{
    static const char *const rows[] = { "#######", "#P.h.X#", "#######" };
    load(rows, COUNT(rows));
    g.player.health = 1;
    CHECK(run(&RIGHT, 40) & EVENT_HEART);
    CHECK(g.player.health == 2);
}

static void test_full_health_leaves_heart(void)
{
    static const char *const rows[] = { "#######", "#P.h.X#", "#######" };
    load(rows, COUNT(rows));
    run(&RIGHT, 40);
    CHECK(game_tile_at(&g, 3, 1) == TILE_HEART);
}

// --- Damage -----------------------------------------------------------------

static void test_thorns_hurt(void)
{
    static const char *const rows[] = { "#######", "#P.^.X#", "#######" };
    load(rows, COUNT(rows));
    CHECK(run(&RIGHT, 30) & EVENT_HURT);
    CHECK(g.player.health == MAX_HEALTH - 1);
    CHECK(g.player.hurt_timer > 0);  // safe for a moment
}

static void test_last_heart_means_game_over_and_retry(void)
{
    static const char *const rows[] = { "#########", "#P....b.#", "#......X#", "#########" };
    load(rows, COUNT(rows));
    g.level = 0;
    g.player.health = 1;
    freeze_enemies();
    run(&RIGHT, 120);
    CHECK(g.state == STATE_DYING || g.state == STATE_GAME_OVER);
    run(&NONE, 120);
    CHECK(g.state == STATE_GAME_OVER);
    run(&START, 1);
    CHECK(g.state == STATE_PLAYING);
    CHECK(g.level == 0);
    CHECK(g.player.health == MAX_HEALTH);
}

// --- Fighting ---------------------------------------------------------------

static const char *const arena[] = {
    "#########",
    "#Pb.....#",
    "#......X#",
    "#########",
};

static void face_right(void)
{
    g.player.face_x = 1;
    g.player.face_y = 0;
}

static void test_swing_defeats_beetle(void)
{
    load(arena, COUNT(arena));
    freeze_enemies();
    face_right();
    unsigned events = run(&SWING, 1) | run(&NONE, 20);
    CHECK(events & EVENT_SWING);
    CHECK(events & EVENT_DEFEAT);
    CHECK(!g.enemies[0].alive);
    CHECK(g.player.health == MAX_HEALTH);
}

static void test_slime_takes_two_swings(void)
{
    static const char *const rows[] = { "#########", "#Ps.....#", "#......X#", "#########" };
    load(rows, COUNT(rows));
    face_right();
    float before = g.enemies[0].x;
    unsigned events = run(&SWING, 1) | run(&NONE, 5);
    CHECK(events & EVENT_HIT);
    CHECK(g.enemies[0].alive);  // one swing hits only once
    CHECK(g.enemies[0].health == 1);
    CHECK(g.enemies[0].x > before + 3);  // knocked back
}

static void test_missed_swing_does_nothing(void)
{
    load(arena, COUNT(arena));
    freeze_enemies();
    g.player.face_x = -1;
    g.player.face_y = 0;
    CHECK(!(run(&SWING, 1) & EVENT_HIT));
    CHECK(g.enemies[0].alive);
}

static void test_slime_follows_player(void)
{
    static const char *const rows[] = { "##########", "#P....s..#", "#.......X#", "##########" };
    load(rows, COUNT(rows));
    float before = g.enemies[0].x;
    run(&NONE, 30);
    CHECK(g.enemies[0].x < before - 10);
}

static void test_touching_enemy_hurts(void)
{
    load(arena, COUNT(arena));
    freeze_enemies();
    unsigned events = run(&RIGHT, 30);
    CHECK(events & EVENT_HURT);
    CHECK(g.player.health == MAX_HEALTH - 1);
}

// --- Flow -------------------------------------------------------------------

static void test_title_starts_game(void)
{
    game_init(&g);
    CHECK(g.state == STATE_TITLE);
    run(&START, 1);
    CHECK(g.state == STATE_TITLE);  // too soon after the screen appeared
    run(&NONE, 30);
    run(&START, 1);
    CHECK(g.state == STATE_PLAYING);
    CHECK(g.level == 0 && g.player.health == MAX_HEALTH);
}

static void test_portal_leads_to_next_map(void)
{
    static const char *const rows[] = { "######", "#P.X.#", "######" };
    load(rows, COUNT(rows));
    g.level = 0;
    run(&RIGHT, 60);
    CHECK(g.state == STATE_LEVEL_DONE);
    run(&NONE, 120);
    CHECK(g.state == STATE_PLAYING);
    CHECK(g.level == 1);
}

static void test_last_map_wins(void)
{
    static const char *const rows[] = { "######", "#P.X.#", "######" };
    load(rows, COUNT(rows));
    g.level = level_count() - 1;
    run(&RIGHT, 60);
    run(&NONE, 120);
    CHECK(g.state == STATE_WON);
}

int main(void)
{
    struct {
        const char *name;
        void (*fn)(void);
    } tests[] = {
        { "maps are valid and completable", test_maps_are_valid },
        { "bad map is rejected", test_bad_map_is_rejected },
        { "enemies stay on open ground", test_enemies_stay_on_open_ground },
        { "walls stop the player", test_walls_stop_the_player },
        { "diagonal is not faster", test_diagonal_is_not_faster },
        { "pause", test_pause },
        { "seeds open the portal", test_seeds_open_the_portal },
        { "closed portal does nothing", test_closed_portal_does_nothing },
        { "key opens door", test_key_opens_door },
        { "door without key blocks", test_door_without_key_blocks },
        { "heart heals", test_heart_heals },
        { "full health leaves heart", test_full_health_leaves_heart },
        { "thorns hurt", test_thorns_hurt },
        { "last heart: game over and retry", test_last_heart_means_game_over_and_retry },
        { "swing defeats beetle", test_swing_defeats_beetle },
        { "slime takes two swings", test_slime_takes_two_swings },
        { "missed swing does nothing", test_missed_swing_does_nothing },
        { "slime follows player", test_slime_follows_player },
        { "touching enemy hurts", test_touching_enemy_hurts },
        { "title starts game", test_title_starts_game },
        { "portal leads to next map", test_portal_leads_to_next_map },
        { "last map wins", test_last_map_wins },
    };
    int count = COUNT(tests);
    for (int i = 0; i < count; i++) {
        int before = failures;
        tests[i].fn();
        printf("%s %s\n", failures == before ? "ok  " : "FAIL", tests[i].name);
    }
    printf("%d test(s), %d failure(s)\n", count, failures);
    return failures == 0 ? 0 : 1;
}
