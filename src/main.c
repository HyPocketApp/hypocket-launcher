// Window, input, drawing and sound, with raylib. The rules are in game.c.
//
// The same file builds for PC (Windows, Linux, macOS) and Android. On
// Android, raylib's NativeActivity glue calls main() for us. Everything is
// laid out on a 320x180 screen, drawn at twice that (640x360) so the art can
// have 32x32 pixels per map tile, and scaled up to the window.
#include "game.h"
#include "save.h"
#include "assets.h"
#include "story.h"
#include "text.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define COUNT(a) (int)(sizeof(a) / sizeof((a)[0]))

#include "raylib.h"

// --- Colors -------------------------------------------------------------------

#define GRASS_A (Color){ 104, 168, 80, 255 }
#define GRASS_B (Color){ 98, 160, 76, 255 }
#define GRASS_TUFT (Color){ 76, 136, 64, 255 }
#define FLOWER (Color){ 250, 236, 140, 255 }
#define CLIFF_DARK (Color){ 84, 76, 70, 255 }
#define TRUNK (Color){ 112, 76, 48, 255 }
#define TRUNK_LIGHT (Color){ 176, 132, 86, 255 }
#define LEAVES (Color){ 46, 110, 60, 255 }
#define LEAVES_LIGHT (Color){ 74, 144, 76, 255 }
#define ROCK (Color){ 140, 140, 150, 255 }
#define ROCK_DARK (Color){ 96, 96, 108, 255 }
#define WATER (Color){ 64, 124, 200, 255 }
#define WATER_LIGHT (Color){ 120, 176, 236, 255 }
#define PLANK (Color){ 176, 128, 80, 255 }
#define PLANK_DARK (Color){ 120, 82, 50, 255 }
#define GOLDEN (Color){ 250, 200, 70, 255 }
#define FIRE_A (Color){ 255, 170, 40, 255 }
#define FIRE_B (Color){ 255, 236, 120, 255 }
#define PATH (Color){ 176, 150, 104, 255 }
#define SHIRT (Color){ 196, 50, 50, 255 }
#define SHIRT_DARK (Color){ 120, 28, 32, 255 }
#define PANTS (Color){ 60, 80, 130, 255 }
#define SKIN (Color){ 236, 188, 150, 255 }
#define BEARD (Color){ 136, 84, 46, 255 }
#define CAP (Color){ 60, 60, 70, 255 }
#define STEEL (Color){ 200, 210, 220, 255 }
#define SLIME_C (Color){ 110, 200, 120, 255 }
#define SLIME_DARK (Color){ 60, 140, 80, 255 }
#define BEETLE_C (Color){ 70, 90, 150, 255 }
#define BEETLE_DARK (Color){ 40, 50, 96, 255 }
#define WOLF_C (Color){ 130, 130, 140, 255 }
#define WOLF_DARK (Color){ 80, 80, 92, 255 }
#define BARK (Color){ 96, 66, 44, 255 }
#define BARK_DARK (Color){ 64, 42, 30, 255 }
#define MOSS (Color){ 90, 150, 70, 255 }
#define EYE_GLOW (Color){ 255, 220, 90, 255 }
#define HP_COLOR (Color){ 220, 70, 80, 255 }
#define SP_COLOR (Color){ 80, 170, 230, 255 }
#define PANEL (Color){ 28, 30, 48, 235 }
#define PANEL_EDGE (Color){ 220, 210, 180, 255 }
#define INK (Color){ 30, 30, 44, 255 }
#define MOON_WHITE (Color){ 244, 246, 255, 255 }
#define MOON_SHADE (Color){ 196, 204, 232, 255 }
#define MOON_SKIN (Color){ 250, 226, 210, 255 }
#define MOON_HAIR (Color){ 212, 218, 232, 255 }  // silver
#define MOON_SILVER (Color){ 200, 220, 255, 255 }
#define EMBER_RED (Color){ 230, 80, 40, 255 }
#define ROOF (Color){ 150, 60, 50, 255 }
#define ROOF_DARK (Color){ 110, 40, 36, 255 }
#define SHADOW (Color){ 0, 0, 0, 60 }
#define DISABLED (Color){ 120, 120, 130, 255 }

// --- Sound ----------------------------------------------------------------------

enum {
    SND_CHOP, SND_CHEST, SND_REST, SND_ENCOUNTER, SND_CURSOR, SND_CONFIRM, SND_DENIED, SND_HIT,
    SND_HURT, SND_HEAL, SND_DEFEATED, SND_VICTORY, SND_LEVEL_UP, SND_FLEE, SND_DOWN, SND_EXIT, SND_BUMP,
    SND_COUNT,
};

typedef struct {
    Sound s[SND_COUNT];
    bool ready;
} Sounds;

// Makes a short beep that slides from f0 to f1 Hz. With steps > 1 the pitch
// jumps in that many steps instead of sliding (an arpeggio). `noisy` adds a
// rough, woody sound.
static Sound make_tone(float f0, float f1, float seconds, float volume, int steps, bool noisy)
{
    const int rate = 22050;
    int n = (int)(seconds * rate);
    short *data = MemAlloc((unsigned)n * sizeof(short));
    float phase = 0.0f;
    unsigned seed = 12345u;
    for (int i = 0; i < n; i++) {
        float t = (float)i / (float)n;
        float k = steps > 1 ? floorf(t * steps) / (float)(steps - 1) : t;
        float f = f0 + (f1 - f0) * k;
        if (noisy) {
            seed = seed * 1103515245u + 12345u;
            f *= 0.6f + 0.8f * (float)((seed >> 16) & 0xff) / 255.0f;
        }
        phase += f / rate;
        float wave = fmodf(phase, 1.0f) < 0.5f ? 1.0f : -1.0f;
        float env = (1.0f - t) * fminf(1.0f, t * 200.0f);
        data[i] = (short)(wave * env * volume * 32767.0f);
    }
    Wave w = { (unsigned)n, (unsigned)rate, 16, 1, data };
    Sound s = LoadSoundFromWave(w);
    UnloadWave(w);
    return s;
}

static Sounds load_sounds(void)
{
    Sounds s = { 0 };
    InitAudioDevice();
    if (!IsAudioDeviceReady()) return s;
    s.s[SND_CHOP] = make_tone(220, 90, 0.14f, 0.25f, 0, true);
    s.s[SND_CHEST] = make_tone(523, 1047, 0.30f, 0.16f, 4, false);
    s.s[SND_REST] = make_tone(392, 784, 0.50f, 0.14f, 3, false);
    s.s[SND_ENCOUNTER] = make_tone(200, 800, 0.35f, 0.16f, 6, false);
    s.s[SND_CURSOR] = make_tone(880, 880, 0.03f, 0.08f, 0, false);
    s.s[SND_CONFIRM] = make_tone(660, 990, 0.07f, 0.10f, 2, false);
    s.s[SND_DENIED] = make_tone(140, 120, 0.15f, 0.15f, 0, false);
    s.s[SND_HIT] = make_tone(300, 90, 0.14f, 0.24f, 0, true);
    s.s[SND_HURT] = make_tone(440, 110, 0.25f, 0.22f, 0, true);
    s.s[SND_HEAL] = make_tone(523, 1047, 0.30f, 0.14f, 0, false);
    s.s[SND_DEFEATED] = make_tone(400, 60, 0.30f, 0.20f, 0, true);
    s.s[SND_VICTORY] = make_tone(523, 1568, 0.60f, 0.15f, 6, false);
    s.s[SND_LEVEL_UP] = make_tone(659, 1976, 0.60f, 0.15f, 8, false);
    s.s[SND_FLEE] = make_tone(800, 200, 0.25f, 0.14f, 0, false);
    s.s[SND_DOWN] = make_tone(330, 82, 0.80f, 0.18f, 4, false);
    s.s[SND_EXIT] = make_tone(392, 784, 0.25f, 0.12f, 2, false);
    s.s[SND_BUMP] = make_tone(160, 140, 0.08f, 0.20f, 0, true);
    s.ready = true;
    return s;
}

static bool sound_on = true;  // the title's Options page turns it off

static void play_events(const Sounds *s, unsigned ev)
{
    if (!s->ready || !sound_on) return;
    static const struct { unsigned event; int sound; } map[] = {
        { EVENT_CHOP, SND_CHOP }, { EVENT_CHEST, SND_CHEST }, { EVENT_REST, SND_REST },
        { EVENT_ENCOUNTER, SND_ENCOUNTER }, { EVENT_CURSOR, SND_CURSOR }, { EVENT_CONFIRM, SND_CONFIRM },
        { EVENT_DENIED, SND_DENIED }, { EVENT_HIT, SND_HIT }, { EVENT_HURT, SND_HURT }, { EVENT_HEAL, SND_HEAL },
        { EVENT_DEFEATED, SND_DEFEATED }, { EVENT_VICTORY, SND_VICTORY }, { EVENT_LEVEL_UP, SND_LEVEL_UP },
        { EVENT_FLEE, SND_FLEE }, { EVENT_PLAYER_DOWN, SND_DOWN }, { EVENT_EXIT, SND_EXIT }, { EVENT_BUMP, SND_BUMP },
    };
    for (int i = 0; i < (int)(sizeof map / sizeof map[0]); i++)
        if (ev & map[i].event) PlaySound(s->s[map[i].sound]);
}

static void unload_sounds(Sounds *s)
{
    if (!s->ready) return;
    for (int i = 0; i < SND_COUNT; i++) UnloadSound(s->s[i]);
    CloseAudioDevice();
}

// --- Music ----------------------------------------------------------------------
// Two pieces, from assets/music: the flute theme of this part of the forest,
// and "Conflito na Névoa" for battles and tense scenes. One fades into the
// other when the moment changes.

enum { MUSIC_FLUTE, MUSIC_CONFLICT, MUSIC_COUNT };
#define MUSIC_VOLUME 0.55f  // under the sound effects
#define MUSIC_FADE_IN 1.2f  // seconds
#define MUSIC_FADE_OUT 0.8f

static struct {
    Music m;
    bool loaded, playing, started;
    float vol;  // 0 to 1, fading
} music[MUSIC_COUNT];

static void load_music(void)
{
    if (!IsAudioDeviceReady()) return;
    static const char *const names[MUSIC_COUNT] = { "music/flute", "music/conflict" };
    for (int i = 0; i < MUSIC_COUNT; i++) {
        const EmbeddedAsset *a = asset_find(names[i]);
        if (!a) continue;
        music[i].m = LoadMusicStreamFromMemory(".ogg", a->data, (int)a->size);
        music[i].loaded = IsMusicValid(music[i].m);
        if (music[i].loaded) music[i].m.looping = true;
    }
}

// Which piece fits the moment, or -1 for none.
static int music_for(const Game *g)
{
    switch (g->state) {
    case STATE_BATTLE: return MUSIC_CONFLICT;
    case STATE_SCENE:  // reaching the goddess under attack, and the Elder Stump waking
        return g->scene == SCENE_CREATURES || g->scene == SCENE_ELDER ? MUSIC_CONFLICT : MUSIC_FLUTE;
    case STATE_GAME_OVER: return -1;
    default: return MUSIC_FLUTE;
    }
}

static void update_music(const Game *g, float dt)
{
    int want = sound_on ? music_for(g) : -1;
    float master = MUSIC_VOLUME * (g->state == STATE_PAUSED ? 0.4f : 1.0f);
    for (int i = 0; i < MUSIC_COUNT; i++) {
        if (!music[i].loaded) continue;
        bool on = i == want;
        if (on && !music[i].playing) {
            // The battle music starts over each time; the theme goes on
            // from where it stopped.
            if (i == MUSIC_CONFLICT || !music[i].started) {
                StopMusicStream(music[i].m);
                PlayMusicStream(music[i].m);
            } else {
                ResumeMusicStream(music[i].m);
            }
            music[i].playing = music[i].started = true;
        }
        if (!music[i].playing) continue;
        music[i].vol = on ? fminf(1.0f, music[i].vol + dt / MUSIC_FADE_IN) : fmaxf(0.0f, music[i].vol - dt / MUSIC_FADE_OUT);
        if (!on && music[i].vol <= 0.0f) {
            PauseMusicStream(music[i].m);
            music[i].playing = false;
            continue;
        }
        SetMusicVolume(music[i].m, music[i].vol * master);
        UpdateMusicStream(music[i].m);
    }
}

static void unload_music(void)
{
    for (int i = 0; i < MUSIC_COUNT; i++)
        if (music[i].loaded) UnloadMusicStream(music[i].m);
    memset(music, 0, sizeof music);
}

// --- Screen scaling -----------------------------------------------------------------

// Where the virtual 320x180 picture sits in the window.
typedef struct {
    float scale, x, y;
} View;

static View fit_view(void)
{
    float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
    float scale = fminf(sw / VIEW_W, sh / VIEW_H);
    return (View){ scale, (sw - VIEW_W * scale) / 2.0f, (sh - VIEW_H * scale) / 2.0f };
}

static Vector2 to_virtual(View v, Vector2 p)
{
    return (Vector2){ (p.x - v.x) / v.scale, (p.y - v.y) / v.scale };
}

// --- Battle layout (shared by drawing and touch) ------------------------------------

#define PANEL_Y 118
#define MENU_X 6
#define MENU_W 148
#define MENU_LINE 10
#define MENU_TOP (PANEL_Y + 5)

static Rectangle menu_entry_rect(int i)
{
    return (Rectangle){ MENU_X, (float)(MENU_TOP + i * MENU_LINE), MENU_W, MENU_LINE };
}

// The title menu: entries in a box in the middle of the screen.
#define TITLE_MENU_Y 70
#define TITLE_LINE 16

static Rectangle title_entry_rect(int i)
{
    return (Rectangle){ 100, (float)(TITLE_MENU_Y + 4 + i * TITLE_LINE), 120, TITLE_LINE };
}

// The load page: saves in rows of two lines, LOAD_ROWS at a time, and Back
// under them. load_first is the first save shown.
#define LOAD_Y 62
#define LOAD_ROW 22
#define LOAD_ROWS 4
static int load_first = 0;

static Rectangle load_row_rect(int row)
{
    return (Rectangle){ 44, (float)(LOAD_Y + 4 + row * LOAD_ROW), 232, LOAD_ROW };
}

static Rectangle load_back_rect(void)
{
    return (Rectangle){ 44, (float)(LOAD_Y + 4 + LOAD_ROWS * LOAD_ROW + 2), 232, 14 };
}

// Where each monster stands, and how big it is drawn.
static Rectangle enemy_rect(const Battle *b, int i)
{
    if (b->enemies[i].species == SPECIES_ELDER_STUMP) return (Rectangle){ 190, 22, 84, 90 };
    float spacing = b->enemy_count > 1 ? 60.0f : 0.0f;
    float x = 220 + (i - (b->enemy_count - 1) / 2.0f) * spacing;
    float y = 60 + (i % 2) * 14.0f;
    return (Rectangle){ x - 20, y - 20, 40, 44 };
}

// --- Input --------------------------------------------------------------------------

