/* HyPocket Blocks: the game's own screens, for where no app draws them (the
 * PC version, BlocksPlatform.game_screens): the hotbar, the meters, the
 * notices, the inventory with the recipes, the menu, the settings and the
 * notes. They follow the Android app's screens (blocks/src/main/kotlin/.../ui)
 * and use its words and tables through generated.h.
 *
 * Included by blocks.c (it reads the game's state directly). Drawn in one
 * draw call per frame: coloured rectangles and the letters of a public
 * domain 8x8 font (third_party/font8x8), into the frame before it is shown. */
#include "generated.h"
#include "third_party/font8x8/font8x8_basic.h"
#include "third_party/font8x8/font8x8_ext_latin.h"

/* ---- Drawing ---- */

typedef struct {
    float x, y, u, v; /* pixels from the top left; u < 0: plain colour */
    uint8_t r, g, b, a;
} UiVertex;

#define UI_MAX_QUADS 8000
static UiVertex ui_verts[UI_MAX_QUADS * 6];
static int ui_quads;
static GLuint ui_program, ui_vao, ui_vbo, ui_font;
static GLint uu_screen;
static int ui_w, ui_h, ui_s; /* frame size, and the size of one UI pixel */

static const char* ui_vs =
    "#version 330 core\n"
    "layout(location = 0) in vec2 pos;\n"
    "layout(location = 1) in vec2 uv;\n"
    "layout(location = 2) in vec4 color;\n"
    "uniform vec2 screen;\n"
    "out vec2 v_uv;\n"
    "out vec4 v_color;\n"
    "void main() {\n"
    "    v_uv = uv;\n"
    "    v_color = color;\n"
    "    gl_Position = vec4(pos.x / screen.x * 2.0 - 1.0, 1.0 - pos.y / screen.y * 2.0, 0.0, 1.0);\n"
    "}\n";

static const char* ui_fs =
    "#version 330 core\n"
    "uniform sampler2D font;\n"
    "in vec2 v_uv;\n"
    "in vec4 v_color;\n"
    "out vec4 frag;\n"
    "void main() {\n"
    "    float a = v_uv.x < 0.0 ? 1.0 : texture(font, v_uv).r;\n"
    "    frag = vec4(v_color.rgb, v_color.a * a);\n"
    "}\n";

#define FONT_COLS 16
#define FONT_ROWS 14 /* 128 basic + 96 Latin-1 glyphs */

static void ui_init(void) {
    ui_program = link_program(ui_vs, ui_fs);
    uu_screen = p_glGetUniformLocation(ui_program, "screen");
    p_glUseProgram(ui_program);
    p_glUniform1i(p_glGetUniformLocation(ui_program, "font"), 0);
    static uint8_t atlas[FONT_ROWS * 8][FONT_COLS * 8];
    for (int g = 0; g < FONT_COLS * FONT_ROWS; g++) {
        const char* rows = g < 128 ? font8x8_basic[g] : font8x8_ext_latin[g - 128];
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++)
                atlas[(g / FONT_COLS) * 8 + y][(g % FONT_COLS) * 8 + x] = (rows[y] >> x) & 1 ? 255 : 0;
    }
    p_glGenTextures(1, &ui_font);
    p_glBindTexture(GL_TEXTURE_2D, ui_font);
    p_glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, FONT_COLS * 8, FONT_ROWS * 8, 0, GL_RED, GL_UNSIGNED_BYTE, atlas);
    p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    p_glGenVertexArrays(1, &ui_vao);
    p_glBindVertexArray(ui_vao);
    p_glGenBuffers(1, &ui_vbo);
    p_glBindBuffer(GL_ARRAY_BUFFER, ui_vbo);
    p_glBufferData(GL_ARRAY_BUFFER, sizeof ui_verts, NULL, GL_STREAM_DRAW);
    p_glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(UiVertex), (void*)0);
    p_glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(UiVertex), (void*)8);
    p_glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(UiVertex), (void*)16);
    for (int a = 0; a < 3; a++) p_glEnableVertexAttribArray(a);
}

static void ui_quad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, uint32_t argb) {
    if (ui_quads >= UI_MAX_QUADS) return;
    uint8_t a = argb >> 24, r = argb >> 16, g = argb >> 8, b = argb;
    UiVertex c[4] = {{x0, y0, u0, v0, r, g, b, a}, {x1, y0, u1, v0, r, g, b, a},
                     {x1, y1, u1, v1, r, g, b, a}, {x0, y1, u0, v1, r, g, b, a}};
    UiVertex* v = &ui_verts[ui_quads++ * 6];
    v[0] = c[0]; v[1] = c[1]; v[2] = c[2]; v[3] = c[0]; v[4] = c[2]; v[5] = c[3];
}

static void ui_rect(float x, float y, float w, float h, uint32_t argb) { ui_quad(x, y, x + w, y + h, -1, -1, -1, -1, argb); }

static void ui_frame_rect(float x, float y, float w, float h, float t, uint32_t argb) {
    ui_rect(x, y, w, t, argb);
    ui_rect(x, y + h - t, w, t, argb);
    ui_rect(x, y + t, t, h - 2 * t, argb);
    ui_rect(x + w - t, y + t, t, h - 2 * t, argb);
}

/* The next character of UTF-8 text, as glyphs of the font: a few
 * typographic characters it lacks become plain ones ("…" is three dots). */
