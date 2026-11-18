#include "save.h"

#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#if defined(_WIN32)
#include <direct.h>
#define make_dir(p) _mkdir(p)
#else
#define make_dir(p) mkdir((p), 0755)
#endif

// A save is a text file of "key=value" lines: what the player has (level,
// life, energy, herbs...), the story so far, where he stands, and for each
// area he has been to, the tiles that changed (chests opened, the goddess
// gone from where she waited) and the monsters already beaten. Only that
// is kept, not the whole game, so saves still load after the game is
// updated; keys a version does not know are skipped.
#define FIRST_LINE "timberheart save"
#define VERSION 2

static char dir[512] = ".";

void save_set_dir(const char *d)
{
    snprintf(dir, sizeof dir, "%s", d && *d ? d : ".");
}

const char *save_dir(void) { return dir; }

// Makes the folder and the ones above it.
static void make_dirs(void)
{
    char path[sizeof dir];
    snprintf(path, sizeof path, "%s", dir);
    for (char *p = path + 1; *p; p++)
        if (*p == '/' || *p == '\\') {
            char c = *p;
            *p = 0;
            make_dir(path);
            *p = c;
        }
    make_dir(path);
}

static void slot_path(char *out, size_t size, int slot)
{
    snprintf(out, size, "%s/game_%d.save", dir, slot);
}

// Writes to a temporary file first, so a crash never leaves half a save.
static bool write_file(const char *path, const char *text, size_t size)
{
    char tmp[600];
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "wb");
    if (!f) return false;
    bool ok = fwrite(text, 1, size, f) == size;
    ok = fclose(f) == 0 && ok;
    if (!ok) { remove(tmp); return false; }
    remove(path);  // Windows does not rename over a file
    return rename(tmp, path) == 0;
}

// --- Reading the header -------------------------------------------------------------

// Fills `info` from the first lines of a save. False when it is not one.
static bool read_info(FILE *f, SaveInfo *info)
{
    char line[160];
    memset(info, 0, sizeof *info);
    if (!fgets(line, sizeof line, f) || strncmp(line, FIRST_LINE, strlen(FIRST_LINE)) != 0) return false;
    info->area = -1;
    while (fgets(line, sizeof line, f)) {
        char key[40];
        long long v;
        if (sscanf(line, "%39[^=]=%lld", key, &v) != 2) continue;
        if (strcmp(key, "slot") == 0) info->slot = (int)v;
        else if (strcmp(key, "order") == 0) info->order = (int)v;
        else if (strcmp(key, "saved_at") == 0) info->saved_at = v;
        else if (strcmp(key, "level") == 0) info->level = (int)v;
        else if (strcmp(key, "area") == 0) info->area = (int)v;
        else if (strcmp(key, "area_visited") == 0) break;  // the header is over
    }
    return info->slot > 0;
}

static int newest_first(const void *a, const void *b)
{
    const SaveInfo *x = a, *y = b;
    return y->order - x->order;
}

SaveInfo *save_list(int *count)
{
    *count = 0;
    int cap = 16;
    SaveInfo *list = malloc(cap * sizeof *list);
    DIR *d = opendir(dir);
    if (!d || !list) {
        if (d) closedir(d);
        return list;
    }
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        int slot, n = 0;
        if (sscanf(e->d_name, "game_%d.save%n", &slot, &n) != 1 || n == 0 || e->d_name[n] != 0 || slot <= 0) continue;
        char path[600];
        slot_path(path, sizeof path, slot);
        FILE *f = fopen(path, "rb");
        if (!f) continue;
        SaveInfo info;
        bool ok = read_info(f, &info) && info.slot == slot;
        fclose(f);
        if (!ok) continue;
        if (*count == cap) {
            SaveInfo *bigger = realloc(list, (cap *= 2) * sizeof *list);
            if (!bigger) break;
            list = bigger;
        }
        list[(*count)++] = info;
    }
    closedir(d);
    qsort(list, *count, sizeof *list, newest_first);
    return list;
}

// --- Writing ------------------------------------------------------------------------

// An area as it is at the start of a new game.
static Game fresh;  // big; keep it off the stack

static bool load_fresh(int area)
{
    int rows = 0;
    const char *const *text = area_rows(area, &rows);
    memset(&fresh, 0, sizeof fresh);
    return text && area_load_rows(&fresh, text, rows);
}

typedef struct {
    char *text;
    size_t len, cap;
} Buffer;