// On-screen controls, in virtual pixels. On the map the stick appears where
// a thumb touches the left half; the right half is the axe button.
#define STICK_R 20.0f
static const Vector2 BTN_ACTION = { 286, 146 };
static const Vector2 BTN_PAUSE = { 306, 14 };

typedef struct {
    bool touch_ui;
    int stick_id;
    Vector2 stick_origin, stick_pos;
    bool touch_action;
    bool was_touching;
    // Presses waiting for the next game step.
    bool confirm, cancel, pause;
    int nav_x, nav_y, pick, target_pick;
    // Repeats menu moves while a direction is held.
    int held_x, held_y;
    float repeat;
} InputState;

static bool near(Vector2 a, Vector2 b, float r)
{
    float dx = a.x - b.x, dy = a.y - b.y;
    return dx * dx + dy * dy <= r * r;
}

static void poll_touch(InputState *s, View v, const Game *g)
{
    int count = GetTouchPointCount();
    bool touching = count > 0, new_touch = touching && !s->was_touching;
    bool stick_alive = false;
    s->touch_action = false;
    if (touching) s->touch_ui = true;

    if (g->state == STATE_MAP) {
        for (int i = 0; i < count; i++) {
            int id = GetTouchPointId(i);
            Vector2 p = to_virtual(v, GetTouchPosition(i));
            if (id == s->stick_id) {
                stick_alive = true;
                s->stick_pos = p;
            } else if (near(p, BTN_PAUSE, 16)) {
                if (new_touch) s->pause = true;
            } else if (p.x >= VIEW_W / 2.0f) {
                s->touch_action = true;
            } else if (!stick_alive) {
                s->stick_id = id;
                s->stick_origin = s->stick_pos = p;
                stick_alive = true;
            }
        }
        static bool was_action = false;
        if (s->touch_action && !was_action) s->confirm = true;
        was_action = s->touch_action;
    } else if (new_touch) {
        Vector2 p = to_virtual(v, GetTouchPosition(0));
        if (g->state == STATE_BATTLE) {
            const Battle *b = &g->battle;
            if (b->phase == PHASE_MENU || b->phase == PHASE_SKILLS || b->phase == PHASE_ALLY) {
                int n = b->phase == PHASE_MENU ? MENU_COUNT : b->phase == PHASE_SKILLS ? SKILL_MENU_COUNT : SPELL_MENU_COUNT;
                for (int i = 0; i < n; i++)
                    if (CheckCollisionPointRec(p, menu_entry_rect(i))) s->pick = i + 1;
            } else if (b->phase == PHASE_TARGET) {
                for (int i = 0; i < b->enemy_count; i++)
                    if (b->enemies[i].hp > 0 && CheckCollisionPointRec(p, enemy_rect(b, i))) s->target_pick = i + 1;
                if (!s->target_pick && p.y > PANEL_Y) s->cancel = true;
            } else {
                s->confirm = true;  // tap to skip a message
            }
        } else if (g->state == STATE_TITLE && g->title_page == PAGE_LOAD) {
            for (int i = 0; i < LOAD_ROWS && load_first + i < g->save_count; i++)
                if (CheckCollisionPointRec(p, load_row_rect(i))) s->pick = load_first + i + 1;
            if (CheckCollisionPointRec(p, load_back_rect())) s->pick = g->save_count + 1;
        } else if (g->state == STATE_TITLE) {
            int n = g->title_page == PAGE_OPTIONS ? OPTION_COUNT : TITLE_COUNT;
            for (int i = 0; i < n; i++)
                if (CheckCollisionPointRec(p, title_entry_rect(i))) s->pick = i + 1;
        } else {
            s->confirm = true;  // tap anywhere on other screens
        }
    }
    if (!stick_alive) s->stick_id = -1;
    s->was_touching = touching;
}

static void read_keys(InputState *s, GameInput *in)
{
    if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) in->move_x -= 1;
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) in->move_x += 1;
    if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) in->move_y -= 1;
    if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) in->move_y += 1;
    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_J))
        s->confirm = true;
    if (IsKeyPressed(KEY_X) || IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_K)) s->cancel = true;
    if (IsKeyPressed(KEY_P)) s->pause = true;
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACK)) {
        s->pause = true;
        s->cancel = true;
    }

    if (s->stick_id >= 0) {
        float dx = (s->stick_pos.x - s->stick_origin.x) / STICK_R;
        float dy = (s->stick_pos.y - s->stick_origin.y) / STICK_R;
        if (fabsf(dx) + fabsf(dy) > 0.25f) {  // small dead zone
            in->move_x += dx;
            in->move_y += dy;
        }
    }

    if (IsGamepadAvailable(0)) {
        float ax = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X);
        float ay = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y);
        if (fabsf(ax) > 0.25f) in->move_x += ax;
        if (fabsf(ay) > 0.25f) in->move_y += ay;
        if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT)) in->move_x -= 1;
        if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) in->move_x += 1;
        if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_UP)) in->move_y -= 1;
        if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_DOWN)) in->move_y += 1;
        if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) s->confirm = true;
        if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT)) s->cancel = true;
        if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT)) s->pause = true;
    }

    // Menu moves: once on press, then repeated while held.
    int hx = in->move_x > 0.5f ? 1 : in->move_x < -0.5f ? -1 : 0;
    int hy = in->move_y > 0.5f ? 1 : in->move_y < -0.5f ? -1 : 0;
    float dt = GetFrameTime();
    if (hx != s->held_x || hy != s->held_y) {
        s->held_x = hx;
        s->held_y = hy;
        s->repeat = 0.35f;
        if (hx) s->nav_x = hx;
        if (hy) s->nav_y = hy;
    } else if (hx || hy) {
        s->repeat -= dt;
        if (s->repeat <= 0) {
            s->repeat = 0.12f;
            if (hx) s->nav_x = hx;
            if (hy) s->nav_y = hy;
        }
    }
}

// Hands the presses gathered since the last step to one game step.
static void take_presses(InputState *s, GameInput *in)
{
    in->confirm_pressed = s->confirm;
    in->cancel_pressed = s->cancel;
    in->pause_pressed = s->pause;
    in->nav_x = s->nav_x;
    in->nav_y = s->nav_y;
    in->pick = s->pick;
    in->target_pick = s->target_pick;
    s->confirm = s->cancel = s->pause = false;
    s->nav_x = s->nav_y = s->pick = s->target_pick = 0;
}

// --- Drawing helpers ------------------------------------------------------------------

// How opaque the panels and text are: below 1 while the map panel lets the
// lumberjack show through.
static float ui_alpha = 1.0f;

static void draw_text_shadow(const char *text, int x, int y, int size, Color color)
{
    DrawText(text, x + 1, y + 1, size, Fade(INK, ui_alpha));
    DrawText(text, x, y, size, Fade(color, ui_alpha * color.a / 255.0f));
}

static void draw_text_centered(const char *text, int y, int size, Color color)
{
    draw_text_shadow(text, (VIEW_W - MeasureText(text, size)) / 2, y, size, color);
}

static void draw_box(Rectangle r)
{
    DrawRectangleRec(r, Fade(PANEL, ui_alpha * PANEL.a / 255.0f));
    DrawRectangleLinesEx(r, 1, Fade(PANEL_EDGE, ui_alpha));
}

static void draw_bar(int x, int y, int w, int value, int max, Color c)
{
    DrawRectangle(x, y, w, 4, Fade(BLACK, 0.55f * ui_alpha));
    int fill = max > 0 ? w * value / max : 0;
    DrawRectangle(x, y, fill, 4, Fade(c, ui_alpha));
    DrawRectangle(x, y, fill, 1, Fade(RAYWHITE, 0.35f * ui_alpha));
}

static unsigned tile_hash(int tx, int ty)
{
    unsigned h = (unsigned)(tx * 73856093) ^ (unsigned)(ty * 19349663);
    return h ^ (h >> 13);
}

static const char *species_name(Species s)
{
    static const TextId names[SPECIES_COUNT] = { TXT_SLIME, TXT_BEETLE, TXT_WOLF, TXT_EMBER, TXT_ELDER_STUMP };
    return text_get(names[s]);
}

// The monster's name, with a letter when there are two of the same kind.
static const char *enemy_name(const Battle *b, int i)
{
    static char buf[4][48];
    static int slot = 0;
    char *out = buf[slot = (slot + 1) % 4];
    int same = 0, index = 0;
    for (int k = 0; k < b->enemy_count; k++) {
        if (b->enemies[k].species != b->enemies[i].species) continue;
        if (k < i) index++;
        same++;
    }
    if (same > 1) snprintf(out, 48, "%s %c", species_name(b->enemies[i].species), 'A' + index);
    else snprintf(out, 48, "%s", species_name(b->enemies[i].species));
    return out;
}

// --- Characters ---------------------------------------------------------------------

static bool draw_hero_sprite(float cx, float cy, float s, int face_x, int face_y, float walk, float swing);
static bool draw_goddess_sprite(float cx, float cy, float s, int face_x, int face_y, float walk);
static bool draw_fallen_sprite(float cx, float cy, float s);

// The lumberjack, centered at (cx, cy) with feet near cy + 7*s. `swing`
// goes from 0 to 1 during an axe swing.
static void draw_lumberjack(float cx, float cy, float s, int face_x, int face_y, float walk, float swing)
{
    if (draw_hero_sprite(cx, cy, s, face_x, face_y, walk, swing)) return;
    float bob = walk > 0 ? fabsf(sinf(walk * 0.3f)) * s : 0.0f;
    float y = cy - bob;
    DrawEllipse((int)cx, (int)(cy + 7 * s), 5 * s, 2 * s, SHADOW);
    // Legs.
    float step = walk > 0 ? sinf(walk * 0.3f) * 1.5f * s : 0.0f;
    DrawRectangle((int)(cx - 3 * s), (int)(y + 3 * s + step), (int)(2 * s), (int)(4 * s), PANTS);
    DrawRectangle((int)(cx + 1 * s), (int)(y + 3 * s - step), (int)(2 * s), (int)(4 * s), PANTS);
    // Plaid shirt.
    DrawRectangle((int)(cx - 4 * s), (int)(y - 3 * s), (int)(8 * s), (int)(7 * s), SHIRT);
    DrawRectangle((int)(cx - 4 * s), (int)(y - 1 * s), (int)(8 * s), (int)(1 * s), SHIRT_DARK);
    DrawRectangle((int)(cx - 1 * s), (int)(y - 3 * s), (int)(1 * s), (int)(7 * s), SHIRT_DARK);
    // Head, beard, cap.
    DrawRectangle((int)(cx - 3 * s), (int)(y - 9 * s), (int)(6 * s), (int)(6 * s), SKIN);
    if (face_y >= 0) {
        DrawRectangle((int)(cx - 3 * s), (int)(y - 5 * s), (int)(6 * s), (int)(3 * s), BEARD);
        int ex = (int)(cx + face_x * 1 * s);
        if (face_x >= 0) DrawRectangle(ex + (int)(1 * s), (int)(y - 7 * s), (int)fmaxf(1, s), (int)fmaxf(1, s), INK);
        if (face_x <= 0) DrawRectangle(ex - (int)(2 * s), (int)(y - 7 * s), (int)fmaxf(1, s), (int)fmaxf(1, s), INK);
    } else {
        DrawRectangle((int)(cx - 3 * s), (int)(y - 6 * s), (int)(6 * s), (int)(2 * s), BEARD);
    }
    DrawRectangle((int)(cx - 4 * s), (int)(y - 11 * s), (int)(8 * s), (int)(3 * s), CAP);
    DrawRectangle((int)(cx - 3 * s + face_x * 2 * s), (int)(y - 9 * s), (int)(6 * s), (int)(1 * s), CAP);

    // The axe: resting upright in his hand, or swinging toward where he
    // faces. The angle is measured from straight up, toward the front.
    float d = face_x != 0 ? (float)face_x : 1.0f;
    float angle = (swing > 0 ? -40.0f + swing * 160.0f : 15.0f) * DEG2RAD;
    Vector2 hand = { cx + d * 5 * s, y + 1 * s };
    Vector2 tip = { hand.x + d * sinf(angle) * 10 * s, hand.y - cosf(angle) * 10 * s };
    DrawLineEx(hand, tip, fmaxf(1.0f, 1.5f * s), TRUNK_LIGHT);
    DrawCircleV(tip, 2.5f * s, STEEL);
    DrawCircleV((Vector2){ tip.x - d * 1.0f * s, tip.y }, 1.2f * s, (Color){ 140, 150, 160, 255 });
}

// The Moon Goddess, centered at (cx, cy) with feet near cy + 7*s: long
// white hair, a white dress and a silver crescent, with a soft glow.
static void draw_goddess(float cx, float cy, float s, float t, int face_x, int face_y, float walk)
{
    float glow = 0.18f + 0.07f * sinf(t * 2.0f);
    DrawCircle((int)cx, (int)(cy - 2 * s), 13 * s, Fade(MOON_SILVER, glow));
    if (draw_goddess_sprite(cx, cy, s, face_x, face_y, walk)) return;
    DrawEllipse((int)cx, (int)(cy + 7 * s), 5 * s, 2 * s, SHADOW);
    // Hair falling behind her back.
    DrawRectangle((int)(cx - 4 * s), (int)(cy - 9 * s), (int)(8 * s), (int)(12 * s), MOON_HAIR);
    // Dress, wider at the hem.
    DrawTriangle((Vector2){ cx, cy - 4 * s }, (Vector2){ cx - 6 * s, cy + 7 * s }, (Vector2){ cx + 6 * s, cy + 7 * s }, MOON_WHITE);
    DrawRectangle((int)(cx - 3 * s), (int)(cy - 4 * s), (int)(6 * s), (int)(5 * s), MOON_WHITE);
    DrawRectangle((int)(cx - 3 * s), (int)(cy - 1 * s), (int)(6 * s), (int)fmaxf(1, s), MOON_SILVER);  // belt
    // Face and the hair around it.
    DrawRectangle((int)(cx - 2 * s), (int)(cy - 9 * s), (int)(5 * s), (int)(5 * s), MOON_SKIN);
    DrawRectangle((int)(cx - 3 * s), (int)(cy - 10 * s), (int)(7 * s), (int)(2 * s), MOON_HAIR);
    DrawRectangle((int)(cx - 3 * s), (int)(cy - 9 * s), (int)fmaxf(1, s), (int)(7 * s), MOON_HAIR);
    DrawRectangle((int)(cx + 3 * s), (int)(cy - 9 * s), (int)fmaxf(1, s), (int)(7 * s), MOON_HAIR);
    DrawRectangle((int)(cx - 1 * s), (int)(cy - 7 * s), (int)fmaxf(1, s), (int)fmaxf(1, s), INK);
    DrawRectangle((int)(cx + 1 * s), (int)(cy - 7 * s), (int)fmaxf(1, s), (int)fmaxf(1, s), INK);
    // Crescent moon above her brow.
    DrawRing((Vector2){ cx, cy - 12 * s }, 1.3f * s, 2.6f * s, 20, 200, 10, MOON_SILVER);
}