static int next_glyphs(const char** s, int out[3]) {
    const unsigned char* p = (const unsigned char*)*s;
    unsigned c = *p++;
    if (c >= 0xC0) {
        int extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : 1;
        c &= 0x3F >> extra;
        while (extra-- && (*p & 0xC0) == 0x80) c = (c << 6) | (*p++ & 0x3F);
    }
    *s = (const char*)p;
    switch (c) {
    case 0x2018: case 0x2019: out[0] = '\''; return 1;
    case 0x201C: case 0x201D: out[0] = '"'; return 1;
    case 0x2013: case 0x2014: out[0] = '-'; return 1;
    case 0x2026: out[0] = out[1] = out[2] = '.'; return 3;
    case 0x2192: out[0] = '-'; out[1] = '>'; return 2;
    case 0x00A0: out[0] = ' '; return 1;
    }
    if (c < 128) out[0] = (int)c;
    else if (c >= 0xA0 && c <= 0xFF) out[0] = 128 + (int)(c - 0xA0);
    else out[0] = '?';
    return 1;
}

/* Width in pixels of [text] at [size] UI pixels per font pixel. */
static float ui_text_width(const char* text, int size) {
    float w = 0;
    int g[3];
    while (*text && *text != '\n') w += next_glyphs(&text, g) * 8 * size;
    return w;
}

static float ui_text(float x, float y, int size, uint32_t argb, const char* text) {
    float start = x;
    int g[3];
    while (*text && *text != '\n') {
        int n = next_glyphs(&text, g);
        for (int i = 0; i < n; i++) {
            if (g[i] != ' ') {
                float u = (g[i] % FONT_COLS) / (float)FONT_COLS, v = (g[i] / FONT_COLS) / (float)FONT_ROWS;
                ui_quad(x, y, x + 8 * size, y + 8 * size, u, v, u + 1.0f / FONT_COLS, v + 1.0f / FONT_ROWS, argb);
            }
            x += 8 * size;
        }
    }
    return x - start;
}

/* Text with a dark shadow, readable over the world. */
static float ui_label(float x, float y, int size, uint32_t argb, const char* text) {
    ui_text(x + size, y + size, size, 0xA0000000, text);
    return ui_text(x, y, size, argb, text);
}

static void ui_label_centered(float cx, float y, int size, uint32_t argb, const char* text) {
    ui_label(cx - ui_text_width(text, size) / 2, y, size, argb, text);
}

/* Draws [text] wrapped to [width] pixels (it may hold "\n"); returns its height. */
static float ui_wrapped(float x, float y, float width, int size, uint32_t argb, const char* text, int draw) {
    float line_h = 11 * size, cy = y;
    char line[512];
    while (*text) {
        /* Take words while they fit. */
        int len = 0, last_space = -1;
        const char* p = text;
        while (*p && *p != '\n') {
            if (len >= (int)sizeof line - 4) break;
            const char* before = p;
            int g[3];
            next_glyphs(&p, g);
            int bytes = (int)(p - before);
            memcpy(line + len, before, (size_t)bytes);
            line[len + bytes] = 0;
            if (ui_text_width(line, size) > width && last_space > 0) {
                p = text + last_space + 1;
                len = last_space;
                break;
            }
            if (*before == ' ') last_space = len;
            len += bytes;
        }
        line[len] = 0;
        if (draw) ui_text(x, cy, size, argb, line);
        cy += line_h;
        text = *p == '\n' ? p + 1 : p;
        if (p == text && !*p) break;
    }
    return cy - y;
}

static void ui_flush(void) {
    if (!ui_quads) return;
    p_glUseProgram(ui_program);
    p_glUniform2f(uu_screen, (float)ui_w, (float)ui_h);
    p_glBindTexture(GL_TEXTURE_2D, ui_font);
    p_glBindVertexArray(ui_vao);
    p_glBindBuffer(GL_ARRAY_BUFFER, ui_vbo);
    p_glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)ui_quads * 6 * sizeof(UiVertex), ui_verts);
    p_glEnable(GL_BLEND);
    p_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    p_glDrawArrays(GL_TRIANGLES, 0, ui_quads * 6);
    p_glDisable(GL_BLEND);
    ui_quads = 0;
}

/* ---- Words ---- */

static int lang_pt;

static const char* T(int id) { return (lang_pt ? strings_pt : strings_en)[id]; }
static const char* const* TA(int id) { return (lang_pt ? arrays_pt : arrays_en)[id]; }

/* A thing in the palette: the base ones from generated.h, the mods' from mods.c. */
static const char* thing_label(int p) {
    if (p >= UI_THINGS) return mod_things[palette[p]].label[lang_pt];
    return thing_label(p);
}
static int thing_kind(int p) { return p >= UI_THINGS ? mod_things[palette[p]].kind : ui_things[p].kind; }
static uint32_t thing_color(int p) { return p >= UI_THINGS ? mod_things[palette[p]].color : ui_things[p].color; }
static uint32_t thing_accent(int p) {
    if (p < UI_THINGS) return ui_things[p].accent;
    const ModThing* m = &mod_things[palette[p]];
    return m->accent ? m->accent : m->color;
}

static void detect_language(void) {
    const char* forced = getenv("HYPOCKET_BLOCKS_LANG");
#ifdef _WIN32
    LANGID id = GetUserDefaultUILanguage();
    lang_pt = (id & 0x3ff) == LANG_PORTUGUESE;
#else
    const char* l = getenv("LC_ALL");
    if (!l || !*l) l = getenv("LC_MESSAGES");
    if (!l || !*l) l = getenv("LANG");
    lang_pt = l && strncmp(l, "pt", 2) == 0;
#endif
    if (forced) lang_pt = strncmp(forced, "pt", 2) == 0;
}

/* ---- State ---- */

enum { SCR_NONE, SCR_INVENTORY, SCR_MENU, SCR_SETTINGS, SCR_NOTES, SCR_NOTE };
#define HOTBAR 9

static int screen = SCR_NONE, screen_note = -1, note_from_list, screen_opened; /* opened this frame */
static int hotbar[HOTBAR], hot_sel;
static int focus, focus_count, confirm_pressed; /* keyboard focus on the screens' buttons */
static int show_stats, sensitivity_percent = 100, captured_now = -1;
static float recipes_scroll, things_scroll;
static char hotbar_path[4200], settings_path[4200];

