// Tests for the game rules (game.c, levels.c). They need no window or GPU.
#include "game.h"
#include "story.h"

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
static const GameInput LEFT = { .move_x = -1 };
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

// Walks the map from the start through everything but cliffs, rocks and
// water (trees count as open: the axe clears them). Every monster and exit
// must be reached, and every chest and campfire must have a reachable side.
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
            if (t != TILE_TREE && game_tile_blocks(t)) continue;
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
}

static void test_bad_map_is_rejected(void)
{
    static const char *const no_end[] = { "#####", "#P..#", "#####" };
    memset(&g, 0, sizeof g);
    CHECK(!area_load_rows(&g, no_end, 3));
}

// --- Exploring ----------------------------------------------------------------------

static void test_axe_chops_trees(void)
{
    static const char *const rows[] = { "#######", "#P.T.9#", "#######" };
    load(rows, COUNT(rows));
    run(&RIGHT, 60);
    CHECK(g.walker.x + WALKER_SIZE <= 3 * TILE + 0.01f);  // the tree stops him
    face(1, 0);
    CHECK(run(&CONFIRM, 1) & EVENT_CHOP);
    CHECK(game_tile_at(&g, 3, 1) == TILE_STUMP);
    run(&RIGHT, 60);
    CHECK(g.walker.x > 3 * TILE);  // the way is open
}

static void test_rocks_are_too_hard(void)
{
    static const char *const rows[] = { "#######", "#PR..9#", "#######" };
    load(rows, COUNT(rows));
    face(1, 0);
    CHECK(run(&CONFIRM, 1) & EVENT_BUMP);
    CHECK(game_tile_at(&g, 2, 1) == TILE_ROCK);
    CHECK(g.toast == TOAST_TOO_HARD);
}

