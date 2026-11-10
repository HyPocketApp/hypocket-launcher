/* The WebAssembly example mod's script, in C. Any language that makes a
 * .wasm file works the same way: import the calls from the module
 * "blocks" and export the events you want (see blocks/game/scripts_wasm.c).
 *
 * Build (clang with the wasm32 target, as in the Android NDK):
 *   clang --target=wasm32 -O2 -nostdlib -Wl,--no-entry -Wl,--export-dynamic \
 *       -Wl,--allow-undefined -o ../script.wasm script.c */

#define IMPORT(name) __attribute__((import_module("blocks"), import_name(#name)))
#define EXPORT __attribute__((visibility("default")))

IMPORT(say) void say(const char* text, int length);
IMPORT(thing_id) int thing_id(const char* name, int length);
IMPORT(give) void give(int thing, int count);

static int length(const char* s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

static void tell(const char* text) { say(text, length(text)); }
static int thing(const char* name) { return thing_id(name, length(name)); }

static int stone, flint, stones_broken;

EXPORT void on_start(void) {
    stone = thing("stone");
    flint = thing("flint");
    tell("WebAssembly example: every fifth stone you break gives a flint");
}

EXPORT void on_break(int x, int y, int z, int what) {
    (void)x; (void)y; (void)z;
    if (what == stone && ++stones_broken % 5 == 0) {
        give(flint, 1);
        tell("A flint came out of the stone");
    }
}