/* What the counters in SharedIo said last frame, to notice changes. */
static int seen_picks, seen_needs, seen_deaths, seen_takes, seen_saves, seen_notes, seen_away = -1;
#define NOTICES 4
static struct { char text[160]; float time; uint32_t color; } notices[NOTICES];
static float hint_time = 20.0f; /* the controls hint, for the first seconds */

static void notice(uint32_t color, const char* text) {
    for (int i = NOTICES - 1; i > 0; i--) notices[i] = notices[i - 1];
    snprintf(notices[0].text, sizeof notices[0].text, "%s", text);
    notices[0].time = 4.0f;
    notices[0].color = color;
}

static int thing_count(int p) { return creative ? 1 : inventory[palette[p]]; }
static int thing_usable(int p) { return thing_kind(p) == THING_BLOCK || thing_kind(p) == THING_FOOD; }

static void save_hotbar(void) {
    if (!hotbar_path[0]) return;
    FILE* f = fopen(hotbar_path, "w");
    if (!f) return;
    for (int i = 0; i < HOTBAR; i++) fprintf(f, "%d\n", hotbar[i]);
    fprintf(f, "%d\n", hot_sel);
    fclose(f);
}

static void save_settings(SharedIo* io) {
    if (!settings_path[0]) return;
    FILE* f = fopen(settings_path, "w");
    if (!f) return;
    fprintf(f, "volume %d\nview %d\nfps %d\nsensitivity %d\nstats %d\n", io->volume_percent, io->view_distance,
            io->fps_setting, sensitivity_percent, show_stats);
    fclose(f);
}

static void screens_start(SharedIo* io, const char* save_path) {
    _Static_assert(UI_THINGS == BASE_PALETTE, "BlocksPalette.kt and palette[] must list the same things");
    detect_language();
    ui_init();
    for (int i = 0, n = 0; i < PALETTE_SIZE && n < HOTBAR; i++)
        if (thing_kind(i) == THING_BLOCK) hotbar[n++] = i;
    if (save_path) {
        snprintf(hotbar_path, sizeof hotbar_path, "%s.hotbar", save_path);
        FILE* f = fopen(hotbar_path, "r");
        if (f) {
            int v[HOTBAR + 1], n = 0;
            while (n <= HOTBAR && fscanf(f, "%d", &v[n]) == 1) n++;
            fclose(f);
            if (n == HOTBAR + 1) {
                for (int i = 0; i < HOTBAR; i++)
                    if (v[i] >= 0 && v[i] < PALETTE_SIZE) hotbar[i] = v[i];
                if (v[HOTBAR] >= 0 && v[HOTBAR] < HOTBAR) hot_sel = v[HOTBAR];
            }
        }
    }
    const char* settings = getenv("HYPOCKET_BLOCKS_SETTINGS");
    if (settings) {
        snprintf(settings_path, sizeof settings_path, "%s", settings);
        FILE* f = fopen(settings_path, "r");
        char key[32];
        int value;
        while (f && fscanf(f, "%31s %d", key, &value) == 2) {
            if (!strcmp(key, "volume")) io->volume_percent = value;
            else if (!strcmp(key, "view")) io->view_distance = value;
            else if (!strcmp(key, "fps")) io->fps_setting = value;
            else if (!strcmp(key, "sensitivity")) sensitivity_percent = value;
            else if (!strcmp(key, "stats")) show_stats = value;
        }
        if (f) fclose(f);
    }
    seen_picks = io->picks;
    seen_needs = io->needs;
    seen_deaths = io->deaths;
    seen_takes = io->takes;
    seen_saves = io->saves;
    seen_notes = io->notes_opened;
}

/* Survival: what was just collected and has no place takes one whose thing ran out. */
static void fill_hotbar(void) {
    if (creative) return;
    for (int p = 0; p < PALETTE_SIZE; p++) {
        if (!thing_usable(p) || thing_count(p) <= 0) continue;
        int placed = 0, empty = -1;
        for (int i = 0; i < HOTBAR; i++) {
            if (hotbar[i] == p) placed = 1;
            if (empty < 0 && thing_count(hotbar[i]) <= 0) empty = i;
        }
        if (!placed && empty >= 0) {
            hotbar[empty] = p;
            save_hotbar();
        }
    }
}

static void assign_hotbar(int p) {
    if (!thing_usable(p)) return;
    for (int i = 0; i < HOTBAR; i++)
        if (hotbar[i] == p) hotbar[i] = hotbar[hot_sel];
    hotbar[hot_sel] = p;
    save_hotbar();
}

/* ---- Widgets ---- */

static const PlatformInput* in;

static int pointer_in(float x, float y, float w, float h) {
    return in && in->pointer_x >= x && in->pointer_x < x + w && in->pointer_y >= y && in->pointer_y < y + h;
}

static int key_down(int key) {
    for (int i = 0; in && i < in->key_count; i++)
        if (in->keys[i] == key) return 1;
    return 0;
}

/* A button; returns 1 when clicked or chosen with the keyboard. */
static int ui_button(float x, float y, float w, float h, const char* label, int enabled) {
    int index = focus_count++;
    int hover = pointer_in(x, y, w, h);
    if (hover && in->clicks == 0 && enabled) focus = index;
    int lit = enabled && (hover || focus == index);
    ui_rect(x, y, w, h, !enabled ? 0x80303030 : lit ? 0xF04A6A9A : 0xE0303846);
    ui_frame_rect(x, y, w, h, ui_s, lit ? 0xFFDDE6F5 : 0xFF5A6478);
    int size = ui_s;
    ui_text(x + (w - ui_text_width(label, size)) / 2, y + (h - 8 * size) / 2, size, enabled ? 0xFFFFFFFF : 0xFF8A8A8A, label);
    return enabled && ((hover && in->clicks > 0) || (confirm_pressed && focus == index));
}