static void test_chests(void)
{
    static const char *const rows[] = { "#######", "#cPA.9#", "#######" };
    load(rows, COUNT(rows));
    int herbs = g.stats.herbs, atk = g.stats.atk;
    face(-1, 0);
    CHECK(run(&CONFIRM, 1) & EVENT_CHEST);
    CHECK(g.stats.herbs == herbs + 2);
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
    CHECK(run(&CONFIRM, 1) & EVENT_REST);
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
    g.tiles[wy + 1][wx + 1] = TILE_STUMP;
    g.foes[0].alive = false;
    place(wx + 1, wy);
    for (int i = 0; i < 60 && g.area == AREA_WOODS; i++) run(&LEFT, 1);
    CHECK(g.area == AREA_HOME);
    CHECK((int)(g.walker.x / TILE) == ex - 1);
    run(&RIGHT, 30);
    CHECK(g.area == AREA_WOODS);
    CHECK(g.state == STATE_MAP);  // no cry the second time
    CHECK(game_tile_at(&g, wx + 1, wy + 1) == TILE_STUMP);
    CHECK(!g.foes[0].alive);
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

static void test_touching_a_monster_starts_a_battle(void)
{
    load(two_slimes, COUNT(two_slimes));
    CHECK(run(&RIGHT, 60) & EVENT_ENCOUNTER);
    CHECK(g.state == STATE_BATTLE);
    CHECK(g.battle.enemy_count == 2);
    CHECK(g.battle.enemies[0].species == SPECIES_SLIME);
    play_messages();
    CHECK(g.battle.phase == PHASE_MENU);
}

static void test_attack_hurts_and_monsters_answer(void)
{
    load(one_slime, COUNT(one_slime));
    game_start_battle(&g, 0);
    play_messages();
    int hp = g.stats.hp;
    pick(MENU_ATTACK);  // only one monster: no need to choose
    CHECK(g.battle.phase == PHASE_MESSAGES);
    CHECK(has_msg(MSG_ATTACK));
    CHECK(g.battle.enemies[0].hp < species_info(SPECIES_SLIME)->max_hp);
    if (g.battle.enemies[0].hp > 0) CHECK(g.stats.hp < hp);
    unsigned events = play_messages();
    CHECK(events & EVENT_HIT);
}

static void test_choosing_a_target(void)
{
    load(two_slimes, COUNT(two_slimes));
    game_start_battle(&g, 0);
    play_messages();
    pick(MENU_ATTACK);
    CHECK(g.battle.phase == PHASE_TARGET);
    CHECK(g.battle.target == 0);
    GameInput next = { .nav_x = 1 };
    run(&next, 1);
    CHECK(g.battle.target == 1);
    run(&CANCEL, 1);
    CHECK(g.battle.phase == PHASE_MENU);
    pick(MENU_ATTACK);
    GameInput tap = { .target_pick = 2 };
    run(&tap, 1);
    CHECK(g.battle.phase == PHASE_MESSAGES);
    CHECK(g.battle.enemies[1].hp < species_info(SPECIES_SLIME)->max_hp);
    CHECK(g.battle.enemies[0].hp == species_info(SPECIES_SLIME)->max_hp);
}

static void test_skills_cost_stamina(void)
{
    load(two_slimes, COUNT(two_slimes));
    game_start_battle(&g, 0);
    play_messages();
    g.stats.sp = SKILL_SPIN_COST;
    pick(MENU_SKILLS);
    CHECK(g.battle.phase == PHASE_SKILLS);
    pick(SKILL_SPIN);
    CHECK(has_msg(MSG_SPIN));
    int hits = 0;
    for (int i = 0; i < g.battle.msg_count; i++) hits += g.battle.msgs[i].type == MSG_SPIN_HIT;
    CHECK(hits == 2);
    CHECK(g.stats.sp <= 1);  // spent 5, maybe got 1 back at the end of the round
    play_messages();
    if (g.state == STATE_BATTLE) {
        g.stats.sp = 0;
        CHECK(!battle_menu_enabled(&g, MENU_SKILLS));
        CHECK(pick(MENU_SKILLS) & EVENT_DENIED);
        CHECK(g.battle.phase == PHASE_MENU);
    }
}

static void test_skills_menu_back(void)
{
    load(one_slime, COUNT(one_slime));
    game_start_battle(&g, 0);
    play_messages();
    pick(MENU_SKILLS);
    pick(SKILL_BACK);
    CHECK(g.battle.phase == PHASE_MENU);
    pick(MENU_SKILLS);
    run(&CANCEL, 1);
    CHECK(g.battle.phase == PHASE_MENU);
}

static void test_herb_heals(void)
{
    load(one_slime, COUNT(one_slime));
    game_start_battle(&g, 0);
    play_messages();
    CHECK(!battle_menu_enabled(&g, MENU_HERB));  // full health
    g.stats.hp = 5;
    int herbs = g.stats.herbs;
    pick(MENU_HERB);
    CHECK(has_msg(MSG_HERB));
    CHECK(g.stats.herbs == herbs - 1);
    CHECK(g.battle.msgs[0].amount == HERB_HEAL);
}

static void test_defend_rests(void)
{
    load(one_slime, COUNT(one_slime));
    game_start_battle(&g, 0);
    play_messages();
    g.stats.sp = 0;
    pick(MENU_DEFEND);
    CHECK(has_msg(MSG_DEFEND));
    CHECK(g.stats.sp == 3);  // 2 for defending, 1 at the end of the round
    // Halved damage: the slime's hits stay small.
    for (int i = 0; i < g.battle.msg_count; i++)
        if (g.battle.msgs[i].type == MSG_ENEMY_ATTACK) CHECK(g.battle.msgs[i].amount <= 3);
}

static void test_moving_the_cursor(void)
{
    load(one_slime, COUNT(one_slime));
    game_start_battle(&g, 0);
    play_messages();
    CHECK(run(&DOWN, 1) & EVENT_CURSOR);
    CHECK(g.battle.cursor == MENU_SKILLS);
    GameInput up = { .nav_y = -1 };
    run(&up, 2);
    CHECK(g.battle.cursor == MENU_FLEE);  // wraps around
}

static void test_winning_a_battle(void)
{
    load(one_slime, COUNT(one_slime));
    g.stats.atk = 99;
    game_start_battle(&g, 0);
    play_messages();
    pick(MENU_ATTACK);
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
    g.stats.hp = 3;
    game_start_battle(&g, 0);
    play_messages();
    pick(MENU_ATTACK);
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
        pick(MENU_FLEE);
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
    pick(MENU_FLEE);
    CHECK(has_msg(MSG_CANT_FLEE));
    CHECK(g.battle.outcome != OUTCOME_FLED);
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
    pick(MENU_DEFEND);  // even halved, a hit takes the last point
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
    pick(MENU_ATTACK);
    play_messages();
    CHECK(g.state == STATE_WON);
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
    "#######",
    "#P.e..#",
    "#..M.9#",
    "#######",
};

static void test_saving_the_goddess(void)
{
    load(goddess_map, COUNT(goddess_map));
    CHECK(g.foe_count == 1 && g.foes[0].kind == ENCOUNTER_EMBERS);
    float x = g.foes[0].x, y = g.foes[0].y;
    g.foes[0].think = 0;
    g.walker.safe_timer = 0;
    run(&NONE, 120);
    CHECK(g.foes[0].x == x && g.foes[0].y == y);  // they stay by her

    // Talking to her before she is safe does nothing.
    g.walker.x = 2 * TILE + 3;
    g.walker.y = 2 * TILE + 3;  // left of her
    face(1, 0);
    g.walker.safe_timer = 10;
    run(&CONFIRM, 1);
    CHECK(g.state == STATE_MAP);

    game_start_battle(&g, 0);
    CHECK(g.battle.enemy_count == 3 && g.battle.enemies[0].species == SPECIES_EMBER);
    play_messages();
    pick(MENU_FLEE);
    CHECK(has_msg(MSG_CANT_FLEE));
    play_messages();
    g.stats.atk = 99;
    g.stats.hp = g.stats.max_hp;
    while (g.state == STATE_BATTLE) {
        pick(MENU_ATTACK);
        if (g.battle.phase == PHASE_TARGET) run(&CONFIRM, 1);
        play_messages();
    }
    CHECK(g.state == STATE_SCENE && g.scene == SCENE_GODDESS);
    CHECK(g.goddess_saved && g.goddess.joined);
    CHECK(g.goddess.hp == g.goddess.max_hp && g.goddess.hp > 0);
    CHECK(game_tile_at(&g, 3, 2) == TILE_FLOOR);  // she left her spot
    CHECK(read_scene() == scene_page_count(SCENE_GODDESS));
    CHECK(g.state == STATE_MAP);

    // She follows him, on the tile he just left.
    place(1, 1);
    run(&RIGHT, 50);
    run(&NONE, 30);
    int hx = (int)(g.walker.x / TILE), hy = (int)(g.walker.y / TILE);
    CHECK(hx == 5 && hy == 1);  // stopped by the wall
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

static void test_goddess_casts_spells(void)
{
    start_party_battle();
    g.battle.enemies[0].hp = 999;
    pick(MENU_DEFEND);
    CHECK(g.battle.phase == PHASE_ALLY);  // her turn to choose
    pick(SPELL_BACK);
    CHECK(g.battle.phase == PHASE_MENU);  // he can change his mind
    pick(MENU_DEFEND);
    pick(SPELL_BEAM);
    CHECK(has_msg(MSG_BEAM));
    CHECK(g.goddess.mp <= 10 - SPELL_BEAM_COST + 1);
    CHECK(g.battle.enemies[0].hp < 999);
    play_messages();

    g.stats.hp = 5;
    g.goddess.hp = 5;
    pick(MENU_DEFEND);
    pick(SPELL_LIGHT);
    CHECK(has_msg(MSG_LIGHT));
    play_messages();
    CHECK(g.state == STATE_BATTLE);

    // Without magic left, the spells are closed.
    g.goddess.mp = 0;
    pick(MENU_DEFEND);
    CHECK(pick(SPELL_BEAM) & EVENT_DENIED);
    CHECK(g.battle.phase == PHASE_ALLY);
    pick(SPELL_DEFEND);
    CHECK(has_msg(MSG_ALLY_DEFEND));
}

static void test_monsters_attack_the_goddess_too(void)
{
    start_party_battle();
    g.battle.enemies[0].hp = 999;
    g.stats.hp = g.stats.max_hp = 999;
    bool hit_her = false, hit_him = false;
    for (int round = 0; round < 30 && g.goddess.hp > 0; round++) {
        pick(MENU_DEFEND);
        pick(SPELL_DEFEND);
        for (int i = 0; i < g.battle.msg_count; i++) {
            const BattleMsg *m = &g.battle.msgs[i];
            if (m->type == MSG_ENEMY_ATTACK) { if (m->on_ally) hit_her = true; else hit_him = true; }
        }
        play_messages();
    }
    CHECK(hit_her && hit_him);

    // When she falls, she sits out and gets back up after the battle.
    g.goddess.hp = 0;
    pick(MENU_ATTACK);
    CHECK(g.battle.phase != PHASE_ALLY);
    g.battle.enemies[0].hp = 0;
    g.stats.atk = 99;
    play_messages();
    while (g.state == STATE_BATTLE) {
        pick(MENU_ATTACK);
        play_messages();
    }
    CHECK(g.goddess.hp == 1);
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
        g.goddess.max_hp = g.goddess.hp = 24 + 5 * level;
        g.goddess.max_mp = g.goddess.mp = 8 + 2 * level;
        g.goddess.mag = 8 + 2 * level;
        g.goddess.def = 2 + level / 2;
    }
    game_start_battle(&g, 0);
    for (int round = 0; round < 100 && g.state == STATE_BATTLE; round++) {
        play_messages();
        if (g.state != STATE_BATTLE) break;
        const Stats *s = &g.stats;
        int alive = 0;
        for (int i = 0; i < g.battle.enemy_count; i++) alive += g.battle.enemies[i].hp > 0;
        if (s->hp * 3 < s->max_hp && battle_menu_enabled(&g, MENU_HERB)) {
            pick(MENU_HERB);
        } else if (alive > 1 && s->sp >= SKILL_SPIN_COST) {
            pick(MENU_SKILLS);
            pick(SKILL_SPIN);
        } else if (s->sp >= SKILL_SPLIT_COST) {
            pick(MENU_SKILLS);
            pick(SKILL_SPLIT);
        } else {
            pick(MENU_ATTACK);
        }
        if (g.battle.phase == PHASE_TARGET) run(&CONFIRM, 1);
        if (g.battle.phase == PHASE_ALLY) {
            // She heals when he is hurt, else casts her beam, else rests.
            if (s->hp * 2 < s->max_hp && battle_spell_enabled(&g, SPELL_LIGHT)) pick(SPELL_LIGHT);
            else if (battle_spell_enabled(&g, SPELL_BEAM)) pick(SPELL_BEAM);
            else pick(SPELL_DEFEND);
            if (g.battle.phase == PHASE_TARGET) run(&CONFIRM, 1);
        }
    }
    return !g.foes[0].alive && (g.state == STATE_MAP || g.state == STATE_SCENE || g.state == STATE_WON);
}

static bool auto_battle(Encounter kind, int level, int herbs, unsigned seed)
{
    return auto_battle_with(kind, level, herbs, seed, false);
}

static void test_balance(void)
{
    int wins;
    // The fire beings are the very first fight.
    wins = 0;
    for (unsigned i = 0; i < 20; i++) wins += auto_battle(ENCOUNTER_EMBERS, 1, 0, i);
    CHECK(wins >= 19);
    // A new lumberjack handles the first monsters.
    wins = 0;
    for (unsigned i = 0; i < 20; i++) wins += auto_battle(ENCOUNTER_SLIMES, 1, 0, i);
    CHECK(wins >= 18);
    // Wolves need a few levels.
    wins = 0;
    for (unsigned i = 0; i < 20; i++) wins += auto_battle(ENCOUNTER_WOLVES, 4, 2, i);
    CHECK(wins >= 15);
    // The boss is a real fight, but a prepared lumberjack wins.
    wins = 0;
    for (unsigned i = 0; i < 20; i++) wins += auto_battle(ENCOUNTER_BOSS, 7, 4, i);
    CHECK(wins >= 15);
    wins = 0;
    for (unsigned i = 0; i < 20; i++) wins += auto_battle(ENCOUNTER_BOSS, 1, 0, i);
    CHECK(wins == 0);
    // With the goddess, the boss is easier but still needs levels.
    wins = 0;
    for (unsigned i = 0; i < 20; i++) wins += auto_battle_with(ENCOUNTER_BOSS, 6, 3, i, true);
    CHECK(wins >= 17);
    wins = 0;
    for (unsigned i = 0; i < 20; i++) wins += auto_battle_with(ENCOUNTER_BOSS, 2, 0, i, true);
    CHECK(wins <= 2);
    wins = 0;
    for (unsigned i = 0; i < 20; i++) wins += auto_battle_with(ENCOUNTER_WOLVES, 3, 1, i, true);
    CHECK(wins >= 17);
}

int main(void)
{
    struct {
        const char *name;
        void (*fn)(void);
    } tests[] = {
        { "maps are valid and completable", test_maps_are_valid },
        { "scene can be skipped", test_scene_can_be_skipped },
        { "saving the goddess", test_saving_the_goddess },
        { "reaching the creatures plays their scene", test_reaching_the_creatures_plays_their_scene },
        { "story is complete", test_story_is_complete },
        { "goddess casts spells", test_goddess_casts_spells },
        { "monsters attack the goddess too", test_monsters_attack_the_goddess_too },
        { "bad map is rejected", test_bad_map_is_rejected },
        { "axe chops trees", test_axe_chops_trees },
        { "rocks are too hard", test_rocks_are_too_hard },
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
