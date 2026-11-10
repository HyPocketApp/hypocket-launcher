/* HyPocket Blocks: mods. Included by blocks.c, after the things and the
 * palette and before what they affect.
 *
 * Mods live in the world: a folder beside the save, <save>.mods/ (or
 * HYPOCKET_BLOCKS_MODS), holds one folder per mod, loaded in name order.
 * The base game stays in C; a mod adds to it and replaces parts of it:
 *
 *   things.txt    new blocks and items (see below)
 *   recipes.txt   new recipes, and base ones taken away
 *   textures/     16x16 PNG pictures for its blocks
 *   sounds/       Ogg files that replace or add sounds (sfx/<name>.ogg ...)
 *   init.lua      a script (scripts.c)
 *   *.wasm        compiled scripts, in any language (scripts.c)
 *
 * things.txt:
 *     [block glow_stone]          or [item cheese]
 *     name = Glowing stone
 *     name.pt = Pedra brilhante
 *     color = #E0D070             the colour in the hotbar and inventory
 *     accent = #A08020
 *     texture = textures/glow.png (or texture.top, texture.side, texture.bottom)
 *     light = 1                   gives light like a lamp
 *     transparent = 1             see-through (glass, leaves)
 *     solid = 0                   walked through
 *     blows = 3                   blows by hand (0: cannot be broken by hand)
 *     tool = pick                 a thing that breaks it in tool_blows blows
 *     tool_blows = 1
 *     drops = stone 2             what breaking it gives (else itself)
 *     kind = food                 items: material, tool, food, clothing
 *     food = 3                    how much eating it fills (out of 10)
 *
 * recipes.txt, one per line ("#" starts a comment):
 *     stick 2, flint 1 -> knife 1
 *     wood 1 -> planks 4 | tool saw
 *     sand 1 -> glass 1 | near kiln
 *     remove planks               the base recipes that make planks
 *
 * Things are named in lower case: the base ones by their names in
 * BlocksPalette.kt (planks, raw_meat...) and grass, water, air too. A
 * new thing keeps its number in the world for good: ids.txt in the mods
 * folder remembers them. */

/* Paths are cut to their buffers on purpose: a longer one is simply not found. */
#if !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#endif

#define MAX_PALETTE 48
#define MAX_PACKS 32
#define MAX_MOD_LAYERS 64
#define MAX_RECIPES 96

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_HDR
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include "third_party/stb_image.h"
#pragma GCC diagnostic pop
#include <dirent.h>
#include "generated.h"

typedef struct {
    int used;
    char name[32];
    char label[2][48];       /* English, Portuguese */
    uint32_t color, accent;  /* 0xAARRGGBB */
    int kind;                /* THING_* */
    int layer[3];            /* texture layers: top, side, bottom */
    int light, transparent, solid;
    int blows, tool, tool_blows;
    int drops, drops_count;
    float food;
} ModThing;

static ModThing mod_things[THING_TYPES];
static uint8_t mod_pixels[MAX_MOD_LAYERS][16][16][4];
static int mod_layers;
static char packs[MAX_PACKS][1024];
static int pack_count;
static char mods_dir[1024];

static UiRecipe recipes[MAX_RECIPES]; /* the base ones left, then the mods' */
static int recipe_count;

static int is_mod_thing(int t) { return t >= 0 && t < THING_TYPES && mod_things[t].used; }
static int is_mod_block(int t) { return t >= BLOCK_TYPES && t < ITEM_BASE && mod_things[t].used; }

/* Names of the base blocks that are not carried, for packs and scripts. */
static const char* const block_names[BLOCK_TYPES] = {
    "air", "grass", "dirt", "stone", "sand", "wood", "leaves", "water", "planks", "stone_bricks", "bricks",
    "glass", "roof", "plaster", "lamp", "beacon_off", "beacon_on", "shade", "kiln", "stove", "campfire"};

/* A thing by name, or -1. */
static int thing_by_name(const char* name) {
    for (int t = 0; t < BLOCK_TYPES; t++)
        if (!strcmp(block_names[t], name)) return t;
    for (int p = 0; p < UI_THINGS; p++)
        if (!strcmp(ui_thing_names[p], name)) return palette[p];
    for (int t = 0; t < THING_TYPES; t++)
        if (mod_things[t].used && !strcmp(mod_things[t].name, name)) return t;
    return -1;
}