static void ui_icon(float x, float y, float size, int p) {
    uint32_t c = thing_color(p) | 0xFF000000u, a = thing_accent(p) | 0xFF000000u;
    if (thing_kind(p) == THING_BLOCK) {
        ui_rect(x, y, size, size, c);
        ui_rect(x, y + size * 0.7f, size, size * 0.3f, a);
    } else { /* items: a smaller shape with its accent */
        ui_rect(x + size * 0.2f, y + size * 0.2f, size * 0.6f, size * 0.6f, c);
        ui_rect(x + size * 0.35f, y + size * 0.35f, size * 0.3f, size * 0.3f, a);
    }
}

static void ui_panel(float x, float y, float w, float h) {
    ui_rect(x, y, w, h, 0xEE1E1E1E);
    ui_frame_rect(x, y, w, h, ui_s, 0xFF3C4250);
}

static void open_screen(int s, SharedIo* io) {
    screen = s;
    screen_opened = 1;
    focus = 0;
    io->paused = s == SCR_MENU || s == SCR_SETTINGS || s == SCR_NOTES || s == SCR_NOTE;
    if (s == SCR_NONE) stop_speech();
}

/* ---- The screens ---- */

static void draw_hud(SharedIo* io, float dt) {
    int s = ui_s;
    /* Hotbar: nine places along the bottom, the chosen one framed. */
    float slot = 20 * s, gap = 2 * s, bar_w = HOTBAR * slot + (HOTBAR - 1) * gap;
    float bx = (ui_w - bar_w) / 2, by = ui_h - slot - 6 * s;
    for (int i = 0; i < HOTBAR; i++) {
        float x = bx + i * (slot + gap);
        int p = hotbar[i], n = thing_count(p);
        ui_rect(x, by, slot, slot, 0x90000000);
        if (n > 0 || creative) ui_icon(x + 3 * s, by + 3 * s, slot - 6 * s, p);
        else ui_rect(x + 3 * s, by + 3 * s, slot - 6 * s, slot - 6 * s, 0x40FFFFFF);
        if (!creative) {
            char count[8];
            snprintf(count, sizeof count, "%d", n);
            ui_label(x + slot - ui_text_width(count, 1) - 2 * s, by + slot - 9, 1, 0xFFFFFFFF, count);
        }
        ui_frame_rect(x, by, slot, slot, i == hot_sel ? 2 * s : s, i == hot_sel ? 0xFFFFFFFF : 0x80FFFFFF);
    }
    char line[200];
    int p = hotbar[hot_sel];
    if (creative) snprintf(line, sizeof line, "%s", thing_label(p));
    else snprintf(line, sizeof line, T(STR_blocks_block_count), thing_label(p), thing_count(p));
    ui_label_centered(ui_w / 2.0f, by - 11 * s, s, 0xFFFFFFFF, line);

    /* Health, food and warmth, above the hotbar. */
    if (!creative) {
        float mw = 64 * s, mh = 5 * s, my = by - 22 * s, mx = (ui_w - 3 * mw - 2 * 6 * s) / 2;
        int warming = io->survival_flags & SURVIVAL_WARMING, cold = io->survival_flags & SURVIVAL_COLD;
        struct { int tenths; uint32_t fill, back; } meters[3] = {
            {io->health_tenths, 0xFFD94040, 0xFF4A1818},
            {io->food_tenths, 0xFFE0A040, 0xFF4A3418},
            {io->warmth_tenths, cold ? 0xFF7FC4FF : warming ? 0xFFFFC08A : 0xFFFF9A5A, 0xFF1E2E4A},
        };
        for (int i = 0; i < 3; i++) {
            float x = mx + i * (mw + 6 * s);
            ui_rect(x, my, mw, mh, meters[i].back);
            int t = meters[i].tenths < 0 ? 0 : meters[i].tenths > 100 ? 100 : meters[i].tenths;
            ui_rect(x, my, mw * t / 100.0f, mh, meters[i].fill);
        }
        ui_label(mx + mw + 6 * s, my - 9 * s, 1, 0xFFFFFFFF, T(STR_blocks_meter_food));
        ui_label(mx + 2 * (mw + 6 * s), my - 9 * s, 1, 0xFFFFFFFF, T(STR_blocks_meter_warmth));
    }

    /* Clock, or the other side's name, at the top. */
    int away = (io->story_flags & STORY_AWAY) != 0;
    if (away) {
        ui_label_centered(ui_w / 2.0f, 6 * s, s, 0xFFD9C8FF, T(STR_blocks_away));
    } else if (io->day > 0) {
        snprintf(line, sizeof line, T(STR_blocks_clock), io->day, io->clock_minutes / 60, io->clock_minutes % 60);
        ui_label_centered(ui_w / 2.0f, 6 * s, s, 0xFFFFFFFF, line);
    }
    if (io->story_flags & STORY_SHELTERED) ui_label_centered(ui_w / 2.0f, 18 * s, s, 0xFFFFE3A8, T(STR_blocks_sheltered));

    /* Notices: what was picked up, a missing tool, fainting, being taken. */
    if (io->picks != seen_picks) {
        seen_picks = io->picks;
        if (io->picked >= 0 && io->picked < PALETTE_SIZE) {
            snprintf(line, sizeof line, T(STR_blocks_picked), thing_label(io->picked));
            notice(0xFFB8F0B8, line);
        }
    }
    if (io->needs != seen_needs) {
        seen_needs = io->needs;
        if (io->needed >= 0 && io->needed < PALETTE_SIZE) {
            snprintf(line, sizeof line, T(STR_blocks_needs_tool), thing_label(io->needed));
            notice(0xFFFFE08A, line);
        }
    }
    if (io->deaths != seen_deaths) { seen_deaths = io->deaths; notice(0xFFFFFFFF, T(STR_blocks_fainted)); }
    if (io->takes != seen_takes) { seen_takes = io->takes; notice(0xFFD9C8FF, T(STR_blocks_taken)); }
    if (io->saves != seen_saves) { seen_saves = io->saves; notice(0xFFC0C0C0, T(STR_blocks_saved)); }
    if (seen_away >= 0 && away != seen_away) notice(0xFFD9C8FF, T(away ? STR_blocks_away_arrived : STR_blocks_away_back));
    seen_away = away;
    for (int i = 0; i < NOTICES; i++) {
        if (notices[i].time <= 0) continue;
        notices[i].time -= dt;
        uint32_t alpha = notices[i].time < 1 ? (uint32_t)(notices[i].time * 255) : 255;
        float y = 32 * s + i * 11 * s, w = ui_text_width(notices[i].text, s);
        ui_rect((ui_w - w) / 2 - 3 * s, y - 2 * s, w + 6 * s, 11 * s, (alpha * 0x60 / 255) << 24);
        ui_text((ui_w - w) / 2, y, s, (alpha << 24) | (notices[i].color & 0xFFFFFF), notices[i].text);
    }

    /* Under the crosshair: what the place action would do, and digging. */
    float cy = ui_h / 2.0f + 14 * s;
    if (io->story_flags & STORY_AIM_NOTE) ui_label_centered(ui_w / 2.0f, cy, s, 0xFFFFFFFF, T(STR_blocks_note_aim));
    else if (io->story_flags & STORY_AIM_PICKUP) ui_label_centered(ui_w / 2.0f, cy, s, 0xFFFFFFFF, T(STR_blocks_pickup_aim));
    else if (io->aiming_beacon)
        ui_label_centered(ui_w / 2.0f, cy, s, 0xFFFFE3A8, T(creative ? STR_blocks_beacon_aim_free : STR_blocks_beacon_aim));
    if (io->dig_percent > 0 && io->dig_percent < 100) {
        float w = 40 * s;
        ui_rect(ui_w / 2.0f - w / 2, ui_h / 2.0f + 8 * s, w, 2 * s, 0x80000000);
        ui_rect(ui_w / 2.0f - w / 2, ui_h / 2.0f + 8 * s, w * io->dig_percent / 100.0f, 2 * s, 0xFFFFFFFF);
    }
    if (io->story_flags & STORY_GRABBED) {
        ui_rect(0, 0, ui_w, ui_h, 0x50300000);
        ui_label_centered(ui_w / 2.0f, ui_h / 2.0f - 30 * s, s, 0xFFFFD0D0, T(STR_blocks_grabbed));
        float w = 80 * s;
        ui_rect(ui_w / 2.0f - w / 2, ui_h / 2.0f - 18 * s, w, 4 * s, 0x80000000);
        ui_rect(ui_w / 2.0f - w / 2, ui_h / 2.0f - 18 * s, w * io->grab_percent / 100.0f, 4 * s, 0xFFFFFFFF);
    }

    /* The first seconds: how to play here. F3: the statistics. */
    if (hint_time > 0) {
        hint_time -= dt;
        ui_wrapped(6 * s, (show_stats ? 30 : 6) * s, ui_w / 3.0f, 1, 0xC0FFFFFF, T(STR_blocks_hint_mouse), 1);
    }
    if (show_stats) {
        snprintf(line, sizeof line, T(STR_blocks_stats), io->fps, io->frame_ms, io->triangles);
        ui_label(6 * s, 6 * s, 1, 0xFFFFFFFF, line);
        snprintf(line, sizeof line, T(STR_blocks_resolution), io->width, io->height, io->scale_percent);
        ui_label(6 * s, 6 * s + 11, 1, 0xFFFFFFFF, line);
    }
}

