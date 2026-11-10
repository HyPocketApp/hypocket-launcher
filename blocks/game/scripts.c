/* HyPocket Blocks: mods' scripts, in Lua (init.lua) or WebAssembly (*.wasm,
 * see scripts_wasm.c). Included by blocks.c after the survival code.
 *
 * Both get the same game, called "blocks":
 *
 *   Events (functions the script defines; Wasm exports them):
 *     on_start()                       the world is ready
 *     on_tick(dt)                      every frame while not paused (seconds)
 *     on_break(x, y, z, thing)         the player broke a block
 *     on_place(x, y, z, thing)         the player placed one
 *     on_eat(thing)                    the player ate something
 *     on_craft(thing, count)           the player made something
 *
 *   Calls (Lua: blocks.name(...); Wasm: imports from module "blocks"):
 *     get_block(x, y, z) -> thing      set_block(x, y, z, thing)
 *     player() -> x, y, z              teleport(x, y, z)
 *     count(thing) -> n                give(thing, n)        take(thing, n) -> taken
 *     health([v]) food([v]) warmth([v]) -> v   (0-10; with v, set it)
 *     time([v]) -> 0-1 (0.5 is noon)   day() -> days passed
 *     say(text)                        a message on the screen
 *     set(setting, value)              tune the base game (below)
 *     replace(system)                  turn a base system off, to make it in the script
 *
 *   Settings: "hunger" and "cold" (how fast food and warmth drop, 1 is
 *   normal), "walk" (speed, 1 is normal), "jump" (1 is normal).
 *   Systems: "hunger", "cold" (the script keeps food or warmth itself).
 *
 * In Lua things are names ("stone", "glow_stone"); in Wasm their numbers
 * (thing_id(name) gives one). The base game stays in C and runs as before
 * when no mod changes it.
 *
 * A script can change anything in the game, but nothing outside it: no
 * files, network, programs or other worlds. Lua gets no io, os, package or
 * debug, no loading of code or bytecode, a memory limit and a limit on how
 * long each call may run; Wasm runs in its own memory with the same calls. */

#include "third_party/lua/lua.h"
#include "third_party/lua/lauxlib.h"
#include "third_party/lua/lualib.h"

#define SCRIPT_MEMORY (32u << 20)      /* bytes per Lua mod */
#define SCRIPT_STEPS 2000000           /* Lua instructions per call */

/* What scripts change in the base game. */
static float script_hunger = 1, script_cold = 1, script_walk = 1, script_jump = 1;
static int script_no_hunger, script_no_cold;
/* The game's state the scripts reach (locals of blocks_main). */
static Player* script_player;
static float* script_health;
static void (*script_say)(const char* text);

/* ---- The calls, for both languages ---- */

static int api_get_block(int x, int y, int z) { return block_at(x, y, z); }

static void api_set_block(int x, int y, int z, int t) {
    if (t < 0 || t >= ITEM_BASE || (t >= BLOCK_TYPES && !is_mod_block(t))) return;
    if (x < 0 || z < 0 || x >= WX || z >= WZ || y < 1 || y >= WY) return;
    set_block(x, y, z, t);
    world_changed = 1;
}

static int api_count(int t) { return t >= 0 && t < THING_TYPES ? inventory[t] : 0; }

static void api_give(int t, int n) {
    if (t <= AIR || t >= THING_TYPES || (t >= BLOCK_TYPES && t < ITEM_BASE && !is_mod_block(t)) || n <= 0) return;
    give(t, n);
    world_changed = 1;
}

static int api_take(int t, int n) {
    if (t < 0 || t >= THING_TYPES || n <= 0) return 0;
    int taken = n < inventory[t] ? n : inventory[t];
    inventory[t] = (uint16_t)(inventory[t] - taken);
    world_changed = 1;
    return taken;
}

static float clamp10(float v) { return v < 0 ? 0 : v > 10 ? 10 : v; }

static void api_setting(const char* name, float v) {
    if (v < 0) v = 0;
    if (v > 20) v = 20;
    if (!strcmp(name, "hunger")) script_hunger = v;
    else if (!strcmp(name, "cold")) script_cold = v;
    else if (!strcmp(name, "walk")) script_walk = v;
    else if (!strcmp(name, "jump")) script_jump = v;
}

static void api_replace(const char* system) {
    if (!strcmp(system, "hunger")) script_no_hunger = 1;
    else if (!strcmp(system, "cold")) script_no_cold = 1;
}

static void api_say(const char* mod, const char* text) {
    printf("blocks: [%s] %s\n", mod, text);
    if (script_say) script_say(text);
}

/* ---- Lua ---- */

typedef struct {
    lua_State* L;
    char name[64];
    size_t memory;
    long steps;
    int broken; /* stopped after an error */
} LuaMod;

static LuaMod lua_mods[MAX_PACKS];
static int lua_count;

