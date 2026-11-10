/* HyPocket Blocks: mods' scripts compiled to WebAssembly, from any language
 * that makes it (C, C++, Rust, Zig, AssemblyScript...). Included by
 * scripts.c, which documents the events and calls; here they are imports
 * from the module "blocks" and exports of the mod's .wasm file:
 *
 *   imports: get_block(i32 x, i32 y, i32 z) -> i32     set_block(i32, i32, i32, i32 thing)
 *            player_x() -> f32   player_y() -> f32   player_z() -> f32   teleport(f32, f32, f32)
 *            count(i32 thing) -> i32   give(i32 thing, i32 n)   take(i32 thing, i32 n) -> i32
 *            health() food() warmth() -> f32   set_health(f32) set_food(f32) set_warmth(f32)
 *            time() -> f32   set_time(f32)   day() -> i32
 *            say(i32 text, i32 length)                  text is UTF-8 in the module's memory
 *            set(i32 name, i32 length, f32 value)       replace(i32 name, i32 length)
 *            thing_id(i32 name, i32 length) -> i32      -1 when there is no such thing
 *   exports: on_start()  on_tick(f32 dt)  on_break/on_place(i32 x, i32 y, i32 z, i32 thing)
 *            on_eat/on_craft(i32 thing, i32 count)      any of them, or none
 *
 * Every import is optional. Things are numbers here: thing_id("stone").
 * wasm3 (third_party/wasm3, MIT) runs it, interpreted, inside its own
 * memory (at most 64 MB): a module reaches nothing but these calls. Unlike
 * Lua, a call is not cut short if it never returns. */

#include "third_party/wasm3/wasm3.h"
#include "third_party/wasm3/m3_env.h"

#define WASM_MEMORY (64u << 20)

typedef struct {
    IM3Environment env;
    IM3Runtime runtime;
    IM3Module module;
    uint8_t* bytes;
    char name[64];
    int broken;
} WasmMod;

static WasmMod wasm_mods[MAX_PACKS];
static int wasm_count;

static const char* wasm_string(IM3Runtime runtime, uint32_t ptr, uint32_t len, char* out, size_t size) {
    uint32_t memory_size = 0;
    uint8_t* memory = m3_GetMemory(runtime, &memory_size, 0);
    if (!memory || (uint64_t)ptr + len > memory_size) return NULL;
    size_t n = len < size - 1 ? len : size - 1;
    memcpy(out, memory + ptr, n);
    out[n] = 0;
    return out;
}

static WasmMod* wasm_mod_of(IM3Runtime runtime) { return m3_GetUserData(runtime); }

m3ApiRawFunction(w_get_block) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t, x);
    m3ApiGetArg(int32_t, y);
    m3ApiGetArg(int32_t, z);
    m3ApiReturn(api_get_block(x, y, z));
}

m3ApiRawFunction(w_set_block) {
    m3ApiGetArg(int32_t, x);
    m3ApiGetArg(int32_t, y);
    m3ApiGetArg(int32_t, z);
    m3ApiGetArg(int32_t, t);
    api_set_block(x, y, z, t);
    m3ApiSuccess();
}

m3ApiRawFunction(w_player_x) { m3ApiReturnType(float); m3ApiReturn(script_player ? script_player->x : 0); }
m3ApiRawFunction(w_player_y) { m3ApiReturnType(float); m3ApiReturn(script_player ? script_player->y : 0); }
m3ApiRawFunction(w_player_z) { m3ApiReturnType(float); m3ApiReturn(script_player ? script_player->z : 0); }

m3ApiRawFunction(w_teleport) {
    m3ApiGetArg(float, x);
    m3ApiGetArg(float, y);
    m3ApiGetArg(float, z);
    if (script_player && x >= 0.5f && z >= 0.5f && x <= WX - 0.5f && z <= WZ - 0.5f && y >= 1 && y <= WY) {
        script_player->x = x;
        script_player->y = y;
        script_player->z = z;
        script_player->vy = 0;
    }
    m3ApiSuccess();
}