/* The things carried (all blocks in creative mode) and, in survival, the recipes. */
static void draw_inventory(SharedIo* io) {
    int s = ui_s;
    float pw = fminf(ui_w - 20.0f * s, 300.0f * s), ph = fminf(ui_h - 20.0f * s, 190.0f * s);
    float px = (ui_w - pw) / 2, py = (ui_h - ph) / 2;
    ui_rect(0, 0, ui_w, ui_h, 0x8C000000);
    ui_panel(px, py, pw, ph);
    ui_text(px + 8 * s, py + 7 * s, s, 0xFFFFFFFF, T(STR_blocks_inventory));
    float close_w = ui_text_width(T(STR_blocks_close), s) + 12 * s;
    if (ui_button(px + pw - close_w - 6 * s, py + 4 * s, close_w, 13 * s, T(STR_blocks_close), 1) ||
        (!screen_opened && (key_down(BLOCKS_KEY_INVENTORY) || key_down(BLOCKS_KEY_MENU)))) {
        open_screen(SCR_NONE, io);
        return;
    }
    int with_recipes = !creative;
    float gx = px + 8 * s, gy = py + 22 * s, gw = with_recipes ? pw * 0.45f : pw - 16 * s, gh = ph - 30 * s;
    float cell = 18 * s;
    int cols = (int)(gw / cell);
    if (cols < 1) cols = 1;
    /* The things: click one to put it in the chosen hotbar place. */
    int shown[MAX_PALETTE], n = 0;
    for (int p = 0; p < PALETTE_SIZE; p++)
        if (creative ? thing_kind(p) == THING_BLOCK : thing_count(p) > 0) shown[n++] = p;
    int rows = (n + cols - 1) / cols;
    float content = rows * cell;
    if (pointer_in(gx, gy, gw, gh) && in->wheel) things_scroll -= in->wheel * cell;
    things_scroll = fmaxf(0, fminf(things_scroll, fmaxf(0, content - gh)));
    const char* hovered = NULL;
    for (int i = 0; i < n; i++) {
        float x = gx + (i % cols) * cell, y = gy + (i / cols) * cell - things_scroll;
        if (y < gy - 1 || y + cell > gy + gh + 1) continue;
        int p = shown[i], hover = pointer_in(x, y, cell - s, cell - s);
        ui_rect(x, y, cell - s, cell - s, hover ? 0xFF3A4556 : 0xFF2A2F38);
        ui_icon(x + 3 * s, y + 3 * s, cell - 7 * s, p);
        if (!creative) {
            char count[8];
            snprintf(count, sizeof count, "%d", thing_count(p));
            ui_text(x + cell - s - ui_text_width(count, 1) - s, y + cell - s - 9, 1, 0xFFFFFFFF, count);
        }
        for (int h = 0; h < HOTBAR; h++)
            if (hotbar[h] == p) ui_frame_rect(x, y, cell - s, cell - s, s, h == hot_sel ? 0xFFFFFFFF : 0x80FFFFFF);
        if (hover) {
            hovered = thing_label(p);
            if (in->clicks) assign_hotbar(p);
        }
    }
    if (hovered) ui_text(gx, py + ph - 10 * s, 1, 0xFFDDDDDD, hovered);
    /* The hotbar places can be chosen here with 1-9 too. */
    for (int k = 0; k < HOTBAR; k++)
        if (key_down(BLOCKS_KEY_HOTBAR_1 + k)) hot_sel = k;

    if (!with_recipes) return;
    /* Recipes: inputs -> output, with the tool or workstation they need. */
    float rx = px + pw * 0.5f, ry = gy, rw = pw * 0.5f - 8 * s, row = 22 * s;
    ui_text(rx, py + 12 * s, 1, 0xFFB0B8C8, T(STR_blocks_recipes));
    content = recipe_count * row;
    if (pointer_in(rx, ry, rw, gh) && in->wheel) recipes_scroll -= in->wheel * row;
    recipes_scroll = fmaxf(0, fminf(recipes_scroll, fmaxf(0, content - gh)));
    int stations = io->survival_flags & (SURVIVAL_NEAR_KILN | SURVIVAL_NEAR_STOVE | SURVIVAL_NEAR_CAMPFIRE);
    for (int r = 0; r < recipe_count; r++) {
        float y = ry + r * row - recipes_scroll;
        if (y < ry - 1 || y + row > ry + gh + 1) continue;
        const UiRecipe* rc = &recipes[r];
        int enough = 1;
        for (int k = 0; k < 2; k++)
            if (rc->input[k] >= 0 && thing_count(rc->input[k]) < rc->amount[k]) enough = 0;
        int has_tool = rc->tool < 0 || thing_count(rc->tool) > 0;
        int station_bit = rc->station < 0 ? 0 : palette[rc->station] == KILN ? SURVIVAL_NEAR_KILN
                          : palette[rc->station] == STOVE ? SURVIVAL_NEAR_STOVE : SURVIVAL_NEAR_CAMPFIRE;
        int has_station = rc->station < 0 || (stations & station_bit);
        int can = enough && has_tool && has_station;
        int hover = pointer_in(rx, y, rw, row - s);
        ui_rect(rx, y, rw, row - s, hover && can ? 0xFF3A4556 : 0xFF262A32);
        float x = rx + 3 * s;
        char text[64];
        for (int k = 0; k < 2; k++) {
            if (rc->input[k] < 0) continue;
            ui_icon(x, y + 2 * s, 10 * s, rc->input[k]);
            snprintf(text, sizeof text, "%d", rc->amount[k]);
            x += 11 * s;
            x += ui_text(x, y + 4 * s, 1, thing_count(rc->input[k]) >= rc->amount[k] ? 0xFFFFFFFF : 0xFFFF8A8A, text) + 4 * s;
        }
        x += ui_text(x, y + 4 * s, 1, 0xFFB0B8C8, T(STR_blocks_recipe_arrow)) + 4 * s;
        ui_icon(x, y + 2 * s, 10 * s, rc->output);
        x += 11 * s;
        snprintf(text, sizeof text, "%d %s", rc->count, thing_label(rc->output));
        ui_text(x, y + 4 * s, 1, can ? 0xFFFFFFFF : 0xFFA0A0A0, text);
        char need[120] = "";
        if (rc->tool >= 0) {
            snprintf(text, sizeof text, T(STR_blocks_recipe_tool), thing_label(rc->tool));
            snprintf(need, sizeof need, "%s", text);
        }
        if (rc->station >= 0) {
            snprintf(text, sizeof text, T(STR_blocks_recipe_station), thing_label(rc->station));
            size_t used = strlen(need);
            snprintf(need + used, sizeof need - used, "%s%s", used ? " · " : "", text);
        }
        if (need[0])
            ui_text(rx + 3 * s, y + 14 * s, 1, (rc->tool >= 0 && !has_tool) || (rc->station >= 0 && !has_station)
                                                    ? 0xFFFF8A8A : 0xFF9AA4B4, need);
        if (hover && can && in->clicks) {
            io->craft[0] = rc->input[0] < 0 ? 255 : (uint8_t)rc->input[0];
            io->craft[1] = (uint8_t)rc->amount[0];
            io->craft[2] = rc->input[1] < 0 ? 255 : (uint8_t)rc->input[1];
            io->craft[3] = (uint8_t)rc->amount[1];
            io->craft[4] = (uint8_t)rc->output;
            io->craft[5] = (uint8_t)rc->count;
            io->craft[6] = rc->station < 0 ? 255 : (uint8_t)rc->station;
            io->craft[7] = rc->tool < 0 ? 255 : (uint8_t)rc->tool;
            io->crafts++;
        }
    }
}