// The goddess lying on the ground, before the lumberjack saves her.
static void draw_goddess_fallen(float cx, float cy, float s, float t)
{
    DrawCircle((int)cx, (int)cy, 10 * s, Fade(MOON_SILVER, 0.12f + 0.05f * sinf(t * 2.0f)));
    if (draw_fallen_sprite(cx, cy, s)) return;
    DrawEllipse((int)cx, (int)(cy + 3 * s), 8 * s, 2 * s, SHADOW);
    DrawRectangle((int)(cx - 2 * s), (int)(cy - 1 * s), (int)(9 * s), (int)(4 * s), MOON_WHITE);  // dress
    DrawRectangle((int)(cx - 8 * s), (int)(cy - 2 * s), (int)(6 * s), (int)(5 * s), MOON_HAIR);   // hair
    DrawRectangle((int)(cx - 6 * s), (int)(cy - 1 * s), (int)(3 * s), (int)(3 * s), MOON_SKIN);   // face
}

// --- Images from assets/ ----------------------------------------------------------------

// The author's sprites. Anything missing is drawn with shapes instead.
// The art has ART pixels for each pixel of the 320x180 layout: a map tile
// is 16x16 on the layout and 32x32 in its image.
#define ART 2
#define MAX_GRASS 8
#define MAX_VARIANTS 4
#define MAX_LOADED 160
typedef struct {
    Texture2D v[MAX_VARIANTS];
    int count;
} Variants;
static struct {
    Texture2D grass[MAX_GRASS];  // 64x64 each: one covers a block of 2x2 tiles
    int grass_count;
    Variants oak, pine, birch;  // 32x48: the top 16 rows hang over the tile above
    Variants rock, bush, flowers, path, water;
    Texture2D stump, house, bridge_v, bridge_h;
    Texture2D path_edge[4][3];  // n, s, w, e; three variants each
    Texture2D path_inner[4];    // ne, nw, se, sw: grass in the inside of a turn
    Texture2D path_outer[4];    // ne, nw, se, sw: rounds the outside of a turn
    Texture2D bank[4][3];       // the same, for the shore of water
    Texture2D bank_inner[4], bank_outer[4];
    Texture2D slime[2], beetle[2], wolf[2], ember[2], elder[2];  // two frames each
    Texture2D elder_sleep;
    Texture2D hero[3][3], goddess[3][3];  // facing s, n, e; standing and two steps
    Texture2D goddess_fallen, chest, chest_open, woodpile, fire[3];
    bool has_stump, has_path_edges, has_banks, has_house, has_bridge, has_hero, has_goddess, has_fire;
    Texture2D loaded[MAX_LOADED];  // everything, to unload
    int loaded_count;
} art;

static bool load_art_texture(const char *name, Texture2D *out)
{
    const EmbeddedAsset *a = asset_find(name);
    if (!a || art.loaded_count >= MAX_LOADED) return false;
    Image img = LoadImageFromMemory(".png", a->data, (int)a->size);
    if (!img.data) return false;
    *out = LoadTextureFromImage(img);
    UnloadImage(img);
    if (out->id == 0) return false;
    SetTextureFilter(*out, TEXTURE_FILTER_POINT);
    art.loaded[art.loaded_count++] = *out;
    return true;
}

// Loads name_1, name_2, ... while they exist.
static void load_variants(Variants *v, const char *name)
{
    for (int i = 0; i < MAX_VARIANTS; i++)
        if (load_art_texture(TextFormat("%s_%d", name, i + 1), &v->v[v->count])) v->count++;
}

// Loads a set of edges (name_n_1 .. name_e_3), inner corners (name_in_ne ..)
// and outer corners (name_out_ne ..; these may be missing).
static bool load_edges(const char *name, Texture2D edges[4][3], Texture2D inner[4], Texture2D outer[4])
{
    static const char *const sides[4] = { "n", "s", "w", "e" }, *const corners[4] = { "ne", "nw", "se", "sw" };
    bool ok = true;
    for (int i = 0; i < 4; i++) {
        for (int v = 0; v < 3; v++) ok &= load_art_texture(TextFormat("%s_%s_%d", name, sides[i], v + 1), &edges[i][v]);
        ok &= load_art_texture(TextFormat("%s_in_%s", name, corners[i]), &inner[i]);
        load_art_texture(TextFormat("%s_out_%s", name, corners[i]), &outer[i]);
    }
    return ok;
}

static void load_art(void)
{
    for (int i = 0; i < MAX_GRASS; i++)
        if (load_art_texture(TextFormat("tiles/grass_%d", i + 1), &art.grass[art.grass_count])) art.grass_count++;
    load_variants(&art.oak, "tiles/tree");
    load_variants(&art.pine, "tiles/pine");
    load_variants(&art.birch, "tiles/birch");
    load_variants(&art.rock, "tiles/rock");
    load_variants(&art.bush, "tiles/bush");
    load_variants(&art.flowers, "tiles/flowers");
    load_variants(&art.path, "tiles/path");
    load_variants(&art.water, "tiles/water");
    art.has_stump = load_art_texture("tiles/stump", &art.stump);
    art.has_house = load_art_texture("tiles/house", &art.house);
    art.has_bridge = load_art_texture("tiles/bridge_v", &art.bridge_v) && load_art_texture("tiles/bridge_h", &art.bridge_h);
    art.has_path_edges = load_edges("tiles/path_edge", art.path_edge, art.path_inner, art.path_outer);
    art.has_banks = load_edges("tiles/bank", art.bank, art.bank_inner, art.bank_outer);
    for (int f = 0; f < 2; f++) {
        load_art_texture(TextFormat("monsters/slime_%d", f + 1), &art.slime[f]);
        load_art_texture(TextFormat("monsters/beetle_%d", f + 1), &art.beetle[f]);
        load_art_texture(TextFormat("monsters/wolf_%d", f + 1), &art.wolf[f]);
        load_art_texture(TextFormat("monsters/ember_%d", f + 1), &art.ember[f]);
        load_art_texture(TextFormat("monsters/stump_%d", f + 1), &art.elder[f]);
    }
    static const char *const dirs[3] = { "s", "n", "e" };
    art.has_hero = art.has_goddess = true;
    for (int d = 0; d < 3; d++)
        for (int f = 0; f < 3; f++) {
            art.has_hero &= load_art_texture(TextFormat("chars/hero_%s_%d", dirs[d], f + 1), &art.hero[d][f]);
            art.has_goddess &= load_art_texture(TextFormat("chars/goddess_%s_%d", dirs[d], f + 1), &art.goddess[d][f]);
        }
    load_art_texture("chars/goddess_fallen", &art.goddess_fallen);
    load_art_texture("monsters/stump_sleep", &art.elder_sleep);
    load_art_texture("tiles/chest", &art.chest);
    load_art_texture("tiles/chest_open", &art.chest_open);
    load_art_texture("tiles/woodpile", &art.woodpile);
    art.has_fire = true;
    for (int f = 0; f < 3; f++) art.has_fire &= load_art_texture(TextFormat("tiles/fire_%d", f + 1), &art.fire[f]);
}

// Draws an image at its own detail, top-left at (x, y) on the layout.
static void art_draw_v(Texture2D t, Vector2 at)
{
    DrawTexturePro(t, (Rectangle){ 0, 0, (float)t.width, (float)t.height },
                   (Rectangle){ at.x, at.y, t.width / (float)ART, t.height / (float)ART }, (Vector2){ 0, 0 }, 0, WHITE);
}

// Draws a figure standing with its feet on (cx, foot), `k` times its own
// size (whole numbers only, so pixels stay square), flipped if asked.
static void art_draw_figure(Texture2D t, float cx, float foot, float k, bool flip, Color tint)
{
    float w = t.width * k / ART, h = t.height * k / ART;
    Rectangle src = { 0, 0, (float)(flip ? -t.width : t.width), (float)t.height };
    Rectangle dst = { roundf((cx - w / 2) * ART) / ART, roundf((foot - h) * ART) / ART, w, h };
    DrawTexturePro(t, src, dst, (Vector2){ 0, 0 }, 0, tint);
}

// Which way a person faces, as a row of art.hero / art.goddess, and the
// frame: standing, or one of two steps while walking.
static int facing_row(int face_x, int face_y) { return face_y < 0 && face_x == 0 ? 1 : face_x != 0 ? 2 : 0; }
static int walk_frame(float walk) { return walk > 0 ? (sinf(walk * 0.3f) > 0 ? 1 : 2) : 0; }

static bool draw_hero_sprite(float cx, float cy, float s, int face_x, int face_y, float walk, float swing)
{
    if (!art.has_hero) return false;
    float k = fmaxf(1.0f, roundf(s));
    float bob = walk > 0 ? fabsf(sinf(walk * 0.3f)) * 0.5f * s : 0.0f;
    float y = cy - bob;
    int row = facing_row(face_x, face_y);
    DrawEllipse((int)cx, (int)(cy + 7 * s), 5 * s, 2 * s, SHADOW);
    // The axe: resting upright in his hand, or swinging toward where he
    // faces. The angle is measured from straight up, toward the front.
    float d = face_x != 0 ? (float)face_x : 1.0f;
    float angle = (swing > 0 ? -40.0f + swing * 160.0f : 15.0f) * DEG2RAD;
    Vector2 hand = { cx + d * (row == 2 ? 1.5f : 4.5f) * s, y + 1.5f * s };
    Vector2 tip = { hand.x + d * sinf(angle) * 10 * s, hand.y - cosf(angle) * 10 * s };
    bool behind = row == 1;
    for (int pass = 0; pass < 2; pass++) {
        if ((pass == 0) == behind) {
            DrawLineEx(hand, tip, fmaxf(1.0f, 1.0f * s), TRUNK_LIGHT);
            DrawLineEx((Vector2){ hand.x + 0.5f, hand.y }, (Vector2){ tip.x + 0.5f, tip.y }, 0.5f * s, TRUNK);
            DrawCircleV(tip, 2.5f * s, (Color){ 120, 128, 140, 255 });
            DrawCircleV((Vector2){ tip.x + d * 0.5f * s, tip.y - 0.5f * s }, 1.8f * s, STEEL);
        } else {
            art_draw_figure(art.hero[row][walk_frame(walk)], cx, y + 8 * s, k, face_x < 0, WHITE);
        }
    }
    return true;
}

static bool draw_goddess_sprite(float cx, float cy, float s, int face_x, int face_y, float walk)
{
    if (!art.has_goddess) return false;
    float k = fmaxf(1.0f, roundf(s));
    DrawEllipse((int)cx, (int)(cy + 7 * s), 5 * s, 2 * s, SHADOW);
    int row = facing_row(face_x, face_y);
    art_draw_figure(art.goddess[row][walk_frame(walk)], cx, cy + 8 * s, k, face_x < 0, WHITE);
    return true;
}

static bool draw_fallen_sprite(float cx, float cy, float s)
{
    if (!art.goddess_fallen.id) return false;
    DrawEllipse((int)cx, (int)(cy + 3 * s), 10 * s, 2 * s, SHADOW);
    art_draw_figure(art.goddess_fallen, cx, cy + 5 * s, fmaxf(1.0f, roundf(s)), false, WHITE);
    return true;
}

static void unload_art(void)
{
    for (int i = 0; i < art.loaded_count; i++) UnloadTexture(art.loaded[i]);
    memset(&art, 0, sizeof art);
}

// A monster from its sprite, standing on (cx, cy + 5 * s). False if there
// is no sprite for it.
static bool draw_monster_sprite(Species sp, float cx, float cy, float s, float t, bool flash, int facing)
{
    const Texture2D *frames = sp == SPECIES_SLIME ? art.slime : sp == SPECIES_BEETLE ? art.beetle
                            : sp == SPECIES_WOLF ? art.wolf : sp == SPECIES_EMBER ? art.ember
                            : sp == SPECIES_ELDER_STUMP ? art.elder : NULL;
    if (!frames || !frames[0].id || !frames[1].id) return false;
    float speed = sp == SPECIES_SLIME ? 3.0f : sp == SPECIES_WOLF ? 6.0f : sp == SPECIES_ELDER_STUMP ? 1.5f : 8.0f;
    Texture2D tex = frames[(int)(t * speed) & 1];
    float k = fmaxf(1.0f, roundf(s));  // whole pixels only
    bool flip = facing < 0 && sp == SPECIES_WOLF;  // drawn facing right
    float foot = cy + (sp == SPECIES_ELDER_STUMP ? 9 : 5) * s;
    if (sp == SPECIES_EMBER) {  // it floats
        foot += sinf(t * 6.0f) * s - 2 * s;
        DrawCircle((int)cx, (int)(foot - 6 * s), 8 * s, Fade(FIRE_A, 0.2f));
    }
    art_draw_figure(tex, cx, foot, k, flip, WHITE);
    if (flash) {  // hit: light it up
        BeginBlendMode(BLEND_ADDITIVE);
        art_draw_figure(tex, cx, foot, k, flip, WHITE);
        art_draw_figure(tex, cx, foot, k, flip, WHITE);
        EndBlendMode();
    }
    return true;
}