static void put(Buffer *b, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

static void put(Buffer *b, const char *fmt, ...)
{
    va_list ap;
    for (;;) {
        va_start(ap, fmt);
        int n = vsnprintf(b->text ? b->text + b->len : NULL, b->text ? b->cap - b->len : 0, fmt, ap);
        va_end(ap);
        if (n < 0) return;
        if (b->text && b->len + (size_t)n < b->cap) {
            b->len += (size_t)n;
            return;
        }
        size_t cap = b->cap ? b->cap * 2 : 4096;
        while (cap < b->len + (size_t)n + 1) cap *= 2;
        char *bigger = realloc(b->text, cap);
        if (!bigger) return;
        b->text = bigger;
        b->cap = cap;
    }
}

// The highest slot number and save order on disk.
static void highest(int *slot, int *order)
{
    int count;
    SaveInfo *list = save_list(&count);
    *slot = *order = 0;
    for (int i = 0; i < count; i++) {
        if (list[i].slot > *slot) *slot = list[i].slot;
        if (list[i].order > *order) *order = list[i].order;
    }
    free(list);
}

bool save_write(Game *g)
{
    make_dirs();
    int top_slot, top_order;
    highest(&top_slot, &top_order);
    if (g->save_slot <= 0) g->save_slot = top_slot + 1;

    const Stats *st = &g->stats;
    const Ally *a = &g->goddess;
    const Walker *w = &g->walker;
    Buffer b = { 0 };
    put(&b, FIRST_LINE "\nversion=%d\nslot=%d\norder=%d\nsaved_at=%lld\n", VERSION, g->save_slot, top_order + 1,
        (long long)time(NULL));
    put(&b, "level=%d\nxp=%d\nhp=%d\nmax_hp=%d\nsp=%d\nmax_sp=%d\natk=%d\ndef=%d\nherbs=%d\n", st->level, st->xp,
        st->hp, st->max_hp, st->sp, st->max_sp, st->atk, st->def, st->herbs);
    put(&b, "goddess_joined=%d\ngoddess_hp=%d\ngoddess_max_hp=%d\ngoddess_mp=%d\ngoddess_max_mp=%d\n", a->joined,
        a->hp, a->max_hp, a->mp, a->max_mp);
    put(&b, "goddess_mag=%d\ngoddess_def=%d\n", a->mag, a->def);
    put(&b, "goddess_saved=%d\nheard_cry=%d\ncreatures_met=%d\nelder_woke=%d\n", g->goddess_saved, g->heard_cry,
        g->creatures_met, g->elder_woke);
    put(&b, "area=%d\ntile_x=%d\ntile_y=%d\nface_x=%d\nface_y=%d\n", g->area,
        (int)((w->x + WALKER_SIZE / 2.0f) / TILE), (int)((w->y + WALKER_SIZE / 2.0f) / TILE), w->face_x, w->face_y);
    put(&b, "respawn_area=%d\nrespawn_x=%d\nrespawn_y=%d\nrng=%u\n", g->respawn_area, (int)g->respawn_x,
        (int)g->respawn_y, g->rng);

    // Each area he has been to: what changed since the start.
    for (int ar = 0; ar < area_count() && ar < MAX_AREAS; ar++) {
        bool here = ar == g->area;
        if (!here && !g->saved[ar].visited) continue;
        if (!load_fresh(ar)) continue;
        const unsigned char (*tiles)[MAX_MAP_W] = here ? g->tiles : g->saved[ar].tiles;
        const Foe *foes = here ? g->foes : g->saved[ar].foes;
        int foe_count = here ? g->foe_count : g->saved[ar].foe_count;
        put(&b, "area_visited=%d\n", ar);
        for (int y = 0; y < fresh.h; y++)
            for (int x = 0; x < fresh.w; x++)
                if (tiles[y][x] != fresh.tiles[y][x]) put(&b, "tile=%d %d %d %d\n", ar, x, y, tiles[y][x]);
        for (int i = 0; i < foe_count && i < fresh.foe_count; i++)
            if (!foes[i].alive) put(&b, "beaten=%d %d\n", ar, i);
    }

    char path[600];
    slot_path(path, sizeof path, g->save_slot);
    bool ok = b.text && write_file(path, b.text, b.len);
    free(b.text);
    return ok;
}

// --- Loading ------------------------------------------------------------------------

bool save_read(int slot, Game *g)
{
    char path[600];
    slot_path(path, sizeof path, slot);
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    SaveInfo info;
    if (!read_info(f, &info) || info.slot != slot) {
        fclose(f);
        return false;
    }
    rewind(f);

    game_new(g);  // anything the save does not say stays as in a new game
    int area = AREA_HOME, tx = -1, ty = -1, fx = 0, fy = 1;
    int respawn_area = -1, rx = 0, ry = 0;
    bool visited[MAX_AREAS] = { false };
    static unsigned char tiles[MAX_AREAS][MAX_MAP_H][MAX_MAP_W];
    static bool beaten[MAX_AREAS][MAX_FOES];
    memset(beaten, 0, sizeof beaten);
    for (int ar = 0; ar < MAX_AREAS && ar < area_count(); ar++)
        if (load_fresh(ar)) memcpy(tiles[ar], fresh.tiles, sizeof fresh.tiles);

    char line[160];
    while (fgets(line, sizeof line, f)) {
        char key[40];
        int v[4];
        unsigned u;
        if (sscanf(line, "tile=%d %d %d %d", &v[0], &v[1], &v[2], &v[3]) == 4) {
            if (v[0] >= 0 && v[0] < MAX_AREAS && v[1] >= 0 && v[1] < MAX_MAP_W && v[2] >= 0 && v[2] < MAX_MAP_H &&
                v[3] >= 0 && v[3] < TILE_COUNT)
                tiles[v[0]][v[2]][v[1]] = (unsigned char)v[3];
            continue;
        }
        if (sscanf(line, "beaten=%d %d", &v[0], &v[1]) == 2) {
            if (v[0] >= 0 && v[0] < MAX_AREAS && v[1] >= 0 && v[1] < MAX_FOES) beaten[v[0]][v[1]] = true;
            continue;
        }
        if (sscanf(line, "rng=%u", &u) == 1) { g->rng = u; continue; }
        if (sscanf(line, "%39[^=]=%d", key, &v[0]) != 2) continue;
        int n = v[0];
        Stats *st = &g->stats;
        Ally *a = &g->goddess;
        if (!strcmp(key, "level")) st->level = n;
        else if (!strcmp(key, "xp")) st->xp = n;
        else if (!strcmp(key, "hp")) st->hp = n;
        else if (!strcmp(key, "max_hp")) st->max_hp = n;
        else if (!strcmp(key, "sp")) st->sp = n;
        else if (!strcmp(key, "max_sp")) st->max_sp = n;
        else if (!strcmp(key, "atk")) st->atk = n;
        else if (!strcmp(key, "def")) st->def = n;
        else if (!strcmp(key, "herbs")) st->herbs = n;
        else if (!strcmp(key, "goddess_joined")) a->joined = n != 0;
        else if (!strcmp(key, "goddess_hp")) a->hp = n;
        else if (!strcmp(key, "goddess_max_hp")) a->max_hp = n;
        else if (!strcmp(key, "goddess_mp")) a->mp = n;
        else if (!strcmp(key, "goddess_max_mp")) a->max_mp = n;
        else if (!strcmp(key, "goddess_mag")) a->mag = n;
        else if (!strcmp(key, "goddess_def")) a->def = n;
        else if (!strcmp(key, "goddess_saved")) g->goddess_saved = n != 0;
        else if (!strcmp(key, "heard_cry")) g->heard_cry = n != 0;
        else if (!strcmp(key, "creatures_met")) g->creatures_met = n != 0;
        else if (!strcmp(key, "elder_woke")) g->elder_woke = n != 0;
        else if (!strcmp(key, "area")) area = n;
        else if (!strcmp(key, "tile_x")) tx = n;
        else if (!strcmp(key, "tile_y")) ty = n;
        else if (!strcmp(key, "face_x")) fx = n;
        else if (!strcmp(key, "face_y")) fy = n;
        else if (!strcmp(key, "respawn_area")) respawn_area = n;
        else if (!strcmp(key, "respawn_x")) rx = n;
        else if (!strcmp(key, "respawn_y")) ry = n;
        else if (!strcmp(key, "area_visited") && n >= 0 && n < MAX_AREAS) visited[n] = true;
    }
    fclose(f);
    if (area < 0 || area >= area_count() || area >= MAX_AREAS) area = AREA_HOME;
    visited[area] = true;

    // Rebuild every area he had been to, then walk into his.
    for (int ar = 0; ar < MAX_AREAS && ar < area_count(); ar++) {
        g->saved[ar].visited = false;
        if (!visited[ar] || !load_fresh(ar)) continue;
        g->saved[ar].visited = true;
        memcpy(g->saved[ar].tiles, tiles[ar], sizeof tiles[ar]);
        memcpy(g->saved[ar].foes, fresh.foes, sizeof fresh.foes);
        g->saved[ar].foe_count = fresh.foe_count;
        for (int i = 0; i < fresh.foe_count; i++)
            if (beaten[ar][i]) g->saved[ar].foes[i].alive = false;
    }
    g->area = -1;  // so entering does not keep the new game's home
    game_enter_area(g, area, 0);
    if (tx >= 0 && ty >= 0 && tx < g->w && ty < g->h) game_place_walker(g, tx, ty, fx, fy);
    if (respawn_area >= 0) {
        g->respawn_area = respawn_area;
        g->respawn_x = (float)rx;
        g->respawn_y = (float)ry;
    }
    g->save_slot = slot;
    return true;
}

bool settings_write(bool portuguese, bool sound_on, unsigned hints_done)
{
    make_dirs();
    char path[600], text[96];
    snprintf(path, sizeof path, "%s/settings.txt", dir);
    int n = snprintf(text, sizeof text, "language=%s\nsound=%s\nhints_done=%u\n", portuguese ? "pt" : "en",
                     sound_on ? "on" : "off", hints_done);
    return write_file(path, text, (size_t)n);
}

bool settings_read(bool *portuguese, bool *sound_on, unsigned *hints_done)
{
    char path[600], line[64];
    snprintf(path, sizeof path, "%s/settings.txt", dir);
    FILE *f = fopen(path, "r");
    if (!f) return false;
    while (fgets(line, sizeof line, f)) {
        if (strncmp(line, "language=", 9) == 0) *portuguese = strncmp(line + 9, "pt", 2) == 0;
        if (strncmp(line, "sound=", 6) == 0) *sound_on = strncmp(line + 6, "off", 3) != 0;
        if (strncmp(line, "hints_done=", 11) == 0) *hints_done = (unsigned)strtoul(line + 11, NULL, 10);
    }
    fclose(f);
    return true;
}