static void* lua_alloc(void* ud, void* ptr, size_t old, size_t size) {
    LuaMod* m = ud;
    if (!ptr) old = 0;
    if (size == 0) {
        m->memory -= old;
        free(ptr);
        return NULL;
    }
    if (m->memory - old + size > SCRIPT_MEMORY) return NULL;
    void* p = realloc(ptr, size);
    if (p) m->memory = m->memory - old + size;
    return p;
}

static void lua_count_hook(lua_State* L, lua_Debug* ar) {
    (void)ar;
    LuaMod* m;
    lua_getallocf(L, (void**)&m);
    if ((m->steps -= 10000) < 0) luaL_error(L, "the script took too long");
}

static LuaMod* lua_mod_of(lua_State* L) {
    LuaMod* m;
    lua_getallocf(L, (void**)&m);
    return m;
}

static int lua_thing(lua_State* L, int arg) {
    if (lua_type(L, arg) == LUA_TNUMBER) return (int)lua_tointeger(L, arg);
    int t = thing_by_name(luaL_checkstring(L, arg));
    if (t < 0) luaL_error(L, "no thing called %s", lua_tostring(L, arg));
    return t;
}

static int l_get_block(lua_State* L) {
    lua_pushstring(L, thing_name(api_get_block((int)luaL_checkinteger(L, 1), (int)luaL_checkinteger(L, 2),
                                               (int)luaL_checkinteger(L, 3))));
    return 1;
}

static int l_set_block(lua_State* L) {
    api_set_block((int)luaL_checkinteger(L, 1), (int)luaL_checkinteger(L, 2), (int)luaL_checkinteger(L, 3), lua_thing(L, 4));
    return 0;
}

static int l_player(lua_State* L) {
    lua_pushnumber(L, script_player ? script_player->x : 0);
    lua_pushnumber(L, script_player ? script_player->y : 0);
    lua_pushnumber(L, script_player ? script_player->z : 0);
    return 3;
}

static int l_teleport(lua_State* L) {
    if (!script_player) return 0;
    float x = (float)luaL_checknumber(L, 1), y = (float)luaL_checknumber(L, 2), z = (float)luaL_checknumber(L, 3);
    if (x < 0.5f || z < 0.5f || x > WX - 0.5f || z > WZ - 0.5f || y < 1 || y > WY) return 0;
    script_player->x = x;
    script_player->y = y;
    script_player->z = z;
    script_player->vy = 0;
    return 0;
}

static int l_count(lua_State* L) { lua_pushinteger(L, api_count(lua_thing(L, 1))); return 1; }
static int l_give(lua_State* L) { api_give(lua_thing(L, 1), (int)luaL_optinteger(L, 2, 1)); return 0; }
static int l_take(lua_State* L) { lua_pushinteger(L, api_take(lua_thing(L, 1), (int)luaL_optinteger(L, 2, 1))); return 1; }

static int l_meter(lua_State* L, float* value) {
    if (lua_gettop(L) >= 1 && value) *value = clamp10((float)luaL_checknumber(L, 1));
    lua_pushnumber(L, value ? *value : 0);
    return 1;
}
static int l_health(lua_State* L) { return l_meter(L, script_health); }
static int l_food(lua_State* L) { return l_meter(L, &food); }
static int l_warmth(lua_State* L) { return l_meter(L, &warmth); }

static int l_time(lua_State* L) {
    if (lua_gettop(L) >= 1) day_time = (float)fmod(fabs(luaL_checknumber(L, 1)), 1.0);
    lua_pushnumber(L, day_time);
    return 1;
}

static int l_day(lua_State* L) { lua_pushinteger(L, (lua_Integer)day_count); return 1; }
static int l_say(lua_State* L) { api_say(lua_mod_of(L)->name, luaL_checkstring(L, 1)); return 0; }
static int l_set(lua_State* L) { api_setting(luaL_checkstring(L, 1), (float)luaL_checknumber(L, 2)); return 0; }
static int l_replace(lua_State* L) { api_replace(luaL_checkstring(L, 1)); return 0; }

static int l_print(lua_State* L) {
    int n = lua_gettop(L); /* before the buffer, which takes a place on the stack */
    luaL_Buffer b;
    luaL_buffinit(L, &b);
    for (int i = 1; i <= n; i++) {
        if (i > 1) luaL_addchar(&b, '\t');
        luaL_tolstring(L, i, NULL);
        luaL_addvalue(&b);
    }
    luaL_pushresult(&b);
    printf("blocks: [%s] %s\n", lua_mod_of(L)->name, lua_tostring(L, -1));
    return 0;
}

static const luaL_Reg lua_api[] = {
    {"get_block", l_get_block}, {"set_block", l_set_block}, {"player", l_player}, {"teleport", l_teleport},
    {"count", l_count}, {"give", l_give}, {"take", l_take}, {"health", l_health}, {"food", l_food},
    {"warmth", l_warmth}, {"time", l_time}, {"day", l_day}, {"say", l_say}, {"set", l_set},
    {"replace", l_replace}, {NULL, NULL}};