// A monster centered at (cx, cy). `s` is the size (1 on the map).
static void draw_monster(Species sp, float cx, float cy, float s, float t, bool flash, int facing)
{
    Color f = RAYWHITE;
    DrawEllipse((int)cx, (int)(cy + 6 * s), 7 * s, 2 * s, SHADOW);
    if (draw_monster_sprite(sp, cx, cy, s, t, flash, facing)) return;
    switch (sp) {
    case SPECIES_SLIME: {
        float wob = sinf(t * 5.0f);
        DrawEllipse((int)cx, (int)(cy + 1 * s), (6 + wob * 0.6f) * s, (5 - wob * 0.5f) * s, flash ? f : SLIME_DARK);
        DrawEllipse((int)cx, (int)cy, (5 + wob * 0.6f) * s, (4 - wob * 0.5f) * s, flash ? f : SLIME_C);
        DrawRectangle((int)(cx - 3 * s), (int)(cy - 3 * s), (int)(2 * s), (int)fmaxf(1, s), Fade(RAYWHITE, 0.8f));
        DrawRectangle((int)(cx - 2 * s), (int)cy, (int)fmaxf(1, s), (int)(2 * s), INK);
        DrawRectangle((int)(cx + 2 * s), (int)cy, (int)fmaxf(1, s), (int)(2 * s), INK);
        break;
    }
    case SPECIES_BEETLE: {
        float legs = sinf(t * 18.0f);
        for (int k = -1; k <= 1; k++) {
            DrawRectangle((int)(cx - 7 * s), (int)(cy + (k * 3 + legs) * s), (int)(2 * s), (int)fmaxf(1, s), INK);
            DrawRectangle((int)(cx + 5 * s), (int)(cy + (k * 3 - legs) * s), (int)(2 * s), (int)fmaxf(1, s), INK);
        }
        DrawEllipse((int)cx, (int)cy, 6 * s, 6 * s, flash ? f : BEETLE_DARK);
        DrawEllipse((int)cx, (int)(cy - 1 * s), 5 * s, 5 * s, flash ? f : BEETLE_C);
        DrawRectangle((int)cx, (int)(cy - 5 * s), (int)fmaxf(1, s), (int)(10 * s), BEETLE_DARK);
        DrawRectangle((int)(cx - 3 * s), (int)(cy - 7 * s), (int)(2 * s), (int)(2 * s), INK);
        DrawRectangle((int)(cx + 2 * s), (int)(cy - 7 * s), (int)(2 * s), (int)(2 * s), INK);
        break;
    }
    case SPECIES_WOLF: {
        float d = (float)(facing < 0 ? -1 : 1);
        float run = sinf(t * 10.0f) * s;
        DrawRectangle((int)(cx - 5 * s), (int)(cy + 2 * s + run), (int)(2 * s), (int)(4 * s), flash ? f : WOLF_DARK);
        DrawRectangle((int)(cx + 3 * s), (int)(cy + 2 * s - run), (int)(2 * s), (int)(4 * s), flash ? f : WOLF_DARK);
        DrawEllipse((int)cx, (int)cy, 7 * s, 4 * s, flash ? f : WOLF_C);
        DrawCircle((int)(cx + d * 6 * s), (int)(cy - 3 * s), 3.5f * s, flash ? f : WOLF_C);
        DrawTriangle((Vector2){ cx + d * 5 * s, cy - 8 * s }, (Vector2){ cx + d * 4 * s, cy - 4 * s },
                     (Vector2){ cx + d * 7 * s, cy - 5 * s }, WOLF_DARK);
        DrawTriangle((Vector2){ cx + d * 5 * s, cy - 8 * s }, (Vector2){ cx + d * 7 * s, cy - 5 * s },
                     (Vector2){ cx + d * 4 * s, cy - 4 * s }, WOLF_DARK);
        DrawRectangle((int)(cx + d * 7 * s), (int)(cy - 4 * s), (int)fmaxf(1, s), (int)fmaxf(1, s), EYE_GLOW);
        DrawRectangle((int)(cx - d * 9 * s), (int)(cy - 2 * s), (int)(3 * s), (int)(2 * s), WOLF_DARK);  // tail
        break;
    }
    case SPECIES_EMBER: {
        // A small living flame with two dark eyes, bobbing in the air.
        float bob = sinf(t * 6.0f) * s, flick = sinf(t * 17.0f) * s;
        float y = cy - 2 * s + bob;
        DrawCircle((int)cx, (int)y, 8 * s, Fade(FIRE_A, 0.2f));
        DrawCircle((int)cx, (int)(y + 2 * s), 5 * s, flash ? f : EMBER_RED);
        DrawTriangle((Vector2){ cx + flick, y - 9 * s }, (Vector2){ cx - 5 * s, y + 1 * s }, (Vector2){ cx + 5 * s, y + 1 * s },
                     flash ? f : EMBER_RED);
        DrawCircle((int)cx, (int)(y + 3 * s), 3.2f * s, FIRE_A);
        DrawTriangle((Vector2){ cx - flick * 0.5f, y - 4 * s }, (Vector2){ cx - 3 * s, y + 2 * s }, (Vector2){ cx + 3 * s, y + 2 * s },
                     FIRE_A);
        DrawCircle((int)cx, (int)(y + 4 * s), 1.6f * s, FIRE_B);
        DrawRectangle((int)(cx - 2 * s), (int)(y + 1 * s), (int)fmaxf(1, s), (int)(2 * s), INK);
        DrawRectangle((int)(cx + 1 * s), (int)(y + 1 * s), (int)fmaxf(1, s), (int)(2 * s), INK);
        // Sparks.
        float k = fmodf(t * 1.7f, 1.0f);
        DrawRectangle((int)(cx + 4 * s), (int)(y - 2 * s - k * 6 * s), (int)fmaxf(1, s), (int)fmaxf(1, s), Fade(FIRE_B, 1 - k));
        DrawRectangle((int)(cx - 5 * s), (int)(y - k * 5 * s), (int)fmaxf(1, s), (int)fmaxf(1, s), Fade(FIRE_A, 1 - k));
        break;
    }
    case SPECIES_ELDER_STUMP: {
        float sway = sinf(t * 1.5f) * s;
        // Roots and trunk.
        DrawEllipse((int)cx, (int)(cy + 7 * s), 11 * s, 3 * s, BARK_DARK);
        DrawRectangle((int)(cx - 9 * s), (int)(cy - 8 * s), (int)(18 * s), (int)(16 * s), flash ? f : BARK);
        DrawEllipse((int)cx, (int)(cy - 8 * s), 9 * s, 3 * s, flash ? f : TRUNK_LIGHT);
        DrawEllipse((int)cx, (int)(cy - 8 * s), 6 * s, 2 * s, BARK);
        // Moss and a few leaves.
        DrawEllipse((int)(cx - 5 * s + sway), (int)(cy - 10 * s), 4 * s, 2 * s, MOSS);
        DrawEllipse((int)(cx + 6 * s + sway), (int)(cy - 9 * s), 3 * s, 1.5f * s, MOSS);
        // Branch arms.
        DrawLineEx((Vector2){ cx - 9 * s, cy - 2 * s }, (Vector2){ cx - 14 * s, cy - 8 * s + sway }, 2 * s, BARK_DARK);
        DrawLineEx((Vector2){ cx + 9 * s, cy - 2 * s }, (Vector2){ cx + 14 * s, cy - 8 * s - sway }, 2 * s, BARK_DARK);
        // Glowing eyes and mouth.
        DrawRectangle((int)(cx - 5 * s), (int)(cy - 4 * s), (int)(3 * s), (int)(2 * s), EYE_GLOW);
        DrawRectangle((int)(cx + 2 * s), (int)(cy - 4 * s), (int)(3 * s), (int)(2 * s), EYE_GLOW);
        DrawRectangle((int)(cx - 4 * s), (int)(cy + 2 * s), (int)(8 * s), (int)(2 * s), BARK_DARK);
        for (int k = -7; k <= 7; k += 4) DrawRectangle((int)(cx + k * s), (int)(cy - 6 * s), (int)fmaxf(1, s), (int)(12 * s), BARK_DARK);
        break;
    }
    default:
        break;
    }
}

// --- Map ------------------------------------------------------------------------------

// The corners of a patch of dirt or water. n, s, w, e: whether the patch
// goes on that way; diag: whether it goes on to the ne, nw, se, sw. Where it
// stops on two sides the corner is rounded; where it goes on both ways but
// not across, a little of the land shows in the inside corner.
static void draw_corners(Vector2 at, bool n, bool s, bool w, bool e, const bool diag[4], const Texture2D inner[4],
                         const Texture2D outer[4])
{
    bool ns[4] = { n, n, s, s }, we[4] = { e, w, e, w };  // ne, nw, se, sw
    for (int i = 0; i < 4; i++) {
        if (ns[i] && we[i] && !diag[i]) art_draw_v(inner[i], at);
        else if (!ns[i] && !we[i] && outer[i].id) art_draw_v(outer[i], at);
    }
}

// Dirt path or a path out of the area; outside the map counts as path, so
// paths leaving the area run off the edge.
static bool pathish(const Game *g, int tx, int ty)
{
    if (tx < 0 || ty < 0 || tx >= g->w || ty >= g->h) return true;
    TileType t = game_tile_at(g, tx, ty);
    return t == TILE_PATH || t == TILE_EXIT;
}

// The dirt, with grass creeping over every side that does not continue the
// path, and a bit of grass in the inside corner of turns.
static void draw_path_art(const Game *g, int x, int y, int tx, int ty)
{
    Vector2 at = { (float)x, (float)y };
    art_draw_v(art.path.v[tile_hash(tx, ty) % (unsigned)art.path.count], at);
    if (!art.has_path_edges) return;
    bool n = pathish(g, tx, ty - 1), s = pathish(g, tx, ty + 1), w = pathish(g, tx - 1, ty), e = pathish(g, tx + 1, ty);
    bool open[4] = { !n, !s, !w, !e };
    for (int i = 0; i < 4; i++)
        if (open[i]) art_draw_v(art.path_edge[i][tile_hash(tx * 4 + i, ty) % 3u], at);
    bool diag[4] = { pathish(g, tx + 1, ty - 1), pathish(g, tx - 1, ty - 1), pathish(g, tx + 1, ty + 1), pathish(g, tx - 1, ty + 1) };
    draw_corners(at, n, s, w, e, diag, art.path_inner, art.path_outer);
}

static void draw_grass(int x, int y, int tx, int ty)
{
    if (art.grass_count > 0) {
        // Each 2x2 block of tiles shows one of the grass images, picked by
        // place so the ground does not repeat in an obvious pattern.
        int bx = tx >> 1, by = ty >> 1;
        Texture2D t = art.grass[tile_hash(bx, by) % (unsigned)art.grass_count];
        Rectangle src = { (float)((tx & 1) * TILE * ART), (float)((ty & 1) * TILE * ART), TILE * ART, TILE * ART };
        DrawTexturePro(t, src, (Rectangle){ x, y, TILE, TILE }, (Vector2){ 0, 0 }, 0, WHITE);
        return;
    }
    DrawRectangle(x, y, TILE, TILE, ((tx + ty) & 1) ? GRASS_A : GRASS_B);
    unsigned h = tile_hash(tx, ty);
    if (h % 3 == 0) {
        int gx = x + 2 + (int)(h >> 4) % 10, gy = y + 4 + (int)(h >> 8) % 9;
        DrawRectangle(gx, gy, 1, 2, GRASS_TUFT);
        DrawRectangle(gx + 2, gy - 1, 1, 3, GRASS_TUFT);
    }
    if (h % 19 == 0) DrawRectangle(x + 4 + (int)(h >> 5) % 8, y + 3 + (int)(h >> 9) % 9, 2, 2, FLOWER);
}

static bool wet(const Game *g, int tx, int ty)
{
    TileType t = game_tile_at(g, tx, ty);
    return t == TILE_WATER || t == TILE_BRIDGE;
}

// Open water, with a shore on every side that is land.
static void draw_water(const Game *g, int x, int y, int tx, int ty, float t)
{
    if (art.water.count == 0) {
        DrawRectangle(x, y, TILE, TILE, WATER);
        float w = sinf(t * 2.0f + tx * 0.9f + ty * 0.6f);
        DrawRectangle(x + 2 + (int)(w * 2), y + 4, 5, 1, WATER_LIGHT);
        DrawRectangle(x + 8 - (int)(w * 2), y + 11, 5, 1, WATER_LIGHT);
        return;
    }
    Vector2 at = { (float)x, (float)y };
    art_draw_v(art.water.v[(int)(t * 1.6f) % art.water.count], at);
    if (!art.has_banks || game_tile_at(g, tx, ty) != TILE_WATER) return;
    bool n = wet(g, tx, ty - 1), s = wet(g, tx, ty + 1), w = wet(g, tx - 1, ty), e = wet(g, tx + 1, ty);
    bool open[4] = { !n, !s, !w, !e };
    for (int i = 0; i < 4; i++)
        if (open[i]) art_draw_v(art.bank[i][tile_hash(tx * 4 + i, ty) % 3u], at);
    bool diag[4] = { wet(g, tx + 1, ty - 1), wet(g, tx - 1, ty - 1), wet(g, tx + 1, ty + 1), wet(g, tx - 1, ty + 1) };
    draw_corners(at, n, s, w, e, diag, art.bank_inner, art.bank_outer);
}

// Trees grow in groves of one kind, and each area has its own mix:
// out of 10, how many oaks and pines (the rest are birches).
static const Texture2D *tree_texture(const Game *g, int tx, int ty)
{
    static const unsigned char mix[][2] = {
        [AREA_HOME] = { 6, 1 }, [AREA_WOODS] = { 5, 3 }, [AREA_VALLEY] = { 3, 6 }, [AREA_CLEARING] = { 7, 2 },
    };
    int area = g->area >= 0 && g->area < (int)(sizeof mix / sizeof mix[0]) ? g->area : 0;
    unsigned h = tile_hash(tx, ty);
    int gx = (tx + (int)(h & 3) + 64) / 4, gy = (ty + (int)((h >> 2) & 3) + 64) / 4;
    unsigned roll = tile_hash(gx + 101 * area, gy * 7 + 3) % 10u;
    const Variants *v = roll < mix[area][0] ? &art.oak : roll < (unsigned)(mix[area][0] + mix[area][1]) ? &art.pine : &art.birch;
    if (v->count == 0) v = &art.oak;
    if (v->count == 0) return NULL;
    return &v->v[(h >> 5) % (unsigned)v->count];
}

static bool is_house(TileType t) { return t == TILE_HOUSE || t == TILE_DOOR; }

// The ground: grass, dirt, water and flowers.
static void draw_ground(const Game *g, int x, int y, int tx, int ty, TileType tile, float t)
{
    switch (tile) {
    case TILE_WATER:
        draw_water(g, x, y, tx, ty, t);
        return;
    case TILE_BRIDGE: {
        draw_water(g, x, y, tx, ty, t);
        // Water (not more bridge) beside it: walked north-south.
        bool across = game_tile_at(g, tx - 1, ty) == TILE_WATER || game_tile_at(g, tx + 1, ty) == TILE_WATER;
        if (art.has_bridge) {
            art_draw_v(across ? art.bridge_v : art.bridge_h, (Vector2){ (float)x, (float)y });
            return;
        }
        DrawRectangle(x + 1, y, TILE - 2, TILE, PLANK);
        for (int i = 3; i < TILE; i += 4) DrawRectangle(x + 1, y + i, TILE - 2, 1, PLANK_DARK);
        return;
    }
    default:
        break;
    }
    draw_grass(x, y, tx, ty);
    if (tile == TILE_PATH || tile == TILE_EXIT) {
        if (art.path.count > 0) {
            draw_path_art(g, x, y, tx, ty);
        } else {
            DrawRectangle(x, y + 2, TILE, TILE - 4, PATH);
            if (tile_hash(tx, ty) % 2) DrawRectangle(x + 4 + (int)(tile_hash(tx, ty) >> 5) % 8, y + 6, 2, 1, TRUNK_LIGHT);
        }
    } else if (tile == TILE_FLOWERS) {
        if (art.flowers.count > 0) {
            art_draw_v(art.flowers.v[tile_hash(tx, ty) % (unsigned)art.flowers.count], (Vector2){ (float)x, (float)y });
        } else {
            DrawRectangle(x + 4, y + 5, 2, 2, FLOWER);
            DrawRectangle(x + 10, y + 9, 2, 2, FLOWER);
        }
    }
}