static void draw_menu(SharedIo* io) {
    int s = ui_s;
    float bw = 120 * s, bh = 16 * s, gap = 4 * s;
    int notes = __builtin_popcount((unsigned)io->notes_found);
    int buttons = notes ? 4 : 3;
    float ph = 26 * s + buttons * (bh + gap), pw = bw + 24 * s;
    float px = (ui_w - pw) / 2, py = (ui_h - ph) / 2, x = px + 12 * s, y = py + 22 * s;
    ui_rect(0, 0, ui_w, ui_h, 0x99000000);
    ui_panel(px, py, pw, ph);
    ui_text(px + (pw - ui_text_width(T(STR_blocks_paused), s)) / 2, py + 8 * s, s, 0xFFFFFFFF, T(STR_blocks_paused));
    if (ui_button(x, y, bw, bh, T(STR_blocks_resume), 1) || (!screen_opened && key_down(BLOCKS_KEY_MENU))) open_screen(SCR_NONE, io);
    y += bh + gap;
    if (ui_button(x, y, bw, bh, T(STR_blocks_settings), 1)) open_screen(SCR_SETTINGS, io);
    y += bh + gap;
    if (notes) {
        char label[64];
        snprintf(label, sizeof label, T(STR_blocks_notes), notes, UI_NOTES);
        if (ui_button(x, y, bw, bh, label, 1)) open_screen(SCR_NOTES, io);
        y += bh + gap;
    }
    if (ui_button(x, y, bw, bh, T(STR_blocks_save_and_exit), 1)) io->quit = 1;
}

