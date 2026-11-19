// Tests for the game rules (game.c, levels.c). They need no window or GPU.
#include "game.h"
#include "save.h"
#include "story.h"

#include <math.h>
#include <stdlib.h>
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
static const GameInput LEFT = { .move_x = -1 };
static const GameInput UP = { .move_y = -1 };
static const GameInput CONFIRM = { .confirm_pressed = true };
static const GameInput CANCEL = { .cancel_pressed = true };
static const GameInput DOWN = { .nav_y = 1 };

// A fresh level-1 lumberjack on a test map.
static void load(const char *const *rows, int count)
{
    memset(&g, 0, sizeof g);
    game_new(&g);  // stats, and map 1 which we replace now
    CHECK(area_load_rows(&g, rows, count));
    g.area = -1;  // not one of the game's areas
    memset(&g.walker, 0, sizeof g.walker);
    g.walker.x = g.respawn_x = g.start_x;
    g.walker.y = g.respawn_y = g.start_y;
    g.walker.face_y = 1;
    g.state = STATE_MAP;
    for (int i = 0; i < g.foe_count; i++) g.foes[i].think = 1000;  // keep them still
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

// Lets battle messages play until a menu is waiting or the battle is over.
static unsigned play_messages(void)
{
    unsigned events = 0;
    for (int i = 0; i < 60 * 60 && g.state == STATE_BATTLE && g.battle.phase == PHASE_MESSAGES; i++)
        events |= run(&NONE, 1);
    return events;
}

static void face(int x, int y)
{
    g.walker.face_x = x;
    g.walker.face_y = y;
}

static int pick(int entry)
{
    GameInput in = { .pick = entry + 1 };
    return (int)run(&in, 1);
}

static bool has_msg(MsgType type)
{
    for (int i = 0; i < g.battle.msg_count; i++)
        if (g.battle.msgs[i].type == type) return true;
    return false;
}

// --- Maps -------------------------------------------------------------------------

// Walks the map from the start through everything that does not block
// (cliffs, water, trees, bushes and rocks do). Every monster and exit must
// be reached, and every chest and campfire must have a reachable side.
static bool map_is_completable(const Game *m)
{
    static bool seen[MAX_MAP_H][MAX_MAP_W];
    static int queue[MAX_MAP_H * MAX_MAP_W];
    static const int dirs[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
    memset(seen, 0, sizeof seen);
    int sx = (int)(m->start_x / TILE), sy = (int)(m->start_y / TILE);
    int head = 0, tail = 0;
    queue[tail++] = sy * MAX_MAP_W + sx;
    seen[sy][sx] = true;
    while (head < tail) {
        int x = queue[head] % MAX_MAP_W, y = queue[head] / MAX_MAP_W;
        head++;
        for (int i = 0; i < 4; i++) {
            int nx = x + dirs[i][0], ny = y + dirs[i][1];
            if (nx < 0 || ny < 0 || nx >= m->w || ny >= m->h || seen[ny][nx]) continue;
            TileType t = game_tile_at(m, nx, ny);
            if (game_tile_blocks(t)) continue;
            seen[ny][nx] = true;
            queue[tail++] = ny * MAX_MAP_W + nx;
        }
    }
    bool ok = true;
    for (int y = 0; y < m->h; y++) {
        for (int x = 0; x < m->w; x++) {
            TileType t = game_tile_at(m, x, y);
            if (t == TILE_CHEST || t == TILE_CHEST_AXE || t == TILE_FIRE || t == TILE_GODDESS) {
                bool side = false;
                for (int i = 0; i < 4; i++) {
                    int nx = x + dirs[i][0], ny = y + dirs[i][1];
                    if (nx >= 0 && ny >= 0 && nx < m->w && ny < m->h && seen[ny][nx]) side = true;
                }
                if (!side) { printf("  tile %d at (%d, %d) cannot be reached\n", t, x, y); ok = false; }
            } else if (t == TILE_EXIT && !seen[y][x]) {
                printf("  exit at (%d, %d) cannot be reached\n", x, y);
                ok = false;
            }
        }
    }
    for (int i = 0; i < m->foe_count; i++) {
        int x = (int)(m->foes[i].x / TILE), y = (int)(m->foes[i].y / TILE);
        if (!seen[y][x]) { printf("  monster at (%d, %d) cannot be reached\n", x, y); ok = false; }
    }
    return ok;
}

static bool area_has(int area, char c)
{
    int count = 0;
    const char *const *rows = area_rows(area, &count);
    for (int r = 0; r < count; r++)
        if (strchr(rows[r], c)) return true;
    return false;
}

static void test_maps_are_valid(void)
{
    CHECK(area_count() >= 4 && area_count() <= MAX_AREAS);
    for (int i = 0; i < area_count(); i++) {
        int count = 0;
        const char *const *rows = area_rows(i, &count);
        memset(&g, 0, sizeof g);
        CHECK(area_load_rows(&g, rows, count));
        if (i == AREA_HOME) CHECK(g.foe_count == 0 && g.start_x >= 0);  // a safe place to start
        else CHECK(g.foe_count > 0);
        for (int r = 0; r < count; r++) {
            CHECK((int)strlen(rows[r]) == g.w);
            CHECK(strchr("#123456789", rows[r][0]) && strchr("#123456789", rows[r][g.w - 1]));
        }
        for (int x = 0; x < g.w; x++)
            CHECK(strchr("#123456789", rows[0][x]) && strchr("#123456789", rows[count - 1][x]));
        if (g.start_x < 0) {  // no start: search from the first path in
            for (int y = 0; y < g.h && g.start_x < 0; y++)
                for (int x = 0; x < g.w && g.start_x < 0; x++)
                    if (g.links[y][x]) { g.start_x = (float)x * TILE; g.start_y = (float)y * TILE; }
        }
        CHECK(map_is_completable(&g));
    }
    // Each path digit joins exactly two areas, and every area can be reached from home.
    bool reached[MAX_AREAS] = { [AREA_HOME] = true };
    for (int round = 0; round < MAX_AREAS; round++) {
        for (char c = '1'; c <= '9'; c++) {
            int n = 0, first = -1, second = -1;
            for (int i = 0; i < area_count(); i++)
                if (area_has(i, c)) { n++; if (first < 0) first = i; else second = i; }
            CHECK(n == 0 || n == 2);
            if (n == 2 && (reached[first] || reached[second])) reached[first] = reached[second] = true;
        }
    }
    for (int i = 0; i < area_count(); i++) CHECK(reached[i]);
    // The boss waits in the last area; the goddess and the fire beings in the woods.
    CHECK(area_has(area_count() - 1, 'B'));
    CHECK(area_has(AREA_WOODS, 'M') && area_has(AREA_WOODS, 'e'));

    // In the woods she is far from the entrance (out of sight when the cry
    // plays) and has three creatures around her.
    int count = 0;
    const char *const *woods = area_rows(AREA_WOODS, &count);
    int mx = -1, my = -1, ix = -1, iy = -1;
    for (int r = 0; r < count; r++) {
        const char *m = strchr(woods[r], 'M'), *in = strchr(woods[r], '1');
        if (m) { mx = (int)(m - woods[r]); my = r; }
        if (in) { ix = (int)(in - woods[r]); iy = r; }
    }
    CHECK(mx - ix >= VIEW_W / TILE + 2 || my - iy >= VIEW_H / TILE + 2);
    int around = 0;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++)
            if ((dx || dy) && woods[my + dy][mx + dx] == 'e') around++;
    CHECK(around == 3);
}

// Paths and bridges: every stretch of path leads to a way out of the area
// (no path cut off, none touching another only at a corner), and every
// bridge has land at both ends.
static bool pathish_char(char c) { return c == ':' || c == '=' || (c >= '1' && c <= '9'); }

static void test_paths_connect(void)
{
    for (int i = 0; i < area_count(); i++) {
        int h = 0;
        const char *const *rows = area_rows(i, &h);
        int w = (int)strlen(rows[0]);
        static bool seen[MAX_MAP_H][MAX_MAP_W];
        static int stack[MAX_MAP_H * MAX_MAP_W][2];
        memset(seen, 0, sizeof seen);
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                if (!pathish_char(rows[y][x]) || seen[y][x]) continue;
                bool exit_found = false;
                int top = 0;
                stack[top][0] = x, stack[top][1] = y, top++;
                seen[y][x] = true;
                while (top > 0) {
                    top--;
                    int cx = stack[top][0], cy = stack[top][1];
                    if (rows[cy][cx] >= '1' && rows[cy][cx] <= '9') exit_found = true;
                    static const int d[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
                    for (int k = 0; k < 4; k++) {
                        int nx = cx + d[k][0], ny = cy + d[k][1];
                        if (nx < 0 || ny < 0 || nx >= w || ny >= h || seen[ny][nx] || !pathish_char(rows[ny][nx])) continue;
                        seen[ny][nx] = true;
                        stack[top][0] = nx, stack[top][1] = ny, top++;
                    }
                }
                if (!exit_found) printf("  area %d: path at %d,%d leads nowhere\n", i, x, y);
                CHECK(exit_found);
            }
        for (int y = 1; y < h - 1; y++)
            for (int x = 1; x < w - 1; x++) {
                if (rows[y][x] != '=') continue;
                bool across = rows[y][x - 1] == '~' || rows[y][x + 1] == '~';  // walked north-south
                char a = across ? rows[y - 1][x] : rows[y][x - 1], b = across ? rows[y + 1][x] : rows[y][x + 1];
                if (a == '~' || b == '~') printf("  area %d: bridge at %d,%d ends in water\n", i, x, y);
                CHECK(a != '~' && b != '~');
            }
    }
}

static void test_bad_map_is_rejected(void)
{
    static const char *const no_end[] = { "#####", "#P..#", "#####" };
    memset(&g, 0, sizeof g);
    CHECK(!area_load_rows(&g, no_end, 3));
}

// --- Exploring ----------------------------------------------------------------------

// Trees, bushes and rocks are in the way for good: the axe is for battles.
static void test_trees_stay(void)
{
    static const char *const rows[] = { "#######", "#P.To9#", "#.R...#", "#######" };
    load(rows, COUNT(rows));
    run(&RIGHT, 60);
    CHECK(g.walker.x + WALKER_SIZE <= 3 * TILE + 0.01f);  // the tree stops him
    face(1, 0);
    run(&CONFIRM, 1);
    CHECK(game_tile_at(&g, 3, 1) == TILE_TREE && game_tile_at(&g, 4, 1) == TILE_BUSH);
    run(&RIGHT, 60);
    CHECK(g.walker.x + WALKER_SIZE <= 3 * TILE + 0.01f);
}

static void test_chests(void)
{
    static const char *const rows[] = { "#######", "#cPA.9#", "#######" };
    load(rows, COUNT(rows));
    int herbs = g.stats.herbs, atk = g.stats.atk;
    face(-1, 0);
    CHECK(run(&CONFIRM, 1) & EVENT_CHEST);
    CHECK(g.stats.herbs == herbs + CHEST_HERBS);
    face(1, 0);
    run(&CONFIRM, 1);
    CHECK(g.stats.atk == atk + AXE_UPGRADE_ATK);
    CHECK(game_tile_at(&g, 3, 1) == TILE_CHEST_OPEN);
    run(&CONFIRM, 1);  // an open chest gives nothing more
    CHECK(g.stats.atk == atk + AXE_UPGRADE_ATK);
}

static void test_campfire_heals_and_saves(void)
{
    static const char *const rows[] = { "#######", "#..P.9#", "#..f..#", "#######" };
    load(rows, COUNT(rows));
    g.stats.hp = 1;
    g.stats.sp = 0;
    run(&LEFT, 2);  // one tile left, then back
    run(&NONE, 30);
    run(&RIGHT, 2);
    CHECK(!(run(&CONFIRM, 1) & EVENT_REST));  // no action while walking
    run(&NONE, 30);
    face(0, 1);
    CHECK(run(&CONFIRM, 1) & EVENT_SAVE);
    CHECK(g.stats.hp == g.stats.max_hp && g.stats.sp == g.stats.max_sp);
    CHECK(g.respawn_x == g.walker.x);
}

// Puts the lumberjack on tile (tx, ty) of the current area.
static void place(int tx, int ty)
{
    g.walker.x = tx * TILE + (TILE - WALKER_SIZE) / 2.0f;
    g.walker.y = ty * TILE + (TILE - WALKER_SIZE) / 2.0f;
    g.walker.moving = false;
    g.walker.safe_timer = 10;
}

static void find_tile(char c, int area, int *tx, int *ty)
{
    int count = 0;
    const char *const *rows = area_rows(area, &count);
    for (int r = 0; r < count; r++) {
        const char *p = strchr(rows[r], c);
        if (p) { *tx = (int)(p - rows[r]); *ty = r; return; }
    }
    *tx = *ty = -1;
}

static void test_walking_between_areas(void)
{
    memset(&g, 0, sizeof g);
    game_new(&g);
    CHECK(g.area == AREA_HOME && g.state == STATE_MAP);
    int ex, ey;
    find_tile('1', AREA_HOME, &ex, &ey);
    place(ex - 1, ey);
    run(&RIGHT, 30);
    CHECK(g.area == AREA_WOODS);
    CHECK(g.state == STATE_SCENE && g.scene == SCENE_WOODS);  // a cry, the first time
    run(&NONE, 30);
    run(&CANCEL, 1);
    CHECK(g.state == STATE_MAP);
    CHECK(g.toast == TOAST_MAP && g.toast_amount == AREA_WOODS + 1);
    int wx, wy;
    find_tile('1', AREA_WOODS, &wx, &wy);
    CHECK((int)(g.walker.x / TILE) == wx + 1 && (int)(g.walker.y / TILE) == wy);  // next to the path

    // Things done in an area stay done.
    g.tiles[wy + 1][wx + 1] = TILE_CHEST_OPEN;
    g.foes[0].alive = false;
    place(wx + 1, wy);
    for (int i = 0; i < 60 && g.area == AREA_WOODS; i++) run(&LEFT, 1);
    CHECK(g.area == AREA_HOME);
    CHECK((int)(g.walker.x / TILE) == ex - 1);
    run(&RIGHT, 30);
    CHECK(g.area == AREA_WOODS);
    CHECK(g.state == STATE_MAP);  // no cry the second time
    CHECK(game_tile_at(&g, wx + 1, wy + 1) == TILE_CHEST_OPEN);
    CHECK(!g.foes[0].alive);

    // Deeper into the forest only with the goddess.
    for (int i = 0; i < g.foe_count; i++) g.foes[i].alive = false;
    int fx, fy;
    find_tile('2', AREA_WOODS, &fx, &fy);
    place(fx - 1, fy);
    run(&RIGHT, 30);
    CHECK(g.area == AREA_WOODS);
    CHECK(g.toast == TOAST_NOT_YET);
    g.goddess.joined = true;
    run(&RIGHT, 30);
    CHECK(g.area == AREA_VALLEY);
}

static void test_home_is_a_place_to_rest(void)
{
    memset(&g, 0, sizeof g);
    game_new(&g);
    int dx, dy;
    find_tile('D', AREA_HOME, &dx, &dy);
    place(dx, dy + 1);
    face(0, -1);
    g.stats.hp = 1;
    CHECK(run(&CONFIRM, 1) & EVENT_REST);
    CHECK(g.stats.hp == g.stats.max_hp && g.toast == TOAST_HOME);

    // Falling in the woods wakes him at home.
    game_enter_area(&g, AREA_WOODS, 1);
    run(&NONE, 30);
    run(&CANCEL, 1);
    g.stats.hp = 0;
    g.state = STATE_GAME_OVER;
    run(&NONE, 30);
    run(&CONFIRM, 1);
    CHECK(g.state == STATE_MAP && g.area == AREA_HOME);
    CHECK(g.stats.hp == g.stats.max_hp);
}

static void test_pause(void)
{
    static const char *const rows[] = { "######", "#P..9#", "######" };
    load(rows, COUNT(rows));
    GameInput pause = { .pause_pressed = true };
    run(&pause, 1);
    CHECK(g.state == STATE_PAUSED);
    float x = g.walker.x;
    run(&RIGHT, 30);
    CHECK(g.walker.x == x);
    run(&pause, 1);
    CHECK(g.state == STATE_MAP);
    // Enter or a tap also resumes, as the pause screen says.
    run(&pause, 1);
    CHECK(g.state == STATE_PAUSED);
    run(&CONFIRM, 1);
    CHECK(g.state == STATE_MAP);
    run(&pause, 1);
    GameInput tap = { .pick = 1 };
    run(&tap, 1);
    CHECK(g.state == STATE_MAP);
}

static void test_walking_tile_by_tile(void)
{
    static const char *const rows[] = { "#######", "#P....#", "#.....#", "#....9#", "#######" };
    load(rows, COUNT(rows));
    float x0 = g.walker.x, y0 = g.walker.y;
    run(&RIGHT, 2);  // a tap starts a step...
    run(&NONE, 30);  // ...which always ends on the next tile
    CHECK(g.walker.x == x0 + TILE && g.walker.y == y0);
    // Diagonal input moves along one axis only.
    GameInput diag = { .move_x = 1, .move_y = 0.6f };
    run(&diag, 2);
    run(&NONE, 30);
    CHECK(g.walker.x == x0 + 2 * TILE && g.walker.y == y0);
    GameInput down = { .move_y = 1 };
    run(&down, 2);
    run(&NONE, 30);
    CHECK(g.walker.x == x0 + 2 * TILE && g.walker.y == y0 + TILE);
    // Every stop is on a tile.
    run(&RIGHT, 7);
    run(&NONE, 30);
    CHECK(fmodf(g.walker.x - x0, TILE) == 0 && !g.walker.moving);
    // A wall turns him but he stays put.
    place(1, 1);
    GameInput up = { .move_y = -1 };
    run(&up, 20);
    CHECK(g.walker.y == y0 && g.walker.face_y == -1);
}

// --- Battles ------------------------------------------------------------------------

static const char *const one_slime[] = { "#######", "#P.s.9#", "#######" };
static const char *const two_slimes[] = { "#######", "#P.g.9#", "#######" };

static int count_msgs(MsgType type)
{
    int n = 0;
    for (int i = 0; i < g.battle.msg_count; i++) n += g.battle.msgs[i].type == type;
    return n;
}

static const BattleMsg *find_msg(MsgType type)
{
    for (int i = 0; i < g.battle.msg_count; i++)
        if (g.battle.msgs[i].type == type) return &g.battle.msgs[i];
    return NULL;
}

static void test_touching_a_monster_starts_a_battle(void)
{
    load(two_slimes, COUNT(two_slimes));
    CHECK(run(&RIGHT, 60) & EVENT_ENCOUNTER);
    CHECK(g.state == STATE_BATTLE);
    CHECK(g.battle.enemy_count == 2);
    CHECK(g.battle.enemies[0].species == SPECIES_SLIME);
    play_messages();
    CHECK(g.battle.phase == PHASE_COMMAND && g.battle.choosing == 0);
    CHECK(g.battle.order_count == 3);  // him and the two slimes
}

static void test_attack_hurts_and_monsters_answer(void)
{
    load(one_slime, COUNT(one_slime));
    game_start_battle(&g, 0);
    play_messages();
    int hp = g.stats.hp;
    pick(CMD_ATTACK);  // only one monster: no need to choose
    CHECK(g.battle.phase == PHASE_MESSAGES);
    CHECK(has_msg(MSG_ATTACK));
    CHECK(g.battle.enemies[0].hp < species_info(SPECIES_SLIME)->max_hp);
    if (g.battle.enemies[0].hp > 0 && has_msg(MSG_ENEMY_ATTACK)) CHECK(g.stats.hp < hp);
    unsigned events = play_messages();
    CHECK(events & EVENT_HIT);
}

static void test_choosing_a_target(void)
{
    load(two_slimes, COUNT(two_slimes));
    game_start_battle(&g, 0);
    play_messages();
    pick(CMD_ATTACK);
    CHECK(g.battle.phase == PHASE_TARGET);
    CHECK(g.battle.target == 0);
    GameInput next = { .nav_x = 1 };
    run(&next, 1);
    CHECK(g.battle.target == 1);
    run(&CANCEL, 1);
    CHECK(g.battle.phase == PHASE_COMMAND);
    pick(CMD_ATTACK);
    GameInput tap = { .target_pick = 2 };
    run(&tap, 1);
    CHECK(g.battle.phase == PHASE_MESSAGES);
    const BattleMsg *hit = find_msg(MSG_ATTACK);
    CHECK(hit && hit->target == 1);
}

static void test_skills_cost_stamina(void)
{
    load(two_slimes, COUNT(two_slimes));
    game_start_battle(&g, 0);
    play_messages();
    g.stats.sp = SKILL_SPIN_COST;
    pick(CMD_SPECIAL);
    CHECK(g.battle.phase == PHASE_SPECIAL);
    pick(SKILL_SPIN);
    CHECK(has_msg(MSG_SPIN));
    CHECK(count_msgs(MSG_SPIN_HIT) == 2);
    CHECK(g.stats.sp <= 1);  // spent 5, maybe got 1 back at the end of the round
    play_messages();
    if (g.state == STATE_BATTLE) {
        g.stats.sp = 0;
        CHECK(!battle_command_enabled(&g, CMD_SPECIAL));
        CHECK(pick(CMD_SPECIAL) & EVENT_DENIED);
        CHECK(g.battle.phase == PHASE_COMMAND);
    }
}

static void test_skills_menu_back(void)
{
    load(one_slime, COUNT(one_slime));
    game_start_battle(&g, 0);
    play_messages();
    pick(CMD_SPECIAL);
    pick(SKILL_BACK);
    CHECK(g.battle.phase == PHASE_COMMAND);
    pick(CMD_SPECIAL);
    run(&CANCEL, 1);
    CHECK(g.battle.phase == PHASE_COMMAND);
}

static void test_herb_heals(void)
{
    load(one_slime, COUNT(one_slime));
    game_start_battle(&g, 0);
    play_messages();
    CHECK(!battle_command_enabled(&g, CMD_ITEMS));  // full health
    g.stats.hp = 5;
    g.stats.herbs = 2;
    pick(CMD_ITEMS);
    CHECK(g.battle.phase == PHASE_ITEMS);
    pick(ITEM_HERB);
    const BattleMsg *m = find_msg(MSG_HERB);
    CHECK(m && m->amount == HERB_HEAL && !m->on_ally);
    CHECK(g.stats.herbs == 1);
}

static void test_defend_rests(void)
{
    load(one_slime, COUNT(one_slime));
    game_start_battle(&g, 0);
    play_messages();
    g.stats.sp = 0;
    pick(CMD_DEFEND);
    CHECK(has_msg(MSG_DEFEND));
    CHECK(g.stats.sp == 3);  // 2 for defending, 1 at the end of the round
    // Halved damage, even when the slime moves first: its hits stay small.
    for (int i = 0; i < g.battle.msg_count; i++)
        if (g.battle.msgs[i].type == MSG_ENEMY_ATTACK) CHECK(g.battle.msgs[i].amount <= 4);
}

static void test_moving_the_cursor(void)
{
    load(one_slime, COUNT(one_slime));
    game_start_battle(&g, 0);
    play_messages();
    CHECK(run(&DOWN, 1) & EVENT_CURSOR);
    CHECK(g.battle.cursor == CMD_SPECIAL);
    GameInput up = { .nav_y = -1 };
    run(&up, 2);
    CHECK(g.battle.cursor == CMD_FLEE);  // wraps around
}

static void test_winning_a_battle(void)
{
    load(one_slime, COUNT(one_slime));
    g.stats.atk = 99;
    game_start_battle(&g, 0);
    play_messages();
    pick(CMD_ATTACK);
    CHECK(has_msg(MSG_ENEMY_DOWN));
    CHECK(has_msg(MSG_VICTORY));
    CHECK(g.stats.xp == species_info(SPECIES_SLIME)->xp);
    unsigned events = play_messages();
    CHECK(events & EVENT_VICTORY);
    CHECK(g.state == STATE_MAP);
    CHECK(!g.foes[0].alive);
}

static void test_level_up(void)
{
    load(one_slime, COUNT(one_slime));
    g.stats.atk = 99;
    g.stats.xp = xp_to_next_level(1) - 1;
    int max_hp = g.stats.max_hp;
    g.stats.hp = 30;
    game_start_battle(&g, 0);
    play_messages();
    pick(CMD_ATTACK);
    CHECK(has_msg(MSG_LEVEL_UP));
    CHECK(g.stats.level == 2);
    CHECK(g.stats.max_hp > max_hp && g.stats.hp == g.stats.max_hp);
    CHECK(play_messages() & EVENT_LEVEL_UP);
}

static void test_fleeing(void)
{
    load(one_slime, COUNT(one_slime));
    bool fled = false;
    for (int attempt = 0; attempt < 20 && !fled; attempt++) {
        game_start_battle(&g, 0);
        g.stats.hp = g.stats.max_hp;
        play_messages();
        pick(CMD_FLEE);
        fled = has_msg(MSG_FLEE_OK);
        play_messages();
    }
    CHECK(fled);
    CHECK(g.state == STATE_MAP);
    CHECK(g.foes[0].alive);
    CHECK(g.walker.safe_timer > 0);
}

static void test_no_fleeing_from_the_boss(void)
{
    static const char *const rows[] = { "######", "#P.B.#", "######" };
    load(rows, COUNT(rows));
    game_start_battle(&g, 0);
    play_messages();
    CHECK(!battle_command_enabled(&g, CMD_FLEE));
    CHECK(pick(CMD_FLEE) & EVENT_DENIED);
    CHECK(g.battle.phase == PHASE_COMMAND);
}

static void test_losing_wakes_at_campfire(void)
{
    load(one_slime, COUNT(one_slime));
    g.respawn_x = 20;
    g.respawn_y = 20;
    game_start_battle(&g, 0);
    play_messages();
    g.stats.hp = 1;
    g.stats.def = 0;
    pick(CMD_DEFEND);  // even halved, a hit takes the last point
    CHECK(has_msg(MSG_PLAYER_DOWN));
    CHECK(play_messages() & EVENT_PLAYER_DOWN);
    CHECK(g.state == STATE_GAME_OVER);
    run(&NONE, 30);
    run(&CONFIRM, 1);
    CHECK(g.state == STATE_MAP);
    CHECK(g.stats.hp == g.stats.max_hp);
    CHECK(g.walker.x == 20 && g.walker.y == 20);
    CHECK(g.foes[0].alive);
}

static void test_beating_the_boss_wins(void)
{
    static const char *const rows[] = { "######", "#P.B.#", "######" };
    load(rows, COUNT(rows));
    g.stats.atk = 500;
    game_start_battle(&g, 0);
    play_messages();
    pick(CMD_ATTACK);
    play_messages();
    CHECK(g.state == STATE_WON);
}

// Speed decides the order: a wolf (fast) always goes before him, a slime
// (slow) almost never does; everyone acts once a round.
static void test_speed_sets_the_order(void)
{
    load(one_slime, COUNT(one_slime));
    int slime_first = 0;
    for (int r = 0; r < 40; r++) {
        game_start_battle(&g, 0);
        g.battle.enemies[0].hp = 999;
        play_messages();
        CHECK(g.battle.order_count == 2);
        slime_first += g.battle.order[0] == 0;
    }
    CHECK(slime_first <= 8);
    g.foes[0].kind = ENCOUNTER_WOLF;
    g.stats.hp = g.stats.max_hp = 999;
    for (int r = 0; r < 20; r++) {
        game_start_battle(&g, 0);
        g.battle.enemies[0].hp = 999;
        play_messages();
        CHECK(g.battle.order[0] == 0 && g.battle.order[1] == ACTOR_HERO);
        pick(CMD_DEFEND);
        CHECK(g.battle.msgs[0].actor == 0);  // the wolf acts first
        int hero_msgs = 0;
        for (int i = 0; i < g.battle.msg_count; i++) hero_msgs += g.battle.msgs[i].actor == ACTOR_HERO;
        CHECK(hero_msgs == 1);
    }
}

// Presses confirm until the scene is over; returns how many pages it took.
static int read_scene(void)
{
    int pages = 0;
    for (int i = 0; i < 600 && g.state == STATE_SCENE; i++) {
        int before = g.scene_page;
        run(&CONFIRM, 1);
        if (g.state != STATE_SCENE || g.scene_page != before) pages++;
    }
    return pages;
}

static void test_title_menu(void)
{
    static const GameInput RIGHT_KEY = { .nav_x = 1 };
    game_init(&g);
    run(&NONE, 30);
    run(&DOWN, 1);
    CHECK(g.title_cursor == TITLE_LOAD && (g.events & EVENT_CURSOR));
    CHECK(run(&CONFIRM, 1) & EVENT_DENIED);  // nothing saved yet
    g.save_count = 3;
    run(&CONFIRM, 1);
    CHECK(g.title_page == PAGE_LOAD && g.title_cursor == 0);
    run(&DOWN, 1);
    CHECK(run(&CONFIRM, 1) & EVENT_LOAD);
    CHECK(g.title_cursor == 1);  // the second save in the list
    run(&DOWN, 2);
    run(&CONFIRM, 1);  // the last entry is Back
    CHECK(g.title_page == PAGE_MAIN && g.title_cursor == TITLE_LOAD);
    run(&DOWN, 1);
    run(&CONFIRM, 1);
    CHECK(g.title_page == PAGE_OPTIONS && g.title_cursor == OPTION_LANGUAGE);
    CHECK(run(&RIGHT_KEY, 1) & EVENT_LANGUAGE);
    run(&DOWN, 1);
    CHECK(run(&CONFIRM, 1) & EVENT_SOUND);
    run(&CANCEL, 1);
    CHECK(g.title_page == PAGE_MAIN && g.title_cursor == TITLE_OPTIONS && g.state == STATE_TITLE);
    run(&DOWN, 1);
    CHECK(run(&CONFIRM, 1) & EVENT_QUIT);
    GameInput tap = { .pick = TITLE_NEW + 1 };  // touch: tap "New game"
    CHECK(run(&tap, 1) & EVENT_SAVE);
    CHECK(g.state == STATE_SCENE && g.scene == SCENE_INTRO && g.save_slot == 0);
}

static void clear_saves(void)
{
    int count;
    SaveInfo *old = save_list(&count);
    for (int i = 0; i < count; i++) {
        char path[256];
        snprintf(path, sizeof path, "%s/game_%d.save", save_dir(), old[i].slot);
        remove(path);
    }
    free(old);
}

// Saves: one file per game, as many as wanted, listed newest first.
static void test_saves(void)
{
    save_set_dir("test_saves/a/b");
    clear_saves();
    int count;
    for (int i = 0; i < 40; i++) {  // no limit
        game_new(&g);
        g.stats.level = 1 + i % 9;
        CHECK(save_write(&g));
        CHECK(g.save_slot == i + 1);
    }
    SaveInfo *list = save_list(&count);
    CHECK(count == 40 && list[0].slot == 40);
    free(list);
    game_new(&g);
    g.save_slot = 7;
    g.stats.level = 5;
    CHECK(save_write(&g));
    list = save_list(&count);
    CHECK(count == 40 && list[0].slot == 7 && list[0].level == 5 && list[0].area == AREA_HOME);
    free(list);
    CHECK(!save_read(99, &g));

    // A file that is not a save is not listed or loaded.
    FILE *f = fopen("test_saves/a/b/game_3.save", "wb");
    if (f) { fputs("hello", f); fclose(f); }
    CHECK(!save_read(3, &g));
    list = save_list(&count);
    CHECK(count == 39);
    free(list);

    bool pt = false, sound = true;
    unsigned hints = 0;
    CHECK(settings_write(true, false, 5));
    CHECK(settings_read(&pt, &sound, &hints) && pt && !sound && hints == 5);
}

// A save keeps the player's progress: what he has, the story, where he
// rested, and in every area the chests opened and the monsters beaten.
static void test_save_keeps_progress(void)
{
    save_set_dir("test_saves/progress");
    clear_saves();
    game_new(&g);
    g.heard_cry = true;
    g.goddess.joined = true;
    g.goddess.hp = 17;
    g.goddess.max_hp = 30;
    g.goddess.mp = 4;
    g.stats.level = 4;
    g.stats.xp = 33;
    g.stats.hp = 21;
    g.stats.max_hp = 70;
    g.stats.sp = 2;
    g.stats.herbs = 3;
    g.stats.atk = 15;
    game_enter_area(&g, AREA_WOODS, 0);
    g.state = STATE_MAP;
    g.foes[0].alive = false;
    g.foes[2].alive = false;
    int cx, cy;
    find_tile('c', AREA_WOODS, &cx, &cy);
    g.tiles[cy][cx] = TILE_CHEST_OPEN;
    game_enter_area(&g, AREA_VALLEY, 0);
    g.state = STATE_MAP;
    g.foes[1].alive = false;
    place(5, 5);
    g.walker.face_x = 1;
    g.walker.face_y = 0;
    g.respawn_area = AREA_VALLEY;
    g.respawn_x = 5 * TILE + 2;
    g.respawn_y = 5 * TILE + 2;
    CHECK(save_write(&g));
    int slot = g.save_slot;

    game_init(&g);
    CHECK(save_read(slot, &g));
    game_loaded(&g);
    CHECK(g.state == STATE_MAP && g.area == AREA_VALLEY);
    CHECK((int)(g.walker.x / TILE) == 5 && (int)(g.walker.y / TILE) == 5 && g.walker.face_x == 1);
    CHECK(g.stats.level == 4 && g.stats.xp == 33 && g.stats.hp == 21 && g.stats.max_hp == 70);
    CHECK(g.stats.sp == 2 && g.stats.herbs == 3 && g.stats.atk == 15);
    CHECK(g.goddess.joined && g.goddess.hp == 17 && g.goddess.max_hp == 30 && g.goddess.mp == 4);
    CHECK(g.heard_cry && g.respawn_area == AREA_VALLEY && (int)g.respawn_x == 5 * TILE + 2);
    CHECK(!g.foes[1].alive && g.foes[0].alive);
    CHECK(g.save_slot == slot);
    // The woods as he left them.
    game_enter_area(&g, AREA_WOODS, 0);
    CHECK(g.state == STATE_MAP);  // no cry again
    CHECK(!g.foes[0].alive && g.foes[1].alive && !g.foes[2].alive);
    CHECK(game_tile_at(&g, cx, cy) == TILE_CHEST_OPEN);

    // A save from a newer game version, with keys this one does not know,
    // still loads.
    FILE *f = fopen("test_saves/progress/game_50.save", "wb");
    if (f) {
        fputs("timberheart save\nversion=9\nslot=50\norder=99\nlevel=6\nsomething_new=4\narea=2\nherbs=8\n", f);
        fclose(f);
    }
    CHECK(save_read(50, &g));
    CHECK(g.area == AREA_VALLEY && g.stats.level == 6 && g.stats.herbs == 8);
}

static void test_title_starts_game(void)
{
    game_init(&g);
    CHECK(g.state == STATE_TITLE);
    run(&CONFIRM, 1);
    CHECK(g.state == STATE_TITLE);  // too soon after the screen appeared
    run(&NONE, 30);
    run(&CONFIRM, 1);
    CHECK(g.state == STATE_SCENE && g.scene == SCENE_INTRO);
    CHECK(read_scene() == scene_page_count(SCENE_INTRO));
    CHECK(g.state == STATE_MAP);
    CHECK(g.area == AREA_HOME && g.stats.level == 1);
    CHECK(!g.goddess_saved);
}

static void test_scene_can_be_skipped(void)
{
    game_init(&g);
    run(&NONE, 30);
    run(&CONFIRM, 1);
    CHECK(g.state == STATE_SCENE);
    run(&NONE, 30);
    run(&CANCEL, 1);
    CHECK(g.state == STATE_MAP);
}

static const char *const goddess_map[] = {
    "#########",
    "#P......#",
    "#...e...#",
    "#..eMe..#",
    "#......9#",
    "#########",
};

static void test_saving_the_goddess(void)
{
    load(goddess_map, COUNT(goddess_map));
    // Three creatures around her, each one on the map.
    CHECK(g.foe_count == 3);
    for (int i = 0; i < 3; i++) CHECK(g.foes[i].kind == ENCOUNTER_EMBERS);
    float x = g.foes[0].x, y = g.foes[0].y;
    g.foes[0].think = 0;
    g.walker.safe_timer = 10;
    run(&NONE, 120);
    CHECK(g.foes[0].x == x && g.foes[0].y == y);  // they stay by her

    game_start_battle(&g, 0);
    CHECK(g.battle.enemy_count == 3 && g.battle.enemies[0].species == SPECIES_EMBER);
    play_messages();
    CHECK(pick(CMD_FLEE) & EVENT_DENIED);  // no running from them
    g.stats.atk = 99;
    g.stats.hp = g.stats.max_hp;
    while (g.state == STATE_BATTLE) {
        pick(CMD_ATTACK);
        if (g.battle.phase == PHASE_TARGET) run(&CONFIRM, 1);
        play_messages();
    }
    CHECK(g.state == STATE_SCENE && g.scene == SCENE_GODDESS);
    CHECK(g.goddess_saved && !g.goddess.joined);  // still on the ground while they talk
    CHECK(game_tile_at(&g, 4, 3) == TILE_GODDESS);
    for (int i = 0; i < 3; i++) CHECK(!g.foes[i].alive);  // all three fell back
    CHECK(read_scene() == scene_page_count(SCENE_GODDESS));
    CHECK(g.state == STATE_MAP);
    CHECK(g.goddess.joined);
    CHECK(g.goddess.hp == g.goddess.max_hp && g.goddess.hp > 0);
    CHECK(game_tile_at(&g, 4, 3) == TILE_FLOOR);  // she got up and left her spot

    // She follows him, on the tile he just left.
    place(1, 1);
    run(&RIGHT, 120);
    run(&NONE, 30);
    int hx = (int)(g.walker.x / TILE), hy = (int)(g.walker.y / TILE);
    CHECK(hx == 7 && hy == 1);  // stopped by the wall
    CHECK((int)(g.follower.x / TILE) == hx - 1 && (int)(g.follower.y / TILE) == hy);
    CHECK(g.follower.x == g.walker.x - TILE && g.follower.y == g.walker.y);
}

static void test_reaching_the_creatures_plays_their_scene(void)
{
    load(goddess_map, COUNT(goddess_map));
    g.walker.safe_timer = 0;
    // Two tiles away from them is close enough.
    for (int i = 0; i < 60 && g.state == STATE_MAP; i++) run(&RIGHT, 1);
    CHECK(g.state == STATE_SCENE && g.scene == SCENE_CREATURES);
    CHECK(g.creatures_met);
    CHECK(read_scene() == scene_page_count(SCENE_CREATURES));
    CHECK(g.state == STATE_BATTLE);  // the battle follows the scene
    CHECK(g.battle.enemy_count == 3);
}

static void test_waking_the_elder_stump(void)
{
    memset(&g, 0, sizeof g);
    game_new(&g);
    g.goddess.joined = true;
    game_enter_area(&g, AREA_CLEARING, 0);
    g.state = STATE_MAP;
    int bx, by;
    find_tile('B', AREA_CLEARING, &bx, &by);
    for (int i = 0; i < g.foe_count; i++)
        if (g.foes[i].kind != ENCOUNTER_BOSS) g.foes[i].alive = false;
    place(bx, by + 6);
    run(&UP, 40);
    CHECK(g.state == STATE_SCENE && g.scene == SCENE_ELDER);
    CHECK(g.elder_woke);
    run(&NONE, 30);
    run(&CANCEL, 1);
    CHECK(g.state == STATE_BATTLE && g.battle.enemies[0].species == SPECIES_ELDER_STUMP);
}

// The axe and the Moon Beam have sounds of their own.
static void test_battle_sound_events(void)
{
    memset(&g, 0, sizeof g);
    game_new(&g);
    g.goddess.joined = true;
    g.goddess.hp = g.goddess.max_hp = 40;
    g.goddess.mp = g.goddess.max_mp = 30;
    g.stats.hp = g.stats.max_hp = 500;  // he will not fall
    game_enter_area(&g, AREA_VALLEY, 0);
    g.state = STATE_MAP;
    game_start_battle(&g, 0);
    unsigned seen = 0;
    for (int i = 0; i < 400 && g.state == STATE_BATTLE; i++) {
        seen |= play_messages();
        if (g.state != STATE_BATTLE) break;
        if (g.battle.choosing == 1) { seen |= pick(CMD_SPECIAL); seen |= pick(SPELL_BEAM); }
        else seen |= pick(CMD_ATTACK);
        if (g.battle.phase == PHASE_TARGET) seen |= run(&CONFIRM, 1);
    }
    CHECK(seen & EVENT_AXE);
    CHECK(seen & EVENT_MOONBEAM);
    CHECK(seen & EVENT_VICTORY);
}

static void test_story_is_complete(void)
{
    for (int sc = 0; sc < SCENE_COUNT; sc++) {
        CHECK(scene_page_count((Scene)sc) > 0);
        CHECK(story_page((Scene)sc, scene_page_count((Scene)sc)) == NULL);
        for (int i = 0; i < scene_page_count((Scene)sc); i++) {
            const StoryPage *p = story_page((Scene)sc, i);
            CHECK(p && p->en && p->pt && p->en[0] && p->pt[0]);
            CHECK(p->who >= SPEAKER_NONE && p->who < SPEAKER_COUNT);
            CHECK(p->who == SPEAKER_NONE || story_speaker_name(p->who, true)[0]);
        }
    }
}

// A battle with the goddess at his side, against one slime.
static void start_party_battle(void)
{
    load(one_slime, COUNT(one_slime));
    g.goddess.joined = true;
    g.goddess.max_hp = g.goddess.hp = 30;
    g.goddess.max_mp = g.goddess.mp = 10;
    g.goddess.mag = 10;
    g.goddess.def = 2;
    game_start_battle(&g, 0);
    CHECK(g.battle.ally_in);
    play_messages();
}

// She has the same commands as he does; her second one opens her spells.
static void test_goddess_has_her_own_menu(void)
{
    start_party_battle();
    g.battle.enemies[0].hp = 999;
    CHECK(g.battle.order_count == 3);
    pick(CMD_DEFEND);
    CHECK(g.battle.phase == PHASE_COMMAND && g.battle.choosing == 1);  // her turn to choose
    run(&CANCEL, 1);
    CHECK(g.battle.choosing == 0);  // he can change his mind
    pick(CMD_DEFEND);
    pick(CMD_SPECIAL);
    CHECK(g.battle.phase == PHASE_SPECIAL);
    pick(SPELL_BEAM);
    CHECK(has_msg(MSG_BEAM));
    CHECK(g.goddess.mp <= 10 - SPELL_BEAM_COST + 1);
    CHECK(g.battle.enemies[0].hp < 999);
    play_messages();

    // Her plain attack costs nothing.
    int mp = g.goddess.mp;
    pick(CMD_DEFEND);
    pick(CMD_ATTACK);
    const BattleMsg *hit = find_msg(MSG_ATTACK);
    CHECK(hit && hit->actor == ACTOR_GODDESS);
    CHECK(g.goddess.mp >= mp);
    play_messages();

    // A herb from him to her.
    g.goddess.hp = 5;
    g.stats.herbs = 1;
    pick(CMD_ITEMS);
    pick(ITEM_HERB);
    CHECK(g.battle.phase == PHASE_ALLY_TARGET && g.battle.target == 1);  // the one with less health
    GameInput tap_her = { .ally_pick = 2 };
    run(&tap_her, 1);
    CHECK(g.battle.choosing == 1);
    CHECK(!battle_command_enabled(&g, CMD_ITEMS));  // the only herb is promised
    pick(CMD_DEFEND);
    const BattleMsg *m = find_msg(MSG_HERB);
    CHECK(m && m->on_ally && m->actor == ACTOR_HERO);
    CHECK(g.stats.herbs == 0);
    play_messages();

    g.stats.hp = 5;
    g.goddess.hp = 5;
    pick(CMD_DEFEND);
    pick(CMD_SPECIAL);
    pick(SPELL_LIGHT);
    CHECK(has_msg(MSG_LIGHT));
    play_messages();
    CHECK(g.state == STATE_BATTLE);

    // Without magic left, the spells are closed.
    g.goddess.mp = 0;
    pick(CMD_DEFEND);
    CHECK(pick(CMD_SPECIAL) & EVENT_DENIED);
    CHECK(g.battle.phase == PHASE_COMMAND);
    pick(CMD_DEFEND);
    const BattleMsg *d = find_msg(MSG_DEFEND);
    CHECK(d != NULL);
}

static void test_monsters_attack_the_goddess_too(void)
{
    start_party_battle();
    g.foes[0].kind = ENCOUNTER_SLIMES;
    g.battle.enemies[0].hp = 999;
    g.stats.hp = g.stats.max_hp = 999;
    bool hit_her = false, hit_him = false;
    for (int round = 0; round < 40 && g.goddess.hp > 0; round++) {
        g.stats.hp = 999;
        pick(CMD_DEFEND);
        pick(CMD_DEFEND);
        for (int i = 0; i < g.battle.msg_count; i++) {
            const BattleMsg *m = &g.battle.msgs[i];
            if (m->type == MSG_ENEMY_ATTACK) { if (m->on_ally) hit_her = true; else hit_him = true; }
        }
        play_messages();
    }
    CHECK(hit_her && hit_him);

    // When she falls, she sits out and gets back up after the battle.
    g.goddess.hp = 0;
    pick(CMD_ATTACK);
    CHECK(g.battle.choosing == 0);
    g.battle.enemies[0].hp = 0;
    g.stats.atk = 99;
    play_messages();
    while (g.state == STATE_BATTLE) {
        pick(CMD_ATTACK);
        play_messages();
    }
    CHECK(g.goddess.hp == 1);
}

// Plays every enemy turn of `rounds` rounds and counts the moves they make.
static void count_moves(Encounter kind, int rounds, int moves[MOVE_COUNT], bool goddess)
{
    load(one_slime, COUNT(one_slime));
    g.foes[0].kind = kind;
    if (goddess) {
        g.goddess.joined = true;
        g.goddess.max_hp = g.goddess.hp = 400;
        g.goddess.max_mp = g.goddess.mp = 20;
        g.goddess.def = 3;
        g.goddess.mag = 10;
    }
    memset(moves, 0, sizeof(int) * MOVE_COUNT);
    for (int r = 0; r < rounds; r++) {
        if (g.state != STATE_BATTLE) game_start_battle(&g, 0);
        g.stats.hp = g.stats.max_hp = 400;
        g.goddess.hp = goddess ? 400 : 0;
        play_messages();
        while (g.state == STATE_BATTLE && g.battle.phase != PHASE_MESSAGES) pick(CMD_DEFEND);
        for (int i = 0; i < g.battle.msg_count; i++) {
            const BattleMsg *m = &g.battle.msgs[i];
            if (m->actor < 0) continue;
            if (m->type == MSG_ENEMY_ATTACK || m->type == MSG_ENEMY_MISS || m->type == MSG_ENEMY_BUFF ||
                m->type == MSG_ENEMY_HEAL)
                moves[m->move]++;
            if (m->type == MSG_ENEMY_DEFEND) moves[MOVE_DEFEND]++;
        }
    }
}

// The stronger monsters use their own moves.
static void test_monster_specials(void)
{
    int moves[MOVE_COUNT];
    count_moves(ENCOUNTER_WOLVES, 30, moves, false);
    CHECK(moves[MOVE_BITE] > 0 && moves[MOVE_HOWL] > 0 && moves[MOVE_ATTACK] > 0);
    count_moves(ENCOUNTER_BEETLE, 30, moves, false);
    CHECK(moves[MOVE_SHELL] > 0 && moves[MOVE_HORN] > 0);
    count_moves(ENCOUNTER_EMBERS, 30, moves, true);
    CHECK(moves[MOVE_FLAME] > 0);
    count_moves(ENCOUNTER_BOSS, 30, moves, true);
    CHECK(moves[MOVE_ROOTS] > 0 && moves[MOVE_SLAM] > 0);
    // A slime has no special moves.
    count_moves(ENCOUNTER_SLIMES, 20, moves, false);
    CHECK(moves[MOVE_BITE] + moves[MOVE_HOWL] + moves[MOVE_SHELL] + moves[MOVE_ROOTS] == 0);
}

// The Elder Stump heals when low, twice at most, and its roots can hold one
// of them, who then loses a turn.
static void test_elder_stump_sap_and_roots(void)
{
    static const char *const rows[] = { "######", "#P.B.#", "######" };
    load(rows, COUNT(rows));
    g.goddess.joined = true;
    g.goddess.max_hp = 400;
    g.goddess.def = 3;
    int saps = 0, held = 0, bound = 0;
    for (int fight = 0; fight < 10; fight++) {
        game_start_battle(&g, 0);
        for (int r = 0; r < 12 && g.state == STATE_BATTLE; r++) {
            g.stats.hp = g.stats.max_hp = 400;
            g.goddess.hp = 400;
            g.battle.enemies[0].hp = 60;  // low: time for sap
            play_messages();
            while (g.state == STATE_BATTLE && g.battle.phase != PHASE_MESSAGES) pick(CMD_DEFEND);
            saps += count_msgs(MSG_ENEMY_HEAL);
            held += count_msgs(MSG_HELD);
            bound += count_msgs(MSG_BOUND);
        }
        CHECK(g.battle.enemies[0].saps <= 2);
    }
    CHECK(saps > 0);
    CHECK(held > 0 && bound > 0);
}

// Clever monsters finish off whoever they can knock out, and go for him
// (that ends the battle) when they can.
static void test_monsters_go_for_the_kill(void)
{
    start_party_battle();
    g.foes[0].kind = ENCOUNTER_WOLF;
    int at_him = 0, total = 0;
    for (int r = 0; r < 30; r++) {
        game_start_battle(&g, 0);
        g.battle.enemies[0].hp = 999;
        g.stats.hp = 4;                 // one bite and he is out
        g.goddess.hp = g.goddess.max_hp = 30;
        play_messages();
        while (g.state == STATE_BATTLE && g.battle.phase != PHASE_MESSAGES) pick(CMD_ATTACK);
        const BattleMsg *m = find_msg(MSG_ENEMY_ATTACK);
        if (!m) continue;
        total++;
        at_him += !m->on_ally;
        g.stats.hp = g.stats.max_hp;
    }
    CHECK(total > 20 && at_him * 10 >= total * 9);
}

// Monsters guard when badly hurt while a friend fights on.
static void test_monsters_defend(void)
{
    load(one_slime, COUNT(one_slime));
    g.foes[0].kind = ENCOUNTER_WOLVES;
    int guards = 0;
    for (int r = 0; r < 30; r++) {
        game_start_battle(&g, 0);
        g.stats.hp = g.stats.max_hp = 400;
        g.battle.enemies[0].hp = 3;
        play_messages();
        pick(CMD_DEFEND);
        for (int i = 0; i < g.battle.msg_count; i++)
            guards += g.battle.msgs[i].type == MSG_ENEMY_DEFEND && g.battle.msgs[i].actor == 0;
    }
    CHECK(guards >= 10);
    // A guarding monster takes half.
    game_start_battle(&g, 0);
    play_messages();
    g.battle.enemies[0].guarding = true;
    g.battle.enemies[0].hp = 999;
    g.stats.atk = 40;
    g.battle.order[0] = ACTOR_HERO;  // he goes first: the guard still holds
    g.battle.order[1] = 0;
    g.battle.order[2] = 1;
    g.battle.order_count = 3;
    pick(CMD_ATTACK);
    run(&CONFIRM, 1);
    const BattleMsg *m = find_msg(MSG_ATTACK);
    CHECK(m && m->target == 0 && m->amount <= (int)(40 * 1.15f * 1.6f) / 2 + 1);
}

// --- Balance --------------------------------------------------------------------------

// Fights a whole battle with a simple strategy: a herb below a third of the
// health, the strongest skill there is stamina for, else a plain attack.
// Returns true on a win.
static bool auto_battle_with(Encounter kind, int level, int herbs, unsigned seed, bool goddess)
{
    static const char *const rows[] = { "######", "#P.s.#", "#...9#", "######" };
    load(rows, COUNT(rows));
    g.foes[0].kind = kind;
    g.rng = seed * 2654435761u + 1u;  // a different fight each time
    for (int l = 1; l < level; l++) {
        g.stats.level++;
        g.stats.max_hp += 8;
        g.stats.max_sp += 2;
        g.stats.atk += 2;
        g.stats.def += 1;
    }
    g.stats.hp = g.stats.max_hp;
    g.stats.sp = g.stats.max_sp;
    g.stats.herbs = herbs;
    if (goddess) {
        g.goddess.joined = true;
        goddess_scale(&g, true);
    }
    game_start_battle(&g, 0);
    for (int round = 0; round < 200 && g.state == STATE_BATTLE; round++) {
        play_messages();
        if (g.state != STATE_BATTLE) break;
        const Stats *s = &g.stats;
        int alive = 0;
        for (int i = 0; i < g.battle.enemy_count; i++) alive += g.battle.enemies[i].hp > 0;
        if (g.battle.choosing == 0) {
            if (s->hp * 3 < s->max_hp && battle_command_enabled(&g, CMD_ITEMS)) {
                pick(CMD_ITEMS);
                pick(ITEM_HERB);
                if (g.battle.phase == PHASE_ALLY_TARGET) { GameInput me = { .ally_pick = 1 }; run(&me, 1); }
            } else if (alive > 1 && s->sp >= SKILL_SPIN_COST) {
                pick(CMD_SPECIAL);
                pick(SKILL_SPIN);
            } else if (s->sp >= SKILL_SPLIT_COST) {
                pick(CMD_SPECIAL);
                pick(SKILL_SPLIT);
            } else {
                pick(CMD_ATTACK);
            }
        } else {
            // She heals when he is hurt, else casts her beam, else strikes.
            if (s->hp * 2 < s->max_hp && battle_special_enabled(&g, SPELL_LIGHT)) {
                pick(CMD_SPECIAL);
                pick(SPELL_LIGHT);
            } else if (battle_command_enabled(&g, CMD_SPECIAL)) {
                pick(CMD_SPECIAL);
                pick(SPELL_BEAM);
            } else {
                pick(CMD_ATTACK);
            }
        }
        if (g.battle.phase == PHASE_TARGET) run(&CONFIRM, 1);
    }
    return !g.foes[0].alive && (g.state == STATE_MAP || g.state == STATE_SCENE || g.state == STATE_WON);
}

static bool auto_battle(Encounter kind, int level, int herbs, unsigned seed)
{
    return auto_battle_with(kind, level, herbs, seed, false);
}

// Wins out of 20 fights, and prints the rate so balancing is easy to follow.
static int rate(const char *what, Encounter kind, int level, int herbs, bool goddess)
{
    int wins = 0;
    for (unsigned i = 0; i < 20; i++) wins += auto_battle_with(kind, level, herbs, i, goddess);
    printf("       %-34s %2d/20\n", what, wins);
    return wins;
}

static void test_balance(void)
{
    // The fire beings are the very first fight: always won, but it hurts.
    CHECK(rate("fire creatures, level 1", ENCOUNTER_EMBERS, 1, 0, false) >= 18);
    // A new lumberjack handles the first monsters.
    CHECK(rate("two slimes, level 1", ENCOUNTER_SLIMES, 1, 0, false) >= 17);
    CHECK(rate("beetle and slime, level 2", ENCOUNTER_BEETLE, 2, 1, false) >= 15);
    // Wolves need a few levels, or the goddess.
    CHECK(rate("two wolves, level 4", ENCOUNTER_WOLVES, 4, 2, false) >= 13);
    CHECK(rate("two wolves, level 3, goddess", ENCOUNTER_WOLVES, 3, 1, true) >= 16);
    // The boss is a real fight: alone, only a well-trained lumberjack wins.
    CHECK(rate("Elder Stump, level 9, alone", ENCOUNTER_BOSS, 9, 4, false) >= 15);
    CHECK(rate("Elder Stump, level 7, alone", ENCOUNTER_BOSS, 7, 4, false) <= 13);
    CHECK(rate("Elder Stump, level 1, alone", ENCOUNTER_BOSS, 1, 0, false) == 0);
    // With the goddess it still needs levels: lost at 5, a toss-up at 6,
    // won at 7.
    CHECK(rate("Elder Stump, level 5, goddess", ENCOUNTER_BOSS, 5, 3, true) <= 5);
    CHECK(rate("Elder Stump, level 6, goddess", ENCOUNTER_BOSS, 6, 3, true) >= 8);
    CHECK(rate("Elder Stump, level 7, goddess", ENCOUNTER_BOSS, 7, 3, true) >= 18);
    CHECK(rate("Elder Stump, level 2, goddess", ENCOUNTER_BOSS, 2, 0, true) <= 2);
}

int main(void)
{
    struct {
        const char *name;
        void (*fn)(void);
    } tests[] = {
        { "maps are valid and completable", test_maps_are_valid },
        { "paths connect", test_paths_connect },
        { "scene can be skipped", test_scene_can_be_skipped },
        { "saving the goddess", test_saving_the_goddess },
        { "reaching the creatures plays their scene", test_reaching_the_creatures_plays_their_scene },
        { "waking the Elder Stump", test_waking_the_elder_stump },
        { "story is complete", test_story_is_complete },
        { "goddess has her own menu", test_goddess_has_her_own_menu },
        { "speed sets the order", test_speed_sets_the_order },
        { "monster specials", test_monster_specials },
        { "Elder Stump sap and roots", test_elder_stump_sap_and_roots },
        { "monsters go for the kill", test_monsters_go_for_the_kill },
        { "monsters defend", test_monsters_defend },
        { "monsters attack the goddess too", test_monsters_attack_the_goddess_too },
        { "bad map is rejected", test_bad_map_is_rejected },
        { "trees stay", test_trees_stay },
        { "chests", test_chests },
        { "campfire heals and saves", test_campfire_heals_and_saves },
        { "walking between areas", test_walking_between_areas },
        { "home is a place to rest", test_home_is_a_place_to_rest },
        { "pause", test_pause },
        { "walking tile by tile", test_walking_tile_by_tile },
        { "touching a monster starts a battle", test_touching_a_monster_starts_a_battle },
        { "attack hurts and monsters answer", test_attack_hurts_and_monsters_answer },
        { "choosing a target", test_choosing_a_target },
        { "skills cost stamina", test_skills_cost_stamina },
        { "skills menu back", test_skills_menu_back },
        { "herb heals", test_herb_heals },
        { "defend rests", test_defend_rests },
        { "moving the cursor", test_moving_the_cursor },
        { "winning a battle", test_winning_a_battle },
        { "level up", test_level_up },
        { "fleeing", test_fleeing },
        { "no fleeing from the boss", test_no_fleeing_from_the_boss },
        { "losing wakes at the campfire", test_losing_wakes_at_campfire },
        { "beating the boss wins", test_beating_the_boss_wins },
        { "title starts game", test_title_starts_game },
        { "title menu", test_title_menu },
        { "saves", test_saves },
        { "save keeps progress", test_save_keeps_progress },
        { "battle sound events", test_battle_sound_events },
        { "balance", test_balance },
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