__attribute__((unused)) static const char* thing_name(int t) {
    if (t >= 0 && t < BLOCK_TYPES) return block_names[t];
    if (is_mod_thing(t)) return mod_things[t].name;
    int p = palette_index(t);
    return p >= 0 && p < UI_THINGS ? ui_thing_names[p] : "?";
}

static char* trim(char* s) {
    while (*s == ' ' || *s == '\t') s++;
    char* e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) *--e = 0;
    return s;
}

static uint32_t parse_color(const char* v) {
    if (*v == '#') v++;
    unsigned rgb = (unsigned)strtoul(v, NULL, 16);
    return strlen(v) > 6 ? rgb : 0xFF000000u | rgb;
}

/* ---- Ids that stay ---- */

static char id_names[THING_TYPES][32];

static void load_ids(void) {
    char path[1200];
    snprintf(path, sizeof path, "%s/ids.txt", mods_dir);
    FILE* f = fopen(path, "r");
    char name[64];
    int id;
    while (f && fscanf(f, "%63s %d", name, &id) == 2)
        if (id >= 0 && id < THING_TYPES) snprintf(id_names[id], sizeof id_names[id], "%s", name);
    if (f) fclose(f);
}

static void save_ids(void) {
    char path[1200];
    snprintf(path, sizeof path, "%s/ids.txt", mods_dir);
    FILE* f = fopen(path, "w");
    if (!f) return;
    for (int t = 0; t < THING_TYPES; t++)
        if (id_names[t][0]) fprintf(f, "%s %d\n", id_names[t], t);
    fclose(f);
}

/* The number a new thing keeps: the one it had, or the first free one. */
static int new_thing_id(const char* name, int block) {
    for (int t = 0; t < THING_TYPES; t++)
        if (!strcmp(id_names[t], name)) return t;
    int from = block ? BLOCK_TYPES : ITEM_END, to = block ? ITEM_BASE : THING_TYPES;
    for (int t = from; t < to; t++)
        if (!id_names[t][0]) {
            snprintf(id_names[t], sizeof id_names[t], "%s", name);
            return t;
        }
    return -1;
}

/* ---- Textures ---- */

/* A 16x16 layer from a PNG (scaled to 16x16 if it is another size), or a
 * plain one from the colour, with a little noise like the base blocks. */
static int mod_layer(const char* pack, const char* file, uint32_t color) {
    if (mod_layers >= MAX_MOD_LAYERS) return -1;
    uint8_t (*px)[16][4] = mod_pixels[mod_layers];
    int w = 0, h = 0, n = 0;
    unsigned char* data = NULL;
    if (file) {
        char path[2200];
        snprintf(path, sizeof path, "%s/%s", pack, file);
        data = stbi_load(path, &w, &h, &n, 4);
        if (!data) printf("blocks: mod texture %s could not be read\n", path);
    }
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++) {
            if (data) {
                const unsigned char* s = data + ((size_t)(y * h / 16) * w + (size_t)(x * w / 16)) * 4;
                memcpy(px[y][x], s, 4);
            } else {
                float shade = 0.85f + 0.15f * rand01(x, y, mod_layers * 7 + 3);
                px[y][x][0] = (uint8_t)(((color >> 16) & 255) * shade);
                px[y][x][1] = (uint8_t)(((color >> 8) & 255) * shade);
                px[y][x][2] = (uint8_t)((color & 255) * shade);
                px[y][x][3] = (uint8_t)(color >> 24);
            }
        }
    stbi_image_free(data);
    return LAYERS + mod_layers++;
}

/* ---- things.txt ---- */

static void add_to_palette(int t) {
    if (palette_index(t) < 0 && palette_size < MAX_PALETTE) palette[palette_size++] = t;
}