/* One setting: its name, its value and buttons to lower and raise it. */
static int setting_row(float x, float y, float w, const char* name, const char* value) {
    int s = ui_s;
    ui_text(x, y + 4 * s, s, 0xFFFFFFFF, name);
    float bw = 14 * s, vx = x + w - bw * 2 - 80 * s;
    int change = 0;
    if (ui_button(vx, y, bw, 14 * s, "-", 1)) change = -1;
    ui_text(vx + bw + (80 * s - ui_text_width(value, s)) / 2, y + 4 * s, s, 0xFFFFE3A8, value);
    if (ui_button(vx + bw + 80 * s, y, bw, 14 * s, "+", 1)) change = 1;
    return change;
}

static void draw_settings(SharedIo* io) {
    int s = ui_s;
    float pw = fminf(ui_w - 20.0f * s, 360.0f * s), ph = 140 * s, px = (ui_w - pw) / 2, py = (ui_h - ph) / 2;
    float x = px + 10 * s, w = pw - 20 * s, y = py + 24 * s, step = 19 * s;
    ui_rect(0, 0, ui_w, ui_h, 0x99000000);
    ui_panel(px, py, pw, ph);
    ui_text(px + 10 * s, py + 8 * s, s, 0xFFFFFFFF, T(STR_blocks_settings));
    char v[32];
    int changed = 0, c;
    snprintf(v, sizeof v, "%d%%", io->volume_percent);
    if ((c = setting_row(x, y, w, T(STR_blocks_setting_volume), v))) {
        io->volume_percent = io->volume_percent + c * 10 < 0 ? 0 : io->volume_percent + c * 10 > 100 ? 100 : io->volume_percent + c * 10;
        changed = 1;
    }
    y += step;
    if (io->view_distance <= 0) snprintf(v, sizeof v, "%s", T(STR_blocks_setting_default));
    else snprintf(v, sizeof v, "%d", io->view_distance);
    if ((c = setting_row(x, y, w, T(STR_blocks_setting_view), v))) {
        int d = io->view_distance <= 0 ? 72 : io->view_distance;
        d += c * 16;
        io->view_distance = d < 32 ? 32 : d > 160 ? 160 : d;
        changed = 1;
    }
    y += step;
    static const int fps_steps[] = {0, 30, 60, 90, 120, 144, -1};
    int fi = 0;
    while (fi < 6 && fps_steps[fi] != io->fps_setting) fi++;
    if (io->fps_setting == 0) snprintf(v, sizeof v, "%s", T(STR_blocks_setting_default));
    else if (io->fps_setting < 0) snprintf(v, sizeof v, "%s", T(STR_blocks_setting_no_limit));
    else snprintf(v, sizeof v, "%d", io->fps_setting);
    if ((c = setting_row(x, y, w, T(STR_blocks_setting_fps), v))) {
        fi = (fi + c + 7) % 7;
        io->fps_setting = fps_steps[fi];
        changed = 1;
    }
    y += step;
    snprintf(v, sizeof v, "%d%%", sensitivity_percent);
    if ((c = setting_row(x, y, w, T(STR_blocks_setting_sensitivity), v))) {
        sensitivity_percent += c * 10;
        sensitivity_percent = sensitivity_percent < 30 ? 30 : sensitivity_percent > 300 ? 300 : sensitivity_percent;
        changed = 1;
    }
    y += step;
    snprintf(v, sizeof v, "%s", T(show_stats ? STR_blocks_on : STR_blocks_off));
    if (setting_row(x, y, w, T(STR_blocks_setting_stats), v)) {
        show_stats = !show_stats;
        changed = 1;
    }
    if (changed) save_settings(io);
    float bw = ui_text_width(T(STR_blocks_back), s) + 16 * s;
    if (ui_button(px + pw - bw - 10 * s, py + ph - 20 * s, bw, 14 * s, T(STR_blocks_back), 1) || (!screen_opened && key_down(BLOCKS_KEY_MENU)))
        open_screen(SCR_MENU, io);
}

static void draw_notes(SharedIo* io) {
    int s = ui_s;
    float pw = fminf(ui_w - 20.0f * s, 240.0f * s), ph = fminf(ui_h - 20.0f * s, 200.0f * s);
    float px = (ui_w - pw) / 2, py = (ui_h - ph) / 2, y = py + 24 * s;
    ui_rect(0, 0, ui_w, ui_h, 0x99000000);
    ui_panel(px, py, pw, ph);
    ui_text(px + 10 * s, py + 8 * s, s, 0xFFFFFFFF, T(STR_blocks_notes_title));
    for (int i = 0; i < UI_NOTES; i++) {
        if (!(io->notes_found & (1 << ui_notes[i].id))) continue;
        char label[160];
        snprintf(label, sizeof label, "%s", T(ui_notes[i].title));
        if (ui_button(px + 10 * s, y, pw - 20 * s, 15 * s, label, 1)) {
            screen_note = ui_notes[i].id;
            note_from_list = 1;
            open_screen(SCR_NOTE, io);
        }
        y += 18 * s;
    }
    float bw = ui_text_width(T(STR_blocks_back), s) + 16 * s;
    if (ui_button(px + pw - bw - 10 * s, py + ph - 20 * s, bw, 14 * s, T(STR_blocks_back), 1) || (!screen_opened && key_down(BLOCKS_KEY_MENU)))
        open_screen(SCR_MENU, io);
}