// The house as one picture, from its top-left tile; or tile by tile.
static void draw_house_tile(const Game *g, int x, int y, int tx, int ty, TileType tile, float t)
{
    if (art.has_house) {
        if (is_house(game_tile_at(g, tx - 1, ty)) || is_house(game_tile_at(g, tx, ty - 1))) return;
        art_draw_v(art.house, (Vector2){ (float)(x - 2), (float)(y - 24) });
        float k = fmodf(t * 0.5f, 1.0f);  // smoke from the chimney
        for (int i = 0; i < 2; i++) {
            float kk = fmodf(k + i * 0.5f, 1.0f);
            DrawCircle(x + 51 + (int)(kk * 6), y - 26 - (int)(kk * 14), 2 + kk * 3, Fade(RAYWHITE, 0.45f * (1 - kk)));
        }
        return;
    }
    bool roof = !is_house(game_tile_at(g, tx, ty - 1));
    if (roof) {
        DrawRectangle(x - 1, y - 4, TILE + 2, TILE + 4, ROOF);
        for (int i = 0; i < TILE; i += 4) DrawRectangle(x - 1, y - 4 + i, TILE + 2, 1, ROOF_DARK);
        DrawRectangle(x - 1, y + TILE - 2, TILE + 2, 2, ROOF_DARK);
    } else {
        DrawRectangle(x, y, TILE, TILE, TRUNK_LIGHT);
        for (int i = 3; i < TILE; i += 4) DrawRectangle(x, y + i, TILE, 1, TRUNK);
        if (tile == TILE_DOOR) {
            DrawRectangle(x + 3, y + 2, 10, 14, PLANK_DARK);
            DrawRectangle(x + 4, y + 3, 8, 13, TRUNK);
        }
    }
}

// Things standing on the ground: trees, rocks, the house... Drawn after all
// of the ground, row by row, so tall things cover what is behind them.
static void draw_object(const Game *g, int x, int y, int tx, int ty, TileType tile, float t)
{
    switch (tile) {
    case TILE_WALL:  // the edge of the world: deep forest
    case TILE_TREE: {
        const Texture2D *tex = tree_texture(g, tx, ty);
        if (tex) {
            art_draw_v(*tex, (Vector2){ (float)x, (float)(y - 8) });
            break;
        }
        DrawEllipse(x + 8, y + 14, 7, 2, SHADOW);
        DrawRectangle(x + 6, y + 9, 4, 6, TRUNK);
        DrawCircle(x + 8, y + 6, 7.5f, LEAVES);
        DrawCircle(x + 6, y + 4, 3.5f, LEAVES_LIGHT);
        break;
    }
    case TILE_STUMP:
        if (art.has_stump) {
            art_draw_v(art.stump, (Vector2){ (float)x, (float)y });
            break;
        }
        DrawEllipse(x + 8, y + 10, 4, 3, TRUNK);
        DrawEllipse(x + 8, y + 9, 3, 2, TRUNK_LIGHT);
        break;
    case TILE_ROCK:
        if (art.rock.count > 0) {
            art_draw_v(art.rock.v[tile_hash(tx, ty) % (unsigned)art.rock.count], (Vector2){ (float)x, (float)y });
            break;
        }
        DrawEllipse(x + 8, y + 13, 7, 2, SHADOW);
        DrawEllipse(x + 8, y + 9, 7, 6, ROCK_DARK);
        DrawEllipse(x + 7, y + 8, 6, 5, ROCK);
        break;
    case TILE_BUSH:
        if (art.bush.count > 0) {
            // Berries on one bush in four.
            unsigned h = tile_hash(tx, ty);
            int v = art.bush.count > 1 && h % 4 == 0 ? 1 : 0;
            art_draw_v(art.bush.v[v], (Vector2){ (float)x, (float)y });
            break;
        }
        DrawEllipse(x + 8, y + 13, 7, 2, SHADOW);
        DrawCircle(x + 8, y + 9, 6, LEAVES);
        DrawCircle(x + 6, y + 7, 3, LEAVES_LIGHT);
        break;
    case TILE_CHEST:
    case TILE_CHEST_AXE:
        if (art.chest.id) {
            DrawEllipse(x + 8, y + 14, 7, 2, SHADOW);
            art_draw_v(art.chest, (Vector2){ x, y });
            break;
        }
        DrawEllipse(x + 8, y + 14, 7, 2, SHADOW);
        DrawRectangle(x + 2, y + 5, 12, 9, PLANK_DARK);
        DrawRectangle(x + 3, y + 6, 10, 3, PLANK);
        DrawRectangle(x + 3, y + 10, 10, 3, PLANK);
        DrawRectangle(x + 7, y + 8, 2, 3, GOLDEN);
        break;
    case TILE_CHEST_OPEN:
        if (art.chest_open.id) {
            DrawEllipse(x + 8, y + 14, 7, 2, SHADOW);
            art_draw_v(art.chest_open, (Vector2){ x, y });
            break;
        }
        DrawEllipse(x + 8, y + 14, 7, 2, SHADOW);
        DrawRectangle(x + 2, y + 3, 12, 3, PLANK);
        DrawRectangle(x + 2, y + 7, 12, 7, PLANK_DARK);
        DrawRectangle(x + 3, y + 8, 10, 2, INK);
        break;
    case TILE_FIRE: {
        if (art.has_fire) {
            float f = sinf(t * 12.0f + tx);
            DrawCircle(x + 8, y + 9, 7 + f * 0.5f, Fade(FIRE_A, 0.22f));
            art_draw_v(art.fire[(int)(t * 8.0f + tx) % 3], (Vector2){ x, y });
            break;
        }
        DrawCircle(x + 8, y + 11, 6, Fade(INK, 0.35f));  // ring of ash
        for (int i = 0; i < 6; i++) {                     // ring of stones
            float a = i * 1.047f;
            DrawCircle(x + 8 + (int)(cosf(a) * 6), y + 11 + (int)(sinf(a) * 3), 1.6f, ROCK);
        }
        DrawRectangle(x + 3, y + 11, 10, 2, TRUNK);
        DrawRectangle(x + 5, y + 10, 6, 2, TRUNK_LIGHT);
        float f = sinf(t * 12.0f + tx) * 1.0f;
        DrawCircle(x + 8, y + 9, 6 + f * 0.5f, Fade(FIRE_A, 0.25f));
        DrawTriangle((Vector2){ x + 8.0f + f, y + 2.0f }, (Vector2){ x + 4.0f, y + 11.0f },
                     (Vector2){ x + 12.0f, y + 11.0f }, FIRE_A);
        DrawTriangle((Vector2){ x + 8.0f - f, y + 6.0f }, (Vector2){ x + 6.0f, y + 11.0f },
                     (Vector2){ x + 10.0f, y + 11.0f }, FIRE_B);
        break;
    }
    case TILE_GODDESS:
        draw_goddess_fallen(x + 8.0f, y + 9.0f, 1.0f, t);
        break;
    case TILE_HOUSE:
    case TILE_DOOR:
        draw_house_tile(g, x, y, tx, ty, tile, t);
        break;
    case TILE_WOODPILE:
        if (art.woodpile.id) {
            DrawEllipse(x + 8, y + 14, 8, 2, SHADOW);
            art_draw_v(art.woodpile, (Vector2){ x, y });
            break;
        }
        DrawEllipse(x + 8, y + 14, 8, 2, SHADOW);
        for (int r = 0; r < 3; r++)
            for (int c = 0; c < 3 - r; c++) {
                int lx = x + 3 + c * 5 + r * 2, ly = y + 11 - r * 4;
                DrawCircle(lx, ly, 2.5f, TRUNK_LIGHT);
                DrawCircle(lx, ly, 1.0f, TRUNK);
            }
        break;
    default:
        break;
    }
}

static Species encounter_leader(Encounter e)
{
    Species party[MAX_BATTLE_ENEMIES];
    encounter_party(e, party);
    return party[0];
}

// The Elder Stump sleeps, with the fire creatures attacking him, until his
// scene; in it he wakes on the third page and they run on the fourth.
static bool elder_asleep(const Game *g)
{
    if (g->state == STATE_SCENE && g->scene == SCENE_ELDER) return g->scene_page < 2;
    return !g->elder_woke;
}

static bool elder_harassed(const Game *g)
{
    if (g->state == STATE_SCENE && g->scene == SCENE_ELDER) return g->scene_page < 3;
    return !g->elder_woke && g->goddess.joined;
}

// A fire creature floating by the Elder Stump; `flying` ones are high up,
// with their shadow on the ground far below.
static void draw_ember_at(float x, float y, float t, bool flying)
{
    float bob = roundf(sinf(t * 6.0f) * 1.5f);
    DrawEllipse((int)x, (int)(y + (flying ? 22 : 6)), 4, 1.5f, SHADOW);
    if (!art.ember[0].id) { draw_monster(SPECIES_EMBER, x, y + bob, 0.9f, t, false, 1); return; }
    BeginBlendMode(BLEND_ADDITIVE);  // warm light around it
    DrawCircleGradient((Vector2){ x, y + bob - 2 }, 9, Fade(FIRE_A, 0.35f), Fade(FIRE_A, 0.0f));
    EndBlendMode();
    art_draw_figure(art.ember[(int)(t * 8.0f) & 1], x, y + bob + 5, 1, false, WHITE);
}

static void draw_foe(const Game *g, int i, int cam_x, int cam_y)
{
    const Foe *f = &g->foes[i];
    float cx = f->x + WALKER_SIZE / 2.0f - cam_x, cy = f->y + WALKER_SIZE / 2.0f - cam_y;
    if (cx < -32 || cy < -48 || cx > VIEW_W + 32 || cy > VIEW_H + 32) return;
    Species sp = encounter_leader(f->kind);
    if (sp == SPECIES_ELDER_STUMP) {
        float t = g->time;
        bool harassed = elder_harassed(g);
        // Three fire creatures attack him: one on each side and one from
        // above. Each darts in until it touches him and backs off again, and
        // never goes over him, so his face stays in view.
        static const struct { float x, y, dx, dy; } spot[3] = {
            { -24, 0, 7, 0 }, { 24, -2, -7, 0 }, { 13, -26, -3, 5 },
        };
        DrawEllipse((int)cx, (int)(cy + 6), 10, 3, SHADOW);
        if (elder_asleep(g) && art.elder_sleep.id) {
            art_draw_figure(art.elder_sleep, cx, cy - 6 + 9 * 1.4f, 1, false, WHITE);
            float z = fmodf(t * 0.7f, 1.0f);  // snoring
            DrawText("z", (int)(cx - 14 - z * 6), (int)(cy - 26 - z * 10), 10, Fade(RAYWHITE, 1 - z));
        } else {
            if (!draw_monster_sprite(sp, cx, cy - 6, 1.4f, t + i, false, 1)) draw_monster(sp, cx, cy - 6, 1.4f, t + i, false, 1);
        }
        for (int k = 0; harassed && k < 3; k++) {
            float lunge = fmaxf(0.0f, sinf(t * 4.0f + k * 2.1f));
            lunge *= lunge;  // quick in, slow out
            float ex = roundf(cx + spot[k].x + spot[k].dx * lunge), ey = roundf(cy + spot[k].y + spot[k].dy * lunge);
            draw_ember_at(ex, ey, t + k * 0.4f, spot[k].y < -10);
        }
        return;
    }
    draw_monster(sp, cx, cy, 0.9f, g->time + i, false, f->vx < 0 ? -1 : 1);
    if (f->kind == ENCOUNTER_SLIMES || f->kind == ENCOUNTER_WOLVES || f->kind == ENCOUNTER_BEETLE)
        DrawText("+", (int)cx + 5, (int)cy - 12, 10, RAYWHITE);  // more than one monster
}

// The row of tiles something standing at top-left y is on.
static int row_of(float y) { return (int)floorf((y + WALKER_SIZE / 2.0f) / TILE); }

// Monsters, the goddess and the lumberjack standing on one row of tiles.
// They are drawn with the row's objects, so a tree or the house in front
// (lower on the screen) covers them, and they cover what is behind.
static void draw_actors_in_row(const Game *g, int row, int cam_x, int cam_y, float chop)
{
    for (int i = 0; i < g->foe_count; i++)
        if (g->foes[i].alive && row_of(g->foes[i].y) == row) draw_foe(g, i, cam_x, cam_y);
    if (g->state == STATE_TITLE || g->state == STATE_WON) return;
    const Walker *w = &g->walker, *f = &g->follower;
    bool with_her = g->goddess.joined;
    bool blink = g->state != STATE_SCENE && w->safe_timer > 0 && w->safe_timer < 1.9f && fmodf(w->safe_timer, 0.2f) < 0.1f;
    float fx = f->x + WALKER_SIZE / 2.0f - cam_x, fy = f->y + WALKER_SIZE / 2.0f - cam_y - 2;
    fy -= f->walk_anim > 0 ? fabsf(sinf(f->walk_anim * 0.3f)) : 0.0f;
    bool her_here = with_her && row_of(f->y) == row, him_here = !blink && row_of(w->y) == row;
    if (her_here && f->y < w->y) draw_goddess(fx, fy, 1.0f, g->time, f->face_x, f->face_y, f->walk_anim);  // behind him
    if (him_here)
        draw_lumberjack(w->x + WALKER_SIZE / 2.0f - cam_x, w->y + WALKER_SIZE / 2.0f - cam_y - 2, 1.0f,
                        w->face_x, w->face_y, w->walk_anim, chop);
    if (her_here && f->y >= w->y) draw_goddess(fx, fy, 1.0f, g->time, f->face_x, f->face_y, f->walk_anim);
}

static void draw_map(const Game *g, int cam_x, int cam_y, float chop)
{
    int x0 = (int)floorf(cam_x / (float)TILE), x1 = (cam_x + VIEW_W) / TILE + 1;
    int y0 = (int)floorf(cam_y / (float)TILE), y1 = (cam_y + VIEW_H) / TILE + 1;
    float t = g->time;
    for (int ty = y0; ty <= y1; ty++)
        for (int tx = x0; tx <= x1; tx++)
            draw_ground(g, tx * TILE - cam_x, ty * TILE - cam_y, tx, ty, game_tile_at(g, tx, ty), t);
    // Objects reach up into the row above, and the house up to 4 tiles left
    // and 3 up from where it is drawn: start a little outside the view.
    for (int ty = y0 - 3; ty <= y1 + 1; ty++) {
        for (int tx = x0 - 4; tx <= x1; tx++) {
            TileType tile = game_tile_at(g, tx, ty);
            bool far = ty < y0 || tx < x0 || ty > y1;
            if (far && !is_house(tile) && !(ty == y1 + 1 && tx >= x0)) continue;
            draw_object(g, tx * TILE - cam_x, ty * TILE - cam_y, tx, ty, tile, t);
        }
        draw_actors_in_row(g, ty, cam_x, cam_y, chop);
    }
}