/* Calls a function the script may define; nargs arguments are on the stack. */
static void lua_event(LuaMod* m, const char* event, int nargs) {
    lua_State* L = m->L;
    if (m->broken) {
        lua_pop(L, nargs);
        return;
    }
    if (lua_getglobal(L, event) != LUA_TFUNCTION) {
        lua_pop(L, nargs + 1);
        return;
    }
    lua_insert(L, -(nargs + 1));
    m->steps = SCRIPT_STEPS;
    if (lua_pcall(L, nargs, 0, 0) != LUA_OK) {
        printf("blocks: [%s] %s stopped: %s\n", m->name, event, lua_tostring(L, -1));
        lua_pop(L, 1);
        m->broken = 1;
        if (script_say) {
            char text[160];
            snprintf(text, sizeof text, "%s: error", m->name);
            script_say(text);
        }
    }
}

static void lua_load_mod(const char* pack) {
    char path[1200];
    snprintf(path, sizeof path, "%s/init.lua", pack);
    FILE* f = fopen(path, "rb");
    if (!f || lua_count >= MAX_PACKS) {
        if (f) fclose(f);
        return;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* code = malloc((size_t)size + 1);
    size_t got = fread(code, 1, (size_t)size, f);
    fclose(f);
    LuaMod* m = &lua_mods[lua_count];
    memset(m, 0, sizeof *m);
    const char* base = strrchr(pack, '/');
    snprintf(m->name, sizeof m->name, "%s", base ? base + 1 : pack);
    m->L = lua_newstate(lua_alloc, m);
    if (!m->L) {
        free(code);
        return;
    }
    lua_State* L = m->L;
    /* The safe libraries only, and nothing that loads code or touches files. */
    static const luaL_Reg libs[] = {{LUA_GNAME, luaopen_base}, {LUA_TABLIBNAME, luaopen_table},
                                    {LUA_STRLIBNAME, luaopen_string}, {LUA_MATHLIBNAME, luaopen_math},
                                    {LUA_UTF8LIBNAME, luaopen_utf8}, {LUA_COLIBNAME, luaopen_coroutine}};
    for (size_t i = 0; i < sizeof libs / sizeof libs[0]; i++) {
        luaL_requiref(L, libs[i].name, libs[i].func, 1);
        lua_pop(L, 1);
    }
    static const char* const removed[] = {"dofile", "loadfile", "load", "require", "collectgarbage"};
    for (size_t i = 0; i < sizeof removed / sizeof removed[0]; i++) {
        lua_pushnil(L);
        lua_setglobal(L, removed[i]);
    }
    lua_getglobal(L, "string");
    lua_pushnil(L);
    lua_setfield(L, -2, "dump");
    lua_pop(L, 1);
    lua_pushcfunction(L, l_print);
    lua_setglobal(L, "print");
    luaL_newlib(L, lua_api);
    lua_setglobal(L, "blocks");
    lua_sethook(L, lua_count_hook, LUA_MASKCOUNT, 10000);
    m->steps = SCRIPT_STEPS;
    if (luaL_loadbufferx(L, code, got, m->name, "t") != LUA_OK || lua_pcall(L, 0, 0, 0) != LUA_OK) {
        printf("blocks: [%s] init.lua: %s\n", m->name, lua_tostring(L, -1));
        lua_close(L);
        m->L = NULL;
    } else {
        lua_count++;
    }
    free(code);
}

/* ---- Events, from the game ---- */

#include "scripts_wasm.c"

static void scripts_load(void) {
    for (int i = 0; i < pack_count; i++) {
        lua_load_mod(packs[i]);
        wasm_load_mod(packs[i]);
    }
    if (lua_count || wasm_count) printf("blocks: %d Lua and %d WebAssembly scripts\n", lua_count, wasm_count);
}

static void scripts_start(void) {
    for (int i = 0; i < lua_count; i++) lua_event(&lua_mods[i], "on_start", 0);
    wasm_event("on_start", 0, 0, 0, 0, 0);
}

static void scripts_tick(float dt) {
    for (int i = 0; i < lua_count; i++) {
        lua_pushnumber(lua_mods[i].L, dt);
        lua_event(&lua_mods[i], "on_tick", 1);
    }
    wasm_event_f("on_tick", dt);
}

static void scripts_block(const char* event, int x, int y, int z, int thing) {
    for (int i = 0; i < lua_count; i++) {
        lua_State* L = lua_mods[i].L;
        lua_pushinteger(L, x);
        lua_pushinteger(L, y);
        lua_pushinteger(L, z);
        lua_pushstring(L, thing_name(thing));
        lua_event(&lua_mods[i], event, 4);
    }
    wasm_event(event, 4, x, y, z, thing);
}

static void scripts_thing(const char* event, int thing, int count) {
    for (int i = 0; i < lua_count; i++) {
        lua_State* L = lua_mods[i].L;
        lua_pushstring(L, thing_name(thing));
        lua_pushinteger(L, count);
        lua_event(&lua_mods[i], event, 2);
    }
    wasm_event(event, 2, thing, count, 0, 0);
}