m3ApiRawFunction(w_count) { m3ApiReturnType(int32_t); m3ApiGetArg(int32_t, t); m3ApiReturn(api_count(t)); }

m3ApiRawFunction(w_give) {
    m3ApiGetArg(int32_t, t);
    m3ApiGetArg(int32_t, n);
    api_give(t, n);
    m3ApiSuccess();
}

m3ApiRawFunction(w_take) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t, t);
    m3ApiGetArg(int32_t, n);
    m3ApiReturn(api_take(t, n));
}

m3ApiRawFunction(w_health) { m3ApiReturnType(float); m3ApiReturn(script_health ? *script_health : 0); }
m3ApiRawFunction(w_food) { m3ApiReturnType(float); m3ApiReturn(food); }
m3ApiRawFunction(w_warmth) { m3ApiReturnType(float); m3ApiReturn(warmth); }
m3ApiRawFunction(w_set_health) { m3ApiGetArg(float, v); if (script_health) *script_health = clamp10(v); m3ApiSuccess(); }
m3ApiRawFunction(w_set_food) { m3ApiGetArg(float, v); food = clamp10(v); m3ApiSuccess(); }
m3ApiRawFunction(w_set_warmth) { m3ApiGetArg(float, v); warmth = clamp10(v); m3ApiSuccess(); }
m3ApiRawFunction(w_time) { m3ApiReturnType(float); m3ApiReturn(day_time); }
m3ApiRawFunction(w_set_time) { m3ApiGetArg(float, v); day_time = fmodf(fabsf(v), 1.0f); m3ApiSuccess(); }
m3ApiRawFunction(w_day) { m3ApiReturnType(int32_t); m3ApiReturn((int32_t)day_count); }

m3ApiRawFunction(w_say) {
    m3ApiGetArg(uint32_t, ptr);
    m3ApiGetArg(uint32_t, len);
    char text[256];
    if (wasm_string(runtime, ptr, len, text, sizeof text)) api_say(wasm_mod_of(runtime)->name, text);
    m3ApiSuccess();
}

m3ApiRawFunction(w_set) {
    m3ApiGetArg(uint32_t, ptr);
    m3ApiGetArg(uint32_t, len);
    m3ApiGetArg(float, v);
    char name[64];
    if (wasm_string(runtime, ptr, len, name, sizeof name)) api_setting(name, v);
    m3ApiSuccess();
}

m3ApiRawFunction(w_replace) {
    m3ApiGetArg(uint32_t, ptr);
    m3ApiGetArg(uint32_t, len);
    char name[64];
    if (wasm_string(runtime, ptr, len, name, sizeof name)) api_replace(name);
    m3ApiSuccess();
}

m3ApiRawFunction(w_thing_id) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(uint32_t, ptr);
    m3ApiGetArg(uint32_t, len);
    char name[64];
    m3ApiReturn(wasm_string(runtime, ptr, len, name, sizeof name) ? thing_by_name(name) : -1);
}

static void wasm_link(IM3Module module) {
    static const struct { const char* name; const char* sig; M3RawCall call; } imports[] = {
        {"get_block", "i(iii)", w_get_block}, {"set_block", "v(iiii)", w_set_block},
        {"player_x", "f()", w_player_x}, {"player_y", "f()", w_player_y}, {"player_z", "f()", w_player_z},
        {"teleport", "v(fff)", w_teleport}, {"count", "i(i)", w_count}, {"give", "v(ii)", w_give},
        {"take", "i(ii)", w_take}, {"health", "f()", w_health}, {"food", "f()", w_food},
        {"warmth", "f()", w_warmth}, {"set_health", "v(f)", w_set_health}, {"set_food", "v(f)", w_set_food},
        {"set_warmth", "v(f)", w_set_warmth}, {"time", "f()", w_time}, {"set_time", "v(f)", w_set_time},
        {"day", "i()", w_day}, {"say", "v(ii)", w_say}, {"set", "v(iif)", w_set}, {"replace", "v(ii)", w_replace},
        {"thing_id", "i(ii)", w_thing_id}};
    for (size_t i = 0; i < sizeof imports / sizeof imports[0]; i++)
        m3_LinkRawFunction(module, "blocks", imports[i].name, imports[i].sig, imports[i].call); /* unused: fine */
}

