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
static const GameInput LEFT = { .move_x = -1 };
static const GameInput CONFIRM = { .confirm_pressed = true };
static const GameInput CANCEL = { .cancel_pressed = true };
static const GameInput DOWN = { .nav_y = 1 };

// A fresh level-1 lumberjack on a test map.
static void load(const char *const *rows, int count)
{
    memset(&g, 0, sizeof g);
    game_new(&g);  // stats, and map 1 which we replace now
    CHECK(level_load_rows(&g, rows, count));
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
            if (t == TILE_CHEST || t == TILE_CHEST_AXE || t == TILE_FIRE) {
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

static void test_maps_are_valid(void)
{
    CHECK(level_count() >= 3);
    for (int i = 0; i < level_count(); i++) {
        int count = 0;
        const char *const *rows = level_rows(i, &count);
        memset(&g, 0, sizeof g);
        CHECK(level_load_rows(&g, rows, count));
        CHECK(g.foe_count > 0);
        for (int r = 0; r < count; r++) {
            CHECK((int)strlen(rows[r]) == g.w);
            CHECK(rows[r][0] == '#' && rows[r][g.w - 1] == '#');
        }
        for (int x = 0; x < g.w; x++) CHECK(rows[0][x] == '#' && rows[count - 1][x] == '#');
        CHECK(map_is_completable(&g));
    }
    // The boss waits on the last map.
    int count = 0;
    memset(&g, 0, sizeof g);
    const char *const *last = level_rows(level_count() - 1, &count);
    level_load_rows(&g, last, count);
    bool boss = false;
    for (int i = 0; i < g.foe_count; i++) boss |= g.foes[i].kind == ENCOUNTER_BOSS;
    CHECK(boss);
}

static void test_bad_map_is_rejected(void)
{
    static const char *const no_end[] = { "#####", "#P..#", "#####" };
    memset(&g, 0, sizeof g);
    CHECK(!level_load_rows(&g, no_end, 3));
}

// --- Exploring ----------------------------------------------------------------------

static void test_axe_chops_trees(void)
{
    static const char *const rows[] = { "#######", "#P.T.X#", "#######" };
    load(rows, COUNT(rows));
    run(&RIGHT, 60);
    CHECK(g.walker.x + WALKER_SIZE <= 3 * TILE + 0.01f);  // the tree stops him
    face(1, 0);
    CHECK(run(&CONFIRM, 1) & EVENT_CHOP);
    CHECK(game_tile_at(&g, 3, 1) == TILE_STUMP);
    CHECK(run(&RIGHT, 120) & EVENT_EXIT);
}

static void test_rocks_are_too_hard(void)
{
    static const char *const rows[] = { "#######", "#PR..X#", "#######" };
    load(rows, COUNT(rows));
    face(1, 0);
    CHECK(run(&CONFIRM, 1) & EVENT_BUMP);
    CHECK(game_tile_at(&g, 2, 1) == TILE_ROCK);
    CHECK(g.toast == TOAST_TOO_HARD);
}

static void test_chests(void)
{
    static const char *const rows[] = { "#######", "#cPA.X#", "#######" };
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
    static const char *const rows[] = { "#######", "#..P.X#", "#..f..#", "#######" };
    load(rows, COUNT(rows));
    g.stats.hp = 1;
    g.stats.sp = 0;
    run(&LEFT, 5);
    face(0, 1);
    CHECK(run(&CONFIRM, 1) & EVENT_REST);
    CHECK(g.stats.hp == g.stats.max_hp && g.stats.sp == g.stats.max_sp);
    CHECK(g.respawn_x == g.walker.x);
}

static void test_exit_leads_to_next_map(void)
{
    static const char *const rows[] = { "######", "#P.X.#", "######" };
    load(rows, COUNT(rows));
    g.level = 0;
    run(&RIGHT, 60);
    CHECK(g.level == 1);
    CHECK(g.state == STATE_MAP);
    CHECK(g.toast == TOAST_MAP);
}

static void test_pause(void)
{
    static const char *const rows[] = { "######", "#P..X#", "######" };
    load(rows, COUNT(rows));
    GameInput pause = { .pause_pressed = true };
    run(&pause, 1);
    CHECK(g.state == STATE_PAUSED);
    float x = g.walker.x;
    run(&RIGHT, 30);
    CHECK(g.walker.x == x);
    run(&pause, 1);
    CHECK(g.state == STATE_MAP);
}

// --- Battles ------------------------------------------------------------------------

static const char *const one_slime[] = { "#######", "#P.s.X#", "#######" };
static const char *const two_slimes[] = { "#######", "#P.g.X#", "#######" };

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

static void test_title_starts_game(void)
{
    game_init(&g);
    CHECK(g.state == STATE_TITLE);
    run(&CONFIRM, 1);
    CHECK(g.state == STATE_TITLE);  // too soon after the screen appeared
    run(&NONE, 30);
    run(&CONFIRM, 1);
    CHECK(g.state == STATE_MAP);
    CHECK(g.level == 0 && g.stats.level == 1);
}

// --- Balance --------------------------------------------------------------------------

// Fights a whole battle with a simple strategy: a herb below a third of the
// health, the strongest skill there is stamina for, else a plain attack.
// Returns true on a win.
static bool auto_battle(Encounter kind, int level, int herbs, unsigned seed)
{
    static const char *const rows[] = { "######", "#P.s.#", "#...X#", "######" };
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
    }
    return (g.state == STATE_MAP && !g.foes[0].alive) || g.state == STATE_WON;
}

static void test_balance(void)
{
    int wins;
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
}

int main(void)
{
    struct {
        const char *name;
        void (*fn)(void);
    } tests[] = {
        { "maps are valid and completable", test_maps_are_valid },
        { "bad map is rejected", test_bad_map_is_rejected },
        { "axe chops trees", test_axe_chops_trees },
        { "rocks are too hard", test_rocks_are_too_hard },
        { "chests", test_chests },
        { "campfire heals and saves", test_campfire_heals_and_saves },
        { "exit leads to next map", test_exit_leads_to_next_map },
        { "pause", test_pause },
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