static void draw_map_hud(const Game *g, const InputState *s, float alpha)
{
    const Stats *st = &g->stats;
    ui_alpha = alpha;
    draw_box((Rectangle){ 3, 3, 100, 29 });
    draw_text_shadow(TextFormat(text_get(TXT_LEVEL_SHORT), st->level), 7, 6, 10, GOLDEN);
    draw_text_shadow(TextFormat("%d/%d", st->hp, st->max_hp), 40, 6, 10, RAYWHITE);
    DrawCircle(88, 10, 2.5f, Fade(MOSS, alpha));  // herbs
    draw_text_shadow(TextFormat("%d", st->herbs), 93, 6, 10, RAYWHITE);
    draw_bar(7, 19, 92, st->hp, st->max_hp, HP_COLOR);
    draw_bar(7, 25, 92, st->sp, st->max_sp, SP_COLOR);
    ui_alpha = 1.0f;

    if (g->toast_timer > 0 && g->toast != TOAST_NONE) {
        const char *msg = "";
        switch (g->toast) {
        case TOAST_HERBS: msg = TextFormat(text_get(TXT_TOAST_HERBS), g->toast_amount); break;
        case TOAST_AXE: msg = TextFormat(text_get(TXT_TOAST_AXE), g->toast_amount); break;
        case TOAST_REST: msg = text_get(TXT_TOAST_REST); break;
        case TOAST_TOO_HARD: msg = text_get(TXT_TOAST_TOO_HARD); break;
        case TOAST_HOME: msg = text_get(TXT_TOAST_HOME); break;
        case TOAST_NOT_YET: msg = text_get(TXT_TOAST_NOT_YET); break;
        case TOAST_MAP: msg = text_get((TextId)(TXT_MAP_HOME + (g->toast_amount - 1) % 4)); break;
        default: break;
        }
        float a = fminf(1.0f, g->toast_timer * 2.0f);
        int w = MeasureText(msg, 10) + 12;
        DrawRectangle((VIEW_W - w) / 2, 150, w, 15, Fade(PANEL, a));
        draw_text_centered(msg, 153, 10, Fade(RAYWHITE, a));
    }

    if (s->touch_ui) {
        if (s->stick_id >= 0) {
            DrawCircleV(s->stick_origin, STICK_R, Fade(RAYWHITE, 0.2f));
            float dx = s->stick_pos.x - s->stick_origin.x, dy = s->stick_pos.y - s->stick_origin.y;
            float len = sqrtf(dx * dx + dy * dy);
            if (len > STICK_R) { dx *= STICK_R / len; dy *= STICK_R / len; }
            DrawCircleV((Vector2){ s->stick_origin.x + dx, s->stick_origin.y + dy }, 8, Fade(RAYWHITE, 0.5f));
        } else {
            DrawCircleV((Vector2){ 40, 140 }, STICK_R, Fade(RAYWHITE, 0.12f));
            DrawCircleV((Vector2){ 40, 140 }, 8, Fade(RAYWHITE, 0.25f));
        }
        DrawCircleV(BTN_ACTION, 22, Fade(RAYWHITE, s->touch_action ? 0.5f : 0.25f));
        // A little axe on the button.
        DrawLineEx((Vector2){ BTN_ACTION.x - 6, BTN_ACTION.y + 8 }, (Vector2){ BTN_ACTION.x + 4, BTN_ACTION.y - 6 }, 2, Fade(INK, 0.7f));
        DrawCircle((int)BTN_ACTION.x + 5, (int)BTN_ACTION.y - 6, 4, Fade(INK, 0.7f));
        DrawCircleV(BTN_PAUSE, 8, Fade(RAYWHITE, 0.25f));
        DrawRectangle((int)BTN_PAUSE.x - 3, (int)BTN_PAUSE.y - 4, 2, 8, Fade(INK, 0.6f));
        DrawRectangle((int)BTN_PAUSE.x + 1, (int)BTN_PAUSE.y - 4, 2, 8, Fade(INK, 0.6f));
    }
}

// --- Battle -----------------------------------------------------------------------------

static const char *msg_text(const Game *g, const BattleMsg *m)
{
    const Battle *b = &g->battle;
    const char *target = (m->target >= 0 && m->target < b->enemy_count) ? enemy_name(b, m->target) : "";
    const char *actor = (m->actor >= 0 && m->actor < b->enemy_count) ? enemy_name(b, m->actor) : "";
    const char *crit = m->crit ? text_get(TXT_MSG_CRIT) : "";
    switch (m->type) {
    case MSG_START:
        if (m->amount == ENCOUNTER_BOSS) return text_get(TXT_MSG_BOSS);
        if (m->amount == ENCOUNTER_EMBERS) return text_get(TXT_MSG_EMBERS);
        return text_get(TXT_MSG_START);
    case MSG_ATTACK: return TextFormat("%s%s", crit, TextFormat(text_get(TXT_MSG_ATTACK), target, m->amount));
    case MSG_SPLIT: return TextFormat("%s%s", crit, TextFormat(text_get(TXT_MSG_SPLIT), target, m->amount));
    case MSG_SPIN: return text_get(TXT_MSG_SPIN);
    case MSG_SPIN_HIT: return TextFormat("%s%s", crit, TextFormat(text_get(TXT_MSG_SPIN_HIT), target, m->amount));
    case MSG_HERB: return TextFormat(text_get(TXT_MSG_HERB), m->amount);
    case MSG_DEFEND: return text_get(TXT_MSG_DEFEND);
    case MSG_FLEE_OK: return text_get(TXT_MSG_FLEE_OK);
    case MSG_FLEE_FAIL: return text_get(TXT_MSG_FLEE_FAIL);
    case MSG_CANT_FLEE: return text_get(TXT_MSG_CANT_FLEE);
    case MSG_BEAM: return TextFormat(text_get(TXT_MSG_BEAM), target, m->amount);
    case MSG_LIGHT: return TextFormat(text_get(TXT_MSG_LIGHT), m->amount);
    case MSG_ALLY_DEFEND: return text_get(TXT_MSG_ALLY_DEFEND);
    case MSG_ALLY_DOWN: return text_get(TXT_MSG_ALLY_DOWN);
    case MSG_ENEMY_ATTACK:
        return TextFormat(text_get(m->on_ally ? TXT_MSG_ENEMY_ATTACK_ALLY : TXT_MSG_ENEMY_ATTACK), actor, m->amount);
    case MSG_ENEMY_SPECIAL:
        if (b->enemies[m->actor].species == SPECIES_ELDER_STUMP)
            return TextFormat(text_get(m->on_ally ? TXT_MSG_ROOTS_ALLY : TXT_MSG_ROOTS), m->amount);
        return TextFormat(text_get(m->on_ally ? TXT_MSG_BITE_ALLY : TXT_MSG_BITE), actor, m->amount);
    case MSG_ENEMY_DOWN:
        if (b->enemies[m->target].species == SPECIES_EMBER) return TextFormat(text_get(TXT_MSG_EMBER_FLEES), target);
        return TextFormat(text_get(TXT_MSG_ENEMY_DOWN), target);
    case MSG_VICTORY: return TextFormat(text_get(TXT_MSG_VICTORY), m->amount);
    case MSG_FOUND_HERB: return TextFormat(text_get(TXT_MSG_FOUND_HERB), m->amount);
    case MSG_LEVEL_UP: return TextFormat(text_get(TXT_MSG_LEVEL_UP), m->amount);
    case MSG_PLAYER_DOWN: return text_get(TXT_MSG_PLAYER_DOWN);
    }
    return "";
}

static void draw_battle_background(const Game *g)
{
    bool boss = g->battle.enemies[0].species == SPECIES_ELDER_STUMP;
    Color top = boss ? (Color){ 40, 36, 60, 255 } : (Color){ 110, 160, 200, 255 };
    Color bottom = boss ? (Color){ 70, 90, 70, 255 } : (Color){ 170, 210, 190, 255 };
    DrawRectangleGradientV(0, 0, VIEW_W, PANEL_Y, top, bottom);
    if (art.grass_count > 0 && art.oak.count > 0) {
        // The ground and two rows of the area's trees, at twice the size;
        // the far row fades into the sky.
        for (int i = 0; i * 64 < VIEW_W; i++)
            DrawTexturePro(art.grass[tile_hash(i, 7) % (unsigned)art.grass_count], (Rectangle){ 0, 0, 32 * ART, 20 * ART },
                           (Rectangle){ i * 64.0f, 78, 64, 40 }, (Vector2){ 0, 0 }, 0, WHITE);
        for (int row = 0; row < 2; row++) {
            Color tint = row == 0 ? (Color){ 150, 176, 196, 255 } : (Color){ 214, 226, 220, 255 };
            if (boss) tint = row == 0 ? (Color){ 70, 70, 100, 255 } : (Color){ 120, 130, 140, 255 };
            for (int i = -1; i < 16; i++) {
                const Texture2D *t = tree_texture(g, i * 2 + row * 37, row * 9);
                if (!t) continue;
                float x = i * 22.0f + row * 11 + (float)(tile_hash(i, row) % 5);
                DrawTexturePro(*t, (Rectangle){ 0, 0, 16 * ART, 24 * ART }, (Rectangle){ x, 36.0f + row * 6, 32, 48 }, (Vector2){ 0, 0 }, 0, tint);
            }
        }
        return;
    }
    for (int i = 0; i < 12; i++) {
        float x = i * 30.0f + (i % 3) * 7;
        DrawRectangle((int)x + 6, 40, 5, 40, Fade(BARK_DARK, 0.6f));
        DrawCircle((int)x + 8, 38, 16 + (i % 2) * 4, Fade(LEAVES, boss ? 0.6f : 0.8f));
    }
    DrawRectangle(0, 78, VIEW_W, PANEL_Y - 78, GRASS_B);
    DrawRectangle(0, 78, VIEW_W, 2, GRASS_TUFT);
    for (int i = 0; i < 24; i++) DrawRectangle(i * 14 + (i % 3) * 3, 86 + (i * 7) % 26, 1, 2, GRASS_TUFT);
}

static void draw_damage_popup(const BattleMsg *m, float timer, Vector2 at, Color c)
{
    float rise = fminf(timer, 0.6f) * 20.0f;
    const char *txt = TextFormat("%d", m->amount);
    int size = m->crit ? 20 : 10;
    draw_text_shadow(txt, (int)(at.x - MeasureText(txt, size) / 2.0f), (int)(at.y - rise), size, c);
}