static void draw_note(SharedIo* io) {
    int s = ui_s;
    const UiNote* note = NULL;
    for (int i = 0; i < UI_NOTES; i++)
        if (ui_notes[i].id == screen_note) note = &ui_notes[i];
    float pw = fminf(ui_w - 20.0f * s, 280.0f * s), ph = ui_h - 20.0f * s;
    float px = (ui_w - pw) / 2, py = 10.0f * s;
    ui_rect(0, 0, ui_w, ui_h, 0xAA000000);
    ui_panel(px, py, pw, ph);
    if (note) {
        ui_text(px + 10 * s, py + 8 * s, s, 0xFFFFE3A8, T(note->title));
        float y = py + 24 * s, width = pw - 20 * s;
        int size = ui_s >= 3 ? 2 : 1;
        if (!note->phone) {
            ui_rect(px + 6 * s, y - 4 * s, pw - 12 * s, ph - 48 * s, 0xFFE9E1CC); /* paper */
            ui_wrapped(px + 10 * s, y, width - 8 * s, size, 0xFF2A2420, T(note->text), 1);
        } else {
            /* A chat: "sender|message", the phone's owner without a name, on the right. */
            const char* const* lines = TA(note->text);
            for (int i = 0; lines[i] && y < py + ph - 40 * s; i++) {
                const char* bar = strchr(lines[i], '|');
                const char* msg = bar ? bar + 1 : lines[i];
                int own = bar == lines[i];
                float bw = width * 0.75f;
                float h = ui_wrapped(0, 0, bw - 8 * s, size, 0, msg, 0) + (own ? 0 : 10 * size);
                float bx = own ? px + pw - 10 * s - bw : px + 10 * s;
                ui_rect(bx, y, bw, h + 6 * s, own ? 0xFF2F5D8A : 0xFF3A3F48);
                float ty = y + 3 * s;
                if (!own) {
                    char sender[64];
                    snprintf(sender, sizeof sender, "%.*s", (int)(bar ? bar - lines[i] : 0), lines[i]);
                    ui_text(bx + 4 * s, ty, size, 0xFFFFC98A, sender);
                    ty += 10 * size;
                }
                ui_wrapped(bx + 4 * s, ty, bw - 8 * s, size, 0xFFFFFFFF, msg, 1);
                y += h + 10 * s;
            }
        }
    }
    float bw = ui_text_width(T(STR_blocks_close), s) + 16 * s;
    if (ui_button(px + pw - bw - 10 * s, py + ph - 20 * s, bw, 14 * s, T(STR_blocks_close), 1) ||
        (!screen_opened && (key_down(BLOCKS_KEY_MENU) || key_down(BLOCKS_KEY_CONFIRM)))) {
        if (note_from_list) open_screen(SCR_NOTES, io);
        else open_screen(SCR_NONE, io);
    }
}

/* Each frame, after the world and before the frame is shown. */
static void screens_frame(SharedIo* io, int width, int height, float dt) {
    const BlocksPlatform* platform = blocks_platform;
    in = platform->input;
    ui_w = width;
    ui_h = height;
    ui_s = height >= 1000 ? 3 : height >= 600 ? 2 : 1;
    focus_count = 0;
    screen_opened = 0;
    confirm_pressed = key_down(BLOCKS_KEY_CONFIRM);
    if (key_down(BLOCKS_KEY_DEBUG)) {
        show_stats = !show_stats;
        save_settings(io);
    }

    /* A note picked up in the world opens at once. */
    if (io->notes_opened != seen_notes) {
        seen_notes = io->notes_opened;
        screen_note = io->note_id;
        note_from_list = 0;
        open_screen(SCR_NOTE, io);
    }
    if (screen == SCR_NONE) {
        if (key_down(BLOCKS_KEY_MENU)) open_screen(SCR_MENU, io);
        else if (key_down(BLOCKS_KEY_INVENTORY)) open_screen(SCR_INVENTORY, io);
        for (int k = 0; k < HOTBAR; k++)
            if (key_down(BLOCKS_KEY_HOTBAR_1 + k)) hot_sel = k;
        if (in && in->wheel) hot_sel = ((hot_sel - in->wheel) % HOTBAR + HOTBAR) % HOTBAR;
    }
    if (screen_opened) confirm_pressed = 0;
    /* Keyboard and controller: move between the buttons of a screen. */
    if (key_down(BLOCKS_KEY_DOWN) || key_down(BLOCKS_KEY_RIGHT)) focus++;
    if (key_down(BLOCKS_KEY_UP) || key_down(BLOCKS_KEY_LEFT)) focus--;

    fill_hotbar();
    int selected = hotbar[hot_sel];
    if (io->selected != selected) io->selected = selected;
    platform->input->look_scale = sensitivity_percent / 100.0f;

    draw_hud(io, dt);
    switch (screen) {
    case SCR_INVENTORY: draw_inventory(io); break;
    case SCR_MENU: draw_menu(io); break;
    case SCR_SETTINGS: draw_settings(io); break;
    case SCR_NOTES: draw_notes(io); break;
    case SCR_NOTE: draw_note(io); break;
    }
    if (focus_count > 0) focus = (focus % focus_count + focus_count) % focus_count;

    int capture = screen == SCR_NONE;
    if (capture != captured_now && platform->capture_pointer) {
        platform->capture_pointer(capture);
        captured_now = capture;
    }
    ui_flush();
}