static void load_things(const char* pack) {
    char path[1200], line[512];
    snprintf(path, sizeof path, "%s/things.txt", pack);
    FILE* f = fopen(path, "r");
    if (!f) return;
    ModThing* m = NULL;
    char textures[3][256] = {"", "", ""};
    int number = 0;
    for (;;) {
        char* got = fgets(line, sizeof line, f);
        /* A thing ends where the next starts, or at the end of the file. */
        if ((!got || trim(line)[0] == '[') && m) {
            for (int k = 0; k < 3 && m->kind == THING_BLOCK; k++)
                m->layer[k] = mod_layer(pack, textures[k][0] ? textures[k] : textures[1][0] ? textures[1] : NULL,
                                        k == 0 ? m->color : m->accent ? m->accent : m->color);
            add_to_palette((int)(m - mod_things));
            m = NULL;
        }
        if (!got) break;
        number++;
        char* s = trim(line);
        if (!*s || *s == '#') continue;
        if (*s == '[') {
            char kind[16], name[32];
            if (sscanf(s, "[%15s %31[^]]]", kind, name) != 2) {
                printf("blocks: %s line %d: expected [block name] or [item name]\n", path, number);
                continue;
            }
            int block = !strcmp(kind, "block");
            int t = thing_by_name(name) >= 0 ? -1 : new_thing_id(name, block);
            if (t < 0) {
                printf("blocks: %s: %s is taken or there is no room for it\n", path, name);
                continue;
            }
            m = &mod_things[t];
            memset(m, 0, sizeof *m);
            m->used = 1;
            snprintf(m->name, sizeof m->name, "%s", name);
            snprintf(m->label[0], sizeof m->label[0], "%s", name);
            snprintf(m->label[1], sizeof m->label[1], "%s", name);
            m->kind = block ? THING_BLOCK : THING_MATERIAL;
            m->color = 0xFF9A9A9A;
            m->solid = 1;
            m->blows = 1;
            m->drops = t;
            m->drops_count = 1;
            textures[0][0] = textures[1][0] = textures[2][0] = 0;
            continue;
        }
        if (!m) continue;
        char* eq = strchr(s, '=');
        if (!eq) continue;
        *eq = 0;
        char *key = trim(s), *value = trim(eq + 1);
        if (!strcmp(key, "name")) snprintf(m->label[0], sizeof m->label[0], "%s", value);
        else if (!strcmp(key, "name.pt")) snprintf(m->label[1], sizeof m->label[1], "%s", value);
        else if (!strcmp(key, "color")) m->color = parse_color(value);
        else if (!strcmp(key, "accent")) m->accent = parse_color(value);
        else if (!strcmp(key, "texture")) snprintf(textures[1], sizeof textures[1], "%s", value);
        else if (!strcmp(key, "texture.top")) snprintf(textures[0], sizeof textures[0], "%s", value);
        else if (!strcmp(key, "texture.side")) snprintf(textures[1], sizeof textures[1], "%s", value);
        else if (!strcmp(key, "texture.bottom")) snprintf(textures[2], sizeof textures[2], "%s", value);
        else if (!strcmp(key, "light")) m->light = atoi(value);
        else if (!strcmp(key, "transparent")) m->transparent = atoi(value);
        else if (!strcmp(key, "solid")) m->solid = atoi(value);
        else if (!strcmp(key, "blows")) m->blows = atoi(value);
        else if (!strcmp(key, "tool")) m->tool = thing_by_name(value);
        else if (!strcmp(key, "tool_blows")) m->tool_blows = atoi(value);
        else if (!strcmp(key, "food")) m->food = (float)atof(value);
        else if (!strcmp(key, "drops")) {
            char what[32];
            int n = 1;
            if (sscanf(value, "%31s %d", what, &n) >= 1) {
                m->drops = thing_by_name(what);
                m->drops_count = n;
            }
        } else if (!strcmp(key, "kind")) {
            static const char* const kinds[] = {"block", "material", "tool", "food", "clothing"};
            for (int k = 1; k < 5; k++)
                if (!strcmp(value, kinds[k]) && m->kind != THING_BLOCK) m->kind = k;
        } else {
            printf("blocks: %s line %d: unknown key %s\n", path, number, key);
        }
    }
    fclose(f);
}

/* ---- recipes.txt ---- */

/* "stick 2" -> the thing's palette index and the amount. */
static int parse_amount(char* s, int* index, int* amount) {
    char name[32];
    int n = 1;
    if (sscanf(trim(s), "%31s %d", name, &n) < 1) return 0;
    int t = thing_by_name(name);
    *index = palette_index(t);
    *amount = n;
    return *index >= 0 && n > 0 && n <= 64;
}