static void draw_battle(const Game *g, const InputState *s)
{
    const Battle *b = &g->battle;
    const Stats *st = &g->stats;
    const BattleMsg *m = battle_current_msg(g);
    draw_battle_background(g);

    // Monsters.
    for (int i = 0; i < b->enemy_count; i++) {
        const Combatant *e = &b->enemies[i];
        if (!e->shown_alive) continue;
        Rectangle r = enemy_rect(b, i);
        float cx = r.x + r.width / 2, cy = r.y + r.height / 2;
        bool flash = e->flash > 0 && fmodf(e->flash, 0.1f) < 0.05f;
        float lunge = (m && (m->type == MSG_ENEMY_ATTACK || m->type == MSG_ENEMY_SPECIAL) && m->actor == i)
                          ? -sinf(fminf(b->msg_timer * 8.0f, PI)) * 12.0f : 0.0f;
        float scale = e->species == SPECIES_ELDER_STUMP ? 3.4f : 2.0f;
        draw_monster(e->species, cx + lunge, cy, scale, g->time + i, flash, -1);
        // Name and health bar.
        const char *name = enemy_name(b, i);
        int bw = (int)fmaxf(30, r.width - 6);
        int by = (int)(r.y + r.height + (e->species == SPECIES_ELDER_STUMP ? -6 : 0));
        draw_bar((int)(cx - bw / 2), by, bw, e->shown_hp, species_info(e->species)->max_hp, HP_COLOR);
        DrawText(name, (int)(cx - MeasureText(name, 10) / 2.0f) + 1, by - 10, 10, INK);
        DrawText(name, (int)(cx - MeasureText(name, 10) / 2.0f), by - 11, 10, RAYWHITE);
        if (b->phase == PHASE_TARGET && b->target == i && fmodf(g->time, 0.5f) < 0.35f) {
            DrawTriangle((Vector2){ cx - 5, r.y - 8 }, (Vector2){ cx, r.y - 2 }, (Vector2){ cx + 5, r.y - 8 }, GOLDEN);
        }
        if (m && m->target == i && m->amount > 0 &&
            (m->type == MSG_ATTACK || m->type == MSG_SPLIT || m->type == MSG_SPIN_HIT))
            draw_damage_popup(m, b->msg_timer, (Vector2){ cx, r.y + 4 }, m->crit ? GOLDEN : RAYWHITE);
        if (m && m->target == i && m->type == MSG_BEAM) {
            // A beam of moonlight falls on the monster.
            float k = fminf(1.0f, b->msg_timer * 3.0f);
            float bw = 10 * (1.0f - fabsf(k * 2 - 1)) + 2;
            DrawRectangle((int)(cx - bw / 2), 0, (int)bw, (int)(cy + 10), Fade(MOON_SILVER, 0.75f));
            DrawRectangle((int)(cx - bw / 4), 0, (int)fmaxf(1, bw / 2), (int)(cy + 10), Fade(RAYWHITE, 0.9f));
            draw_damage_popup(m, b->msg_timer, (Vector2){ cx, r.y + 4 }, MOON_SILVER);
        }
    }

    // The lumberjack, swinging when he acts.
    float swing = 0, step = 0;
    if (m && (m->type == MSG_ATTACK || m->type == MSG_SPLIT || m->type == MSG_SPIN || m->type == MSG_SPIN_HIT)) {
        swing = fminf(1.0f, b->msg_timer * 4.0f);
        step = sinf(fminf(b->msg_timer * 6.0f, PI)) * 18.0f;
    }
    bool hurt = b->player_flash > 0 && fmodf(b->player_flash, 0.1f) < 0.05f;
    bool down = m && m->type == MSG_PLAYER_DOWN;
    // The goddess, behind him: watching while the fire beings attack her,
    // fighting at his side once she has joined.
    if (b->ally_in) {
        bool her_hurt = b->ally_flash > 0 && fmodf(b->ally_flash, 0.1f) < 0.05f;
        bool casting = m && (m->type == MSG_BEAM || m->type == MSG_LIGHT);
        if (b->ally_shown_hp <= 0) {
            DrawRectangle(14, 96, 26, 6, MOON_WHITE);  // she fell
            DrawRectangle(10, 94, 8, 8, MOON_SHADE);
        } else if (!her_hurt) {
            draw_goddess(26 + (casting ? sinf(fminf(b->msg_timer * 6.0f, PI)) * 8.0f : 0.0f), 86, 2.0f, g->time * (casting ? 4.0f : 1.0f), 1, 0, 0);
        }
        if (b->ally_defending) DrawCircleLines(26, 78, 18, Fade(MOON_SILVER, 0.8f));
        if (m && m->type == MSG_LIGHT) {
            for (int k = 0; k < 10; k++) {  // sparkles over both of them
                float a = g->time * 3.0f + k * 0.63f;
                float rise = fmodf(b->msg_timer * 30.0f + k * 7, 40.0f);
                DrawRectangle((int)(48 + cosf(a) * 34), (int)(96 - rise), 2, 2, Fade(MOON_SILVER, 1.0f - rise / 40.0f));
            }
            draw_damage_popup(m, b->msg_timer, (Vector2){ 26, 50 }, SLIME_C);
            draw_damage_popup(m, b->msg_timer, (Vector2){ 70, 50 }, SLIME_C);
        }
        if (m && m->on_ally && (m->type == MSG_ENEMY_ATTACK || m->type == MSG_ENEMY_SPECIAL))
            draw_damage_popup(m, b->msg_timer, (Vector2){ 26, 50 }, HP_COLOR);
    } else if (g->foes[b->foe].kind == ENCOUNTER_EMBERS) {
        draw_goddess_fallen(28, 96, 2.0f, g->time);
    }
    if (!hurt) {
        if (down) {
            DrawRectangle(52, 92, 26, 6, SHIRT);  // lying on the grass
            DrawRectangle(76, 90, 8, 8, SKIN);
        } else {
            draw_lumberjack(70 + step, 82, 2.0f, 1, 0, 0, swing);
        }
    }
    if (b->defending) DrawCircleLines(70, 78, 18, Fade(SP_COLOR, 0.8f));
    if (m && !m->on_ally && (m->type == MSG_ENEMY_ATTACK || m->type == MSG_ENEMY_SPECIAL))
        draw_damage_popup(m, b->msg_timer, (Vector2){ 70, 50 }, HP_COLOR);
    if (m && m->type == MSG_HERB) draw_damage_popup(m, b->msg_timer, (Vector2){ 70, 50 }, SLIME_C);

    // Message line at the top.
    const char *line = NULL;
    if (m) line = msg_text(g, m);
    else if (b->phase == PHASE_TARGET) line = text_get(TXT_CHOOSE_TARGET);
    else if (b->phase == PHASE_ALLY) line = text_get(TXT_ALLY_TURN);
    if (line) {
        draw_box((Rectangle){ 4, 4, VIEW_W - 8, 16 });
        draw_text_centered(line, 7, 10, RAYWHITE);
    }

    // Bottom panel: menu on the left, status on the right.
    draw_box((Rectangle){ 3, PANEL_Y, 154, VIEW_H - PANEL_Y - 3 });
    draw_box((Rectangle){ 161, PANEL_Y, VIEW_W - 164, VIEW_H - PANEL_Y - 3 });
    bool menu_active = b->phase == PHASE_MENU || b->phase == PHASE_SKILLS || b->phase == PHASE_ALLY;
    if (b->phase == PHASE_ALLY || (b->phase == PHASE_TARGET && b->choosing == 1)) {
        for (int i = 0; i < SPELL_MENU_COUNT; i++) {
            const char *label = i == SPELL_BEAM ? TextFormat(text_get(TXT_SPELL_BEAM), SPELL_BEAM_COST)
                              : i == SPELL_LIGHT ? TextFormat(text_get(TXT_SPELL_LIGHT), SPELL_LIGHT_COST)
                              : i == SPELL_DEFEND ? text_get(TXT_MENU_DEFEND)
                                                  : text_get(TXT_BACK);
            Rectangle r = menu_entry_rect(i);
            bool on = b->ally_cursor == i;
            Color c = b->phase != PHASE_ALLY ? Fade(RAYWHITE, 0.5f)
                    : battle_spell_enabled(g, (SpellItem)i) ? RAYWHITE : DISABLED;
            if (on) DrawRectangleRec(r, Fade(PANEL_EDGE, 0.2f));
            DrawText(label, (int)r.x + 10, (int)r.y + 1, 10, c);
            if (i == SPELL_BEAM || i == SPELL_LIGHT) DrawCircle((int)r.x + MENU_W - 12, (int)r.y + 5, 2, MOON_SILVER);
            if (on && !s->touch_ui) DrawText(">", (int)r.x + 2, (int)r.y + 1, 10, GOLDEN);
        }
    } else if (b->phase == PHASE_SKILLS) {
        for (int i = 0; i < SKILL_MENU_COUNT; i++) {
            const char *label = i == SKILL_SPLIT ? TextFormat(text_get(TXT_SKILL_SPLIT), SKILL_SPLIT_COST)
                              : i == SKILL_SPIN ? TextFormat(text_get(TXT_SKILL_SPIN), SKILL_SPIN_COST)
                                                : text_get(TXT_BACK);
            Rectangle r = menu_entry_rect(i);
            bool on = b->skill_cursor == i;
            Color c = battle_skill_enabled(g, (SkillItem)i) ? RAYWHITE : DISABLED;
            if (on) DrawRectangleRec(r, Fade(PANEL_EDGE, 0.2f));
            DrawText(label, (int)r.x + 10, (int)r.y + 1, 10, c);
            if (i != SKILL_BACK) DrawCircle((int)r.x + MENU_W - 12, (int)r.y + 5, 2, SP_COLOR);
            if (on && !s->touch_ui) DrawText(">", (int)r.x + 2, (int)r.y + 1, 10, GOLDEN);
        }
    } else {
        for (int i = 0; i < MENU_COUNT; i++) {
            static const TextId labels[MENU_COUNT] = { TXT_MENU_ATTACK, TXT_MENU_SKILLS, TXT_MENU_HERB, TXT_MENU_DEFEND, TXT_MENU_FLEE };
            const char *label = i == MENU_HERB ? TextFormat(text_get(labels[i]), st->herbs) : text_get(labels[i]);
            Rectangle r = menu_entry_rect(i);
            bool on = b->cursor == i && menu_active;
            Color c = !menu_active ? Fade(RAYWHITE, 0.5f) : battle_menu_enabled(g, (MenuItem)i) ? RAYWHITE : DISABLED;
            if (on) DrawRectangleRec(r, Fade(PANEL_EDGE, 0.2f));
            DrawText(label, (int)r.x + 10, (int)r.y + 1, 10, c);
            if (on && !s->touch_ui) DrawText(">", (int)r.x + 2, (int)r.y + 1, 10, GOLDEN);
        }
    }

    int sx = 167, sy = PANEL_Y + 6;
    if (b->ally_in) {
        // Two of them: a name line with numbers, then thin bars.
        const Ally *a = &g->goddess;
        struct { const char *name, *nums; int hp, max_hp, sp, max_sp; Color sp_color; bool active; } rows[2] = {
            { text_get(TXT_HERO), TextFormat("%s %d  %s %d", text_get(TXT_HP), b->shown_hp, text_get(TXT_SP), b->shown_sp),
              b->shown_hp, st->max_hp, b->shown_sp, st->max_sp, SP_COLOR, b->phase != PHASE_MESSAGES && b->choosing == 0 },
            { text_get(TXT_GODDESS_SHORT), TextFormat("%s %d  %s %d", text_get(TXT_HP), b->ally_shown_hp, text_get(TXT_MP), b->ally_shown_mp),
              b->ally_shown_hp, a->max_hp, b->ally_shown_mp, a->max_mp, MOON_SILVER, b->phase != PHASE_MESSAGES && b->choosing == 1 },
        };
        for (int i = 0; i < 2; i++) {
            int y = PANEL_Y + 4 + i * 26;
            Color name_c = rows[i].active ? GOLDEN : Fade(GOLDEN, 0.6f);
            draw_text_shadow(rows[i].name, sx, y, 10, name_c);
            draw_text_shadow(rows[i].nums, VIEW_W - 9 - MeasureText(rows[i].nums, 10), y, 10, RAYWHITE);
            draw_bar(sx, y + 12, 140, rows[i].hp, rows[i].max_hp, HP_COLOR);
            DrawRectangle(sx, y + 17, 140, 2, (Color){ 0, 0, 0, 140 });
            DrawRectangle(sx, y + 17, rows[i].max_sp > 0 ? 140 * rows[i].sp / rows[i].max_sp : 0, 2, rows[i].sp_color);
        }
        int xp_need2 = xp_to_next_level(st->level);
        DrawRectangle(sx, VIEW_H - 8, 140, 1, (Color){ 0, 0, 0, 140 });
        DrawRectangle(sx, VIEW_H - 8, 140 * st->xp / (xp_need2 > 0 ? xp_need2 : 1), 1, GOLDEN);
        return;
    }
    draw_text_shadow(TextFormat("%s  %s", text_get(TXT_HERO), TextFormat(text_get(TXT_LEVEL_SHORT), st->level)), sx, sy, 10, GOLDEN);
    draw_text_shadow(text_get(TXT_HP), sx, sy + 15, 10, RAYWHITE);
    draw_bar(sx + 20, sy + 18, 80, b->shown_hp, st->max_hp, HP_COLOR);
    draw_text_shadow(TextFormat("%d", b->shown_hp), sx + 106, sy + 15, 10, RAYWHITE);
    draw_text_shadow(text_get(TXT_SP), sx, sy + 29, 10, RAYWHITE);
    draw_bar(sx + 20, sy + 32, 80, b->shown_sp, st->max_sp, SP_COLOR);
    draw_text_shadow(TextFormat("%d", b->shown_sp), sx + 106, sy + 29, 10, RAYWHITE);
    int xp_need = xp_to_next_level(st->level);
    DrawRectangle(sx, sy + 45, 126, 2, (Color){ 0, 0, 0, 140 });
    DrawRectangle(sx, sy + 45, 126 * st->xp / (xp_need > 0 ? xp_need : 1), 2, GOLDEN);
}

// --- Story scenes ---------------------------------------------------------------------

#define SCENE_TEXT_W (VIEW_W - 24)

// Splits `text` into lines that fit `width` pixels. Returns how many lines
// it needs; fills at most `max_lines` of them.
static int wrap_text(const char *text, int width, char lines[][160], int max_lines)
{
    int count = 0;
    char line[160] = "";
    const char *p = text;
    while (*p) {
        while (*p == ' ') p++;
        const char *end = p;
        while (*end && *end != ' ') end++;
        if (end == p) break;
        char word[160];
        int wl = (int)(end - p) < 159 ? (int)(end - p) : 159;
        memcpy(word, p, wl);
        word[wl] = 0;
        char attempt[320];
        snprintf(attempt, sizeof attempt, "%s%s%s", line, line[0] ? " " : "", word);
        if (line[0] && MeasureText(attempt, 10) > width) {
            if (count < max_lines) memcpy(lines[count], line, sizeof line);
            count++;
            memcpy(line, word, sizeof word);
        } else {
            size_t n = strlen(attempt);
            if (n >= sizeof line) n = sizeof line - 1;
            memcpy(line, attempt, n);
            line[n] = 0;
        }
        p = end;
    }
    if (line[0]) {
        if (count < max_lines) memcpy(lines[count], line, sizeof line);
        count++;
    }
    return count;
}

static const char *page_text(const StoryPage *p) { return text_is_portuguese() ? p->pt : p->en; }

// Who speaks is shown on the left, above the box.
static void draw_portrait(Speaker who, float t)
{
    switch (who) {
    case SPEAKER_HERO: draw_lumberjack(40, 94, 3.0f, 1, 0, 0, 0); break;
    case SPEAKER_WOMAN: draw_goddess(40, 92, 3.0f, t, 0, 1, 0); break;
    case SPEAKER_CREATURE_1:
    case SPEAKER_CREATURE_2:
    case SPEAKER_CREATURE_3: draw_monster(SPECIES_EMBER, 40, 96, 3.0f, t + who, false, 1); break;
    default: break;
    }
}

static void draw_scene(const Game *g, bool touch)
{
    const StoryPage *p = story_page(g->scene, g->scene_page);
    if (!p) return;
    draw_portrait(p->who, g->time);

    Rectangle box = { 4, 120, VIEW_W - 8, 56 };
    draw_box(box);
    int y = 124, max_lines = 3;
    if (p->who != SPEAKER_NONE) {
        draw_text_shadow(story_speaker_name(p->who, text_is_portuguese()), 10, y, 10, GOLDEN);
        y += 13;
        max_lines = 2;
    } else {
        y += 2;
    }
    char lines[3][160];
    int n = wrap_text(page_text(p), SCENE_TEXT_W, lines, max_lines);
    for (int i = 0; i < n && i < max_lines; i++, y += 12)
        draw_text_shadow(lines[i], 10, y, 10, p->who == SPEAKER_NONE ? Fade(RAYWHITE, 0.85f) : RAYWHITE);
    const char *hint = text_get(touch ? TXT_SCENE_TAP : TXT_SCENE_KEYS);
    DrawText(hint, (int)(box.x + box.width) - MeasureText(hint, 10) - 6, 164, 10, Fade(RAYWHITE, 0.55f));
    if (fmodf(g->time, 0.8f) < 0.5f) {
        float ax = box.x + box.width - 10, ay = 125;
        DrawTriangle((Vector2){ ax, ay }, (Vector2){ ax + 3, ay + 4 }, (Vector2){ ax + 6, ay }, GOLDEN);
    }
}

// Warns when a page needs more lines than the box has.
static void check_story_fits(void)
{
    char lines[8][160];
    for (int sc = 0; sc < SCENE_COUNT; sc++)
        for (int i = 0; i < story_page_count((Scene)sc); i++) {
            const StoryPage *p = story_page((Scene)sc, i);
            int room = p->who == SPEAKER_NONE ? 3 : 2;
            if (wrap_text(p->en, SCENE_TEXT_W, lines, 8) > room || wrap_text(p->pt, SCENE_TEXT_W, lines, 8) > room)
                TraceLog(LOG_WARNING, "scene %d page %d does not fit the box", sc, i);
        }
}

// --- Screens ----------------------------------------------------------------------------

// The saves on disk, newest first, for the load page.
static SaveInfo *saves = NULL;
static int save_total = 0;

static void refresh_saves(void)
{
    free(saves);
    saves = save_list(&save_total);
}

// The load page: each save shows its number, level, area and when it was
// saved.
static void draw_load_page(const Game *g, bool touch)
{
    int n = save_total;
    if (g->title_cursor < n) {  // keep the chosen save in view
        if (g->title_cursor < load_first) load_first = g->title_cursor;
        if (g->title_cursor >= load_first + LOAD_ROWS) load_first = g->title_cursor - LOAD_ROWS + 1;
    }
    if (load_first > n - LOAD_ROWS) load_first = n - LOAD_ROWS;
    if (load_first < 0) load_first = 0;

    draw_text_centered(text_get(TXT_LOAD_GAME), LOAD_Y - 12, 10, Fade(RAYWHITE, 0.8f));
    draw_box((Rectangle){ 40, LOAD_Y, 240, LOAD_ROWS * LOAD_ROW + 26 });
    for (int i = 0; i < LOAD_ROWS && load_first + i < n; i++) {
        const SaveInfo *sv = &saves[load_first + i];
        Rectangle r = load_row_rect(i);
        bool on = g->title_cursor == load_first + i;
        if (on) DrawRectangleRec(r, Fade(PANEL_EDGE, 0.2f));
        if (i > 0) DrawRectangle((int)r.x + 4, (int)r.y, (int)r.width - 8, 1, Fade(PANEL_EDGE, 0.25f));
        int x = (int)r.x + 12, right = (int)(r.x + r.width) - 6;
        draw_text_shadow(TextFormat(text_get(TXT_SAVE_NAME), sv->slot), x, (int)r.y + 2, 10, on ? GOLDEN : RAYWHITE);
        const char *lv = TextFormat(text_get(TXT_LEVEL_LONG), sv->level);
        draw_text_shadow(lv, right - MeasureText(lv, 10), (int)r.y + 2, 10, on ? GOLDEN : RAYWHITE);
        if (sv->area >= 0 && sv->area <= 3)
            draw_text_shadow(text_get((TextId)(TXT_MAP_HOME + sv->area)), x, (int)r.y + 12, 10, Fade(RAYWHITE, 0.6f));
        time_t when = (time_t)sv->saved_at;
        struct tm *tm = localtime(&when);
        char date[32] = "";
        if (tm) strftime(date, sizeof date, text_is_portuguese() ? "%d/%m/%Y %H:%M" : "%Y-%m-%d %H:%M", tm);
        draw_text_shadow(date, right - MeasureText(date, 10), (int)r.y + 12, 10, Fade(RAYWHITE, 0.6f));
        if (on && !touch) DrawTriangle((Vector2){ r.x + 3, r.y + 3 }, (Vector2){ r.x + 3, r.y + 11 }, (Vector2){ r.x + 8, r.y + 7 }, GOLDEN);
    }
    // Little arrows when there are more saves above or below.
    float ax = 288;
    if (load_first > 0)
        DrawTriangle((Vector2){ ax, LOAD_Y + 6 }, (Vector2){ ax - 4, LOAD_Y + 11 }, (Vector2){ ax + 4, LOAD_Y + 11 }, GOLDEN);
    if (load_first + LOAD_ROWS < n) {
        float by = LOAD_Y + 4 + LOAD_ROWS * LOAD_ROW;
        DrawTriangle((Vector2){ ax - 4, by - 4 }, (Vector2){ ax, by + 1 }, (Vector2){ ax + 4, by - 4 }, GOLDEN);
    }
    Rectangle r = load_back_rect();
    bool on = g->title_cursor == n;
    if (on) DrawRectangleRec(r, Fade(PANEL_EDGE, 0.2f));
    const char *back = text_get(TXT_BACK);
    int bx = (int)(r.x + (r.width - MeasureText(back, 10)) / 2);
    draw_text_shadow(back, bx, (int)r.y + 2, 10, on ? GOLDEN : RAYWHITE);
    if (on && !touch) DrawTriangle((Vector2){ (float)bx - 9, r.y + 2 }, (Vector2){ (float)bx - 9, r.y + 10 }, (Vector2){ (float)bx - 4, r.y + 6 }, GOLDEN);
}