static void wasm_load_file(const char* pack, const char* file) {
    if (wasm_count >= MAX_PACKS) return;
    char path[2200];
    snprintf(path, sizeof path, "%s/%s", pack, file);
    FILE* f = fopen(path, "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 8 || size > (16 << 20)) {
        fclose(f);
        return;
    }
    WasmMod* m = &wasm_mods[wasm_count];
    memset(m, 0, sizeof *m);
    const char* base = strrchr(pack, '/');
    snprintf(m->name, sizeof m->name, "%s", base ? base + 1 : pack);
    m->bytes = malloc((size_t)size);
    size_t got = fread(m->bytes, 1, (size_t)size, f);
    fclose(f);
    m->env = m3_NewEnvironment();
    m->runtime = m3_NewRuntime(m->env, 64 * 1024, m);
    M3Result error = m->runtime ? NULL : "no memory";
    if (!error) {
        m->runtime->memoryLimit = WASM_MEMORY;
        error = m3_ParseModule(m->env, &m->module, m->bytes, (uint32_t)got);
    }
    if (!error) error = m3_LoadModule(m->runtime, m->module);
    if (!error) {
        wasm_link(m->module);
        error = m3_CompileModule(m->module);
    }
    if (error) {
        printf("blocks: [%s] %s: %s\n", m->name, file, error);
        if (m->runtime) m3_FreeRuntime(m->runtime);
        m3_FreeEnvironment(m->env);
        free(m->bytes);
        return;
    }
    /* A start function, if the module has one, ran when it was loaded. */
    wasm_count++;
}

static void wasm_load_mod(const char* pack) {
    DIR* d = opendir(pack);
    if (!d) return;
    char names[8][256];
    int n = 0;
    struct dirent* e;
    while ((e = readdir(d)) && n < 8) {
        size_t len = strlen(e->d_name);
        if (len > 5 && !strcmp(e->d_name + len - 5, ".wasm")) snprintf(names[n++], sizeof names[0], "%s", e->d_name);
    }
    closedir(d);
    qsort(names, (size_t)n, sizeof names[0], pack_name_cmp);
    for (int i = 0; i < n; i++) wasm_load_file(pack, names[i]);
}

static void wasm_failed(WasmMod* m, const char* event, M3Result error) {
    M3ErrorInfo info;
    m3_GetErrorInfo(m->runtime, &info);
    printf("blocks: [%s] %s stopped: %s %s\n", m->name, event, error, info.message ? info.message : "");
    m->broken = 1;
    if (script_say) {
        char text[160];
        snprintf(text, sizeof text, "%s: error", m->name);
        script_say(text);
    }
}

/* Calls an event with up to four i32 arguments, if the module exports it. */
static void wasm_event(const char* event, int nargs, int a, int b, int c, int d) {
    for (int i = 0; i < wasm_count; i++) {
        WasmMod* m = &wasm_mods[i];
        IM3Function f;
        if (m->broken || m3_FindFunction(&f, m->runtime, event) != NULL) continue;
        if ((int)m3_GetArgCount(f) != nargs) continue;
        int32_t args[4] = {a, b, c, d};
        const void* ptrs[4] = {&args[0], &args[1], &args[2], &args[3]};
        M3Result error = m3_Call(f, (uint32_t)nargs, ptrs);
        if (error) wasm_failed(m, event, error);
    }
}

static void wasm_event_f(const char* event, float v) {
    for (int i = 0; i < wasm_count; i++) {
        WasmMod* m = &wasm_mods[i];
        IM3Function f;
        if (m->broken || m3_FindFunction(&f, m->runtime, event) != NULL || m3_GetArgCount(f) != 1) continue;
        const void* ptrs[1] = {&v};
        M3Result error = m3_Call(f, 1, ptrs);
        if (error) wasm_failed(m, event, error);
    }
}
