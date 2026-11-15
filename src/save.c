#include "save.h"

#include <dirent.h>
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

// A save file is this header and then the whole Game. VERSION changes when
// the meaning of what is saved changes; a Game of another size (the game was
// updated and keeps other things) is not loaded either.
#define MAGIC "TBHS"
#define VERSION 1

typedef struct {
    char magic[4];
    int version;
    int game_size;
    SaveInfo info;
} Header;

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
    snprintf(out, size, "%s/game_%d.sav", dir, slot);
}

static bool read_header(FILE *f, Header *h)
{
    return fread(h, sizeof *h, 1, f) == 1 && memcmp(h->magic, MAGIC, 4) == 0 && h->version == VERSION
        && h->game_size == (int)sizeof(Game);
}

// Writes to a temporary file first, so a crash never leaves half a save.
static bool write_file(const char *path, const void *a, size_t a_size, const void *b, size_t b_size)
{
    char tmp[600];
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "wb");
    if (!f) return false;
    bool ok = fwrite(a, a_size, 1, f) == 1 && (!b || fwrite(b, b_size, 1, f) == 1);
    ok = fclose(f) == 0 && ok;
    if (!ok) { remove(tmp); return false; }
    remove(path);  // Windows does not rename over a file
    return rename(tmp, path) == 0;
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
    Header h = { .magic = { 'T', 'B', 'H', 'S' }, .version = VERSION, .game_size = (int)sizeof(Game) };
    h.info.slot = g->save_slot;
    h.info.level = g->stats.level;
    h.info.area = g->area;
    h.info.saved_at = (long long)time(NULL);
    h.info.order = top_order + 1;
    char path[600];
    slot_path(path, sizeof path, g->save_slot);
    return write_file(path, &h, sizeof h, g, sizeof *g);
}

bool save_read(int slot, Game *g)
{
    char path[600];
    slot_path(path, sizeof path, slot);
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    Header h;
    static Game loaded;  // big; keep it off the stack
    bool ok = read_header(f, &h) && fread(&loaded, sizeof loaded, 1, f) == 1;
    fclose(f);
    if (!ok) return false;
    *g = loaded;
    g->save_slot = slot;
    return true;
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
        int slot;
        char end;
        if (sscanf(e->d_name, "game_%d.sa%c", &slot, &end) != 2 || end != 'v' || slot <= 0) continue;
        if (strstr(e->d_name, ".tmp")) continue;
        char path[600];
        slot_path(path, sizeof path, slot);
        FILE *f = fopen(path, "rb");
        if (!f) continue;
        Header h;
        bool ok = read_header(f, &h);
        fclose(f);
        if (!ok) continue;
        if (*count == cap) {
            SaveInfo *bigger = realloc(list, (cap *= 2) * sizeof *list);
            if (!bigger) break;
            list = bigger;
        }
        list[(*count)++] = h.info;
    }
    closedir(d);
    qsort(list, *count, sizeof *list, newest_first);
    return list;
}

bool settings_write(bool portuguese, bool sound_on)
{
    make_dirs();
    char path[600], text[64];
    snprintf(path, sizeof path, "%s/settings.txt", dir);
    int n = snprintf(text, sizeof text, "language=%s\nsound=%s\n", portuguese ? "pt" : "en", sound_on ? "on" : "off");
    return write_file(path, text, (size_t)n, NULL, 0);
}

bool settings_read(bool *portuguese, bool *sound_on)
{
    char path[600], line[64];
    snprintf(path, sizeof path, "%s/settings.txt", dir);
    FILE *f = fopen(path, "r");
    if (!f) return false;
    while (fgets(line, sizeof line, f)) {
        if (strncmp(line, "language=", 9) == 0) *portuguese = strncmp(line + 9, "pt", 2) == 0;
        if (strncmp(line, "sound=", 6) == 0) *sound_on = strncmp(line + 6, "off", 3) != 0;
    }
    fclose(f);
    return true;
}