// The title screen: the name, the lumberjack and the goddess on either side,
// and the menu between them.
static void draw_title(const Game *g, bool touch)
{
    DrawRectangle(0, 0, VIEW_W, VIEW_H, Fade(INK, 0.5f));
    DrawRectangleGradientV(0, 0, VIEW_W, 70, Fade(INK, 0.6f), Fade(INK, 0.0f));
    DrawRectangleGradientV(0, 140, VIEW_W, 40, Fade(INK, 0.0f), Fade(INK, 0.7f));
    draw_text_centered("TIMBERHEART", 14, 30, GOLDEN);
    DrawRectangle(96, 46, 128, 1, Fade(GOLDEN, 0.6f));
    if (g->title_page == PAGE_LOAD) {
        draw_load_page(g, touch);
        return;
    }
    float swing = fmodf(g->time, 3.0f) < 0.4f ? fmodf(g->time, 3.0f) / 0.4f : 0;
    draw_lumberjack(58, 112, 2.0f, 1, 0, 0, swing);
    draw_goddess(262, 112 + sinf(g->time * 2.0f), 2.0f, g->time, -1, 0, 0);

    bool options = g->title_page == PAGE_OPTIONS;
    int count = options ? OPTION_COUNT : TITLE_COUNT;
    if (options) draw_text_centered(text_get(TXT_OPTIONS), TITLE_MENU_Y - 14, 10, Fade(RAYWHITE, 0.8f));
    draw_box((Rectangle){ 96, TITLE_MENU_Y, 128, (float)(count * TITLE_LINE + 8) });
    for (int i = 0; i < count; i++) {
        const char *label;
        bool enabled = true;
        if (!options) {
            static const TextId labels[TITLE_COUNT] = { TXT_NEW_GAME, TXT_LOAD_GAME, TXT_OPTIONS, TXT_QUIT };
            label = text_get(labels[i]);
            enabled = i != TITLE_LOAD || g->save_count > 0;
        } else if (i == OPTION_LANGUAGE) {
            label = TextFormat("%s: %s", text_get(TXT_LANGUAGE_LABEL), text_get(TXT_LANGUAGE));
        } else if (i == OPTION_SOUND) {
            label = TextFormat("%s: %s", text_get(TXT_SOUND_LABEL), text_get(sound_on ? TXT_ON : TXT_OFF));
        } else {
            label = text_get(TXT_BACK);
        }
        Rectangle r = title_entry_rect(i);
        bool on = g->title_cursor == i;
        int w = MeasureText(label, 10), x = (int)(r.x + (r.width - w) / 2), y = (int)r.y + 3;
        if (on) DrawRectangleRec((Rectangle){ r.x - 2, r.y, r.width + 4, r.height }, Fade(PANEL_EDGE, 0.2f));
        draw_text_shadow(label, x, y, 10, !enabled ? DISABLED : on ? GOLDEN : RAYWHITE);
        if (on && !touch) {  // a little arrow that bobs
            int bob = (int)(fmodf(g->time * 2.0f, 1.0f) < 0.5f);
            DrawTriangle((Vector2){ (float)(x - 9 + bob), (float)y }, (Vector2){ (float)(x - 9 + bob), (float)(y + 8) },
                         (Vector2){ (float)(x - 4 + bob), (float)(y + 4) }, GOLDEN);
        }
    }
    if (!touch) draw_text_centered(text_get(TXT_CONTROLS), 164, 10, Fade(RAYWHITE, 0.7f));
}

static void draw_overlay(const Game *g, bool touch)
{
    TextId press = touch ? TXT_TAP_TO_CONTINUE : TXT_PRESS_TO_CONTINUE;
    switch (g->state) {
    case STATE_TITLE:
        draw_title(g, touch);
        break;
    case STATE_PAUSED:
        DrawRectangle(0, 0, VIEW_W, VIEW_H, Fade(INK, 0.5f));
        draw_text_centered(text_get(TXT_PAUSED), 70, 20, RAYWHITE);
        draw_text_centered(text_get(press), 100, 10, RAYWHITE);
        break;
    case STATE_GAME_OVER:
        DrawRectangle(0, 0, VIEW_W, VIEW_H, Fade(INK, 0.85f));
        draw_text_centered(text_get(TXT_GAME_OVER), 60, 20, RAYWHITE);
        draw_text_centered(text_get(TXT_WAKE_UP), 90, 10, Fade(RAYWHITE, 0.8f));
        draw_text_centered(text_get(press), 120, 10, GOLDEN);
        break;
    case STATE_SCENE:
        draw_scene(g, touch);
        break;
    case STATE_WON:
        DrawRectangle(0, 0, VIEW_W, VIEW_H, Fade(INK, 0.6f));
        draw_text_centered(text_get(TXT_WON), 44, 20, GOLDEN);
        draw_text_centered(text_get(TXT_WON_2), 72, 10, RAYWHITE);
        draw_lumberjack(160, 108, 2.0f, 0, 1, 0, 0);
        draw_text_centered(text_get(press), 140, 10, RAYWHITE);
        break;
    default:
        break;
    }
}

// In the scenes at the clearing, the camera frames the lumberjack and the
// goddess above the dialog box, so the box does not hide them.
static void scene_camera(const Game *g, int *cam_x, int *cam_y)
{
    if (g->state != STATE_SCENE || (g->scene != SCENE_CREATURES && g->scene != SCENE_GODDESS && g->scene != SCENE_ELDER)) return;
    float gx = -1, gy = -1;
    if (g->scene == SCENE_ELDER) {  // frame the Elder Stump instead
        const Foe *f = &g->foes[g->scene_foe];
        gx = f->x + WALKER_SIZE / 2.0f;
        gy = f->y + WALKER_SIZE / 2.0f - 8;
    }
    for (int y = 0; y < g->h && gx < 0; y++)
        for (int x = 0; x < g->w && gx < 0; x++)
            if (game_tile_at(g, x, y) == TILE_GODDESS) { gx = x * TILE + TILE / 2.0f; gy = y * TILE + TILE / 2.0f; }
    if (gx < 0) {  // she has joined: she is where she stood
        gx = g->follower.x + WALKER_SIZE / 2.0f;
        gy = g->follower.y + WALKER_SIZE / 2.0f;
    }
    float hx = g->walker.x + WALKER_SIZE / 2.0f, hy = g->walker.y + WALKER_SIZE / 2.0f;
    float cx = (gx + hx) / 2.0f - VIEW_W / 2.0f, cy = (gy + hy) / 2.0f - 58.0f;
    if (g->scene == SCENE_ELDER) {  // he is big: keep all of him above the box
        cx = gx - VIEW_W / 2.0f;
        cy = gy - 72.0f;
    }
    float max_x = (float)(g->w * TILE - VIEW_W), max_y = (float)(g->h * TILE - VIEW_H);
    *cam_x = (int)fmaxf(0.0f, fminf(cx, max_x));
    *cam_y = (int)fmaxf(0.0f, fminf(cy, max_y));
}

static void draw_game(const Game *g, const InputState *s, float chop)
{
    ClearBackground(CLIFF_DARK);
    if (g->state == STATE_BATTLE) {
        draw_battle(g, s);
        return;
    }
    int cam_x = (int)floorf(g->camera_x), cam_y = (int)floorf(g->camera_y);
    scene_camera(g, &cam_x, &cam_y);
    draw_map(g, cam_x, cam_y, chop);
    if (g->state == STATE_MAP || g->state == STATE_PAUSED) {
        // The panel turns see-through while he or she walks under it.
        const Walker *w = &g->walker, *f = &g->follower;
        Rectangle panel = { 0, 0, 108, 38 };
        bool under = CheckCollisionRecs(panel, (Rectangle){ w->x - cam_x - 4, w->y - cam_y - 14, WALKER_SIZE + 8, WALKER_SIZE + 16 }) ||
                     (g->goddess.joined &&
                      CheckCollisionRecs(panel, (Rectangle){ f->x - cam_x - 4, f->y - cam_y - 14, WALKER_SIZE + 8, WALKER_SIZE + 16 }));
        draw_map_hud(g, s, under ? 0.3f : 1.0f);
    }
    draw_overlay(g, s->touch_ui);
}

// --- Main loop ----------------------------------------------------------------------------

// Where the saves go: the app's own storage on Android; elsewhere the
// user's data folder.
#if defined(PLATFORM_ANDROID)
#include <android_native_app_glue.h>
struct android_app *GetAndroidApp(void);  // from raylib
#endif
static const char *saves_folder(void)
{
    static char path[512];
#if defined(PLATFORM_ANDROID)
    struct android_app *app = GetAndroidApp();
    snprintf(path, sizeof path, "%s", app && app->activity && app->activity->internalDataPath ? app->activity->internalDataPath : ".");
#elif defined(_WIN32)
    const char *base = getenv("APPDATA");
    snprintf(path, sizeof path, "%s/Timberheart", base ? base : ".");
#elif defined(__APPLE__)
    const char *home = getenv("HOME");
    snprintf(path, sizeof path, "%s/Library/Application Support/Timberheart", home ? home : ".");
#else
    const char *data = getenv("XDG_DATA_HOME"), *home = getenv("HOME");
    if (data && *data) snprintf(path, sizeof path, "%s/timberheart", data);
    else snprintf(path, sizeof path, "%s/.local/share/timberheart", home ? home : ".");
#endif
    return path;
}

int main(int argc, char *argv[])
{
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
#if defined(PLATFORM_ANDROID)
    InitWindow(0, 0, "Timberheart");
#else
    InitWindow(VIEW_W * 3, VIEW_H * 3, "Timberheart");
    SetWindowMinSize(VIEW_W, VIEW_H);
#endif
    SetExitKey(KEY_NULL);  // Esc pauses instead of closing
    SetTargetFPS(60);
    text_init();

    // The picture is laid out at 320x180 and drawn at ART times that.
    RenderTexture2D canvas = LoadRenderTexture(VIEW_W * ART, VIEW_H * ART);
    Camera2D layout = { .zoom = (float)ART };
    SetTextureFilter(canvas.texture, TEXTURE_FILTER_POINT);
    Sounds sounds = load_sounds();
    load_music();
    load_art();

    check_story_fits();
    save_set_dir(saves_folder());
    {
        bool pt = text_is_portuguese();
        if (settings_read(&pt, &sound_on)) text_set_portuguese(pt);
    }
    refresh_saves();

    static Game game;
    game_init(&game);
    InputState input = { 0 };
    input.stick_id = -1;
#if defined(PLATFORM_ANDROID)
    input.touch_ui = true;
#endif
    // "--touch" shows the on-screen controls on PC, to try the phone layout.
    for (int i = 1; i < argc; i++)
        if (strcmp(argv[i], "--touch") == 0) input.touch_ui = true;

    float accumulator = 0.0f, chop = 0.0f;
    bool quit = false;
    while (!quit && !WindowShouldClose()) {
        View view = fit_view();
        poll_touch(&input, view, &game);
        GameInput in = { 0 };
        read_keys(&input, &in);

        // Esc or the Android back button on the title screen closes the game.
        // (On the options page it goes back to the menu instead.)
        if (game.state == STATE_TITLE && game.title_page == PAGE_MAIN && (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACK)))
            quit = true;

        // Fixed steps keep the game the same at any frame rate.
        float frame = fminf(GetFrameTime(), 0.1f);
        accumulator += frame;
        if (chop > 0) chop = fmaxf(0.0f, chop + frame * 5.0f > 1.0f ? 0.0f : chop + frame * 5.0f);
        while (accumulator >= STEP_DT) {
            bool on_map = game.state == STATE_MAP;
            take_presses(&input, &in);
            if (game.state == STATE_TITLE) game.save_count = save_total;
            game_step(&game, &in);
            if (on_map && in.confirm_pressed) chop = 0.01f;  // swing the axe
            if (game.events & (EVENT_LANGUAGE | EVENT_SOUND)) {
                if (game.events & EVENT_LANGUAGE) text_set_portuguese(!text_is_portuguese());
                if (game.events & EVENT_SOUND) sound_on = !sound_on;
                settings_write(text_is_portuguese(), sound_on);
            }
            if (game.events & EVENT_QUIT) quit = true;
            if (game.events & EVENT_SAVE) {
                if (!save_write(&game)) TraceLog(LOG_WARNING, "could not save the game in %s", save_dir());
                refresh_saves();
            }
            if ((game.events & EVENT_LOAD) && game.title_cursor < save_total) {
                if (save_read(saves[game.title_cursor].slot, &game)) game_loaded(&game);
                else game.events |= EVENT_DENIED;
            }
            if (game.state == STATE_TITLE && game.title_page != PAGE_LOAD) load_first = 0;
            play_events(&sounds, game.events);
            accumulator -= STEP_DT;
        }
        update_music(&game, frame);

        BeginTextureMode(canvas);
        BeginMode2D(layout);
        draw_game(&game, &input, chop);
        EndMode2D();
        EndTextureMode();

        BeginDrawing();
        ClearBackground(BLACK);
        Rectangle src = { 0, 0, (float)canvas.texture.width, -(float)canvas.texture.height };
        Rectangle dst = { view.x, view.y, VIEW_W * view.scale, VIEW_H * view.scale };
        DrawTexturePro(canvas.texture, src, dst, (Vector2){ 0, 0 }, 0.0f, WHITE);
        EndDrawing();
    }

    free(saves);
    unload_music();
    unload_sounds(&sounds);
    unload_art();
    UnloadRenderTexture(canvas);
    CloseWindow();
    return 0;
}