static void load_recipes(const char* pack) {
    char path[1200], line[512];
    snprintf(path, sizeof path, "%s/recipes.txt", pack);
    FILE* f = fopen(path, "r");
    if (!f) return;
    int number = 0;
    while (fgets(line, sizeof line, f)) {
        number++;
        char* hash = strchr(line, '#');
        if (hash) *hash = 0;
        char* s = trim(line);
        if (!*s) continue;
        if (!strncmp(s, "remove ", 7)) {
            int p = palette_index(thing_by_name(trim(s + 7)));
            for (int r = 0; r < recipe_count; r++)
                if (recipes[r].output == p) {
                    memmove(&recipes[r], &recipes[r + 1], sizeof recipes[0] * (size_t)(recipe_count - r - 1));
                    recipe_count--;
                    r--;
                }
            continue;
        }
        char* arrow = strstr(s, "->");
        if (!arrow || recipe_count >= MAX_RECIPES) {
            printf("blocks: %s line %d: expected inputs -> output\n", path, number);
            continue;
        }
        *arrow = 0;
        char* bar = strchr(arrow + 2, '|');
        if (bar) *bar = 0;
        UiRecipe r = {{-1, -1}, {0, 0}, -1, 0, -1, -1};
        int ok = 1, n = 0, idx, amount;
        for (char* part = strtok(s, ","); part && ok; part = strtok(NULL, ",")) {
            if (n >= 2 || !parse_amount(part, &idx, &amount)) ok = 0;
            else r.input[n] = (int8_t)idx, r.amount[n++] = (int8_t)amount;
        }
        if (ok && parse_amount(arrow + 2, &idx, &amount)) r.output = (int8_t)idx, r.count = (int8_t)amount;
        else ok = 0;
        if (bar && ok) {
            char kind[16], name[32];
            if (sscanf(trim(bar + 1), "%15s %31s", kind, name) == 2) {
                int p = palette_index(thing_by_name(name));
                if (!strcmp(kind, "tool")) r.tool = (int8_t)p;
                else if (!strcmp(kind, "near")) r.station = (int8_t)p;
                if (p < 0) ok = 0;
            }
        }
        if (!ok || n == 0) {
            printf("blocks: %s line %d: recipe not understood\n", path, number);
            continue;
        }
        recipes[recipe_count++] = r;
    }
    fclose(f);
}

static int pack_name_cmp(const void* a, const void* b) { return strcmp((const char*)a, (const char*)b); }

/* Finds the mods of this world and loads their things and recipes; before
 * the textures are made and the world is loaded. */
static void load_mods(const char* save_path) {
    memcpy(recipes, ui_recipes, sizeof ui_recipes);
    recipe_count = UI_RECIPES;
    const char* dir = getenv("HYPOCKET_BLOCKS_MODS");
    if (dir) snprintf(mods_dir, sizeof mods_dir, "%s", dir);
    else if (save_path) snprintf(mods_dir, sizeof mods_dir, "%s.mods", save_path);
    else return;
    DIR* d = opendir(mods_dir);
    if (!d) return;
    struct dirent* e;
    while ((e = readdir(d)) && pack_count < MAX_PACKS) {
        if (e->d_name[0] == '.') continue;
        char path[1200];
        snprintf(path, sizeof path, "%s/%s", mods_dir, e->d_name);
        struct stat st;
        if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) snprintf(packs[pack_count++], sizeof packs[0], "%s", path);
    }
    closedir(d);
    qsort(packs, (size_t)pack_count, sizeof packs[0], pack_name_cmp);
    load_ids();
    for (int i = 0; i < pack_count; i++) load_things(packs[i]);
    for (int i = 0; i < pack_count; i++) load_recipes(packs[i]);
    save_ids();
    int things = 0;
    for (int t = 0; t < THING_TYPES; t++) things += mod_things[t].used;
    printf("blocks: %d mods, %d new things, %d recipes\n", pack_count, things, recipe_count);
}

/* A sound file from the mods (the last one that has it), else NULL. */
static const char* mod_sound_path(const char* name, char* out, size_t size) {
    for (int i = pack_count - 1; i >= 0; i--) {
        snprintf(out, size, "%s/sounds/%s", packs[i], name);
        if (access(out, R_OK) == 0) return out;
    }
    return NULL;
}

#if !defined(__clang__)
#pragma GCC diagnostic pop
#endif
