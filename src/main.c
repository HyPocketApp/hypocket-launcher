// Window, input, drawing and sound, with raylib. The rules are in game.c.
//
// The same file builds for PC (Windows, Linux, macOS) and Android. On
// Android, raylib's NativeActivity glue calls main() for us. Everything is
// drawn by code (no image files) at 320x180 and scaled up to the screen.
#include "game.h"
#include "text.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "raylib.h"

// --- Colors -----------------------------------------------------------------

#define GRASS_A (Color){ 108, 176, 84, 255 }
#define GRASS_B (Color){ 100, 168, 78, 255 }
#define GRASS_TUFT (Color){ 78, 144, 66, 255 }
#define FLOWER (Color){ 250, 236, 140, 255 }
#define STONE (Color){ 128, 128, 140, 255 }
#define STONE_TOP (Color){ 168, 168, 180, 255 }
#define STONE_DARK (Color){ 90, 90, 104, 255 }
#define TRUNK (Color){ 110, 74, 48, 255 }
#define LEAVES (Color){ 50, 118, 64, 255 }
#define LEAVES_LIGHT (Color){ 78, 150, 80, 255 }
#define WATER (Color){ 64, 124, 200, 255 }
#define WATER_LIGHT (Color){ 120, 176, 236, 255 }
#define BRAMBLE (Color){ 52, 92, 52, 255 }
#define THORN (Color){ 196, 90, 90, 255 }
#define SEED (Color){ 200, 240, 110, 255 }
#define SEED_DARK (Color){ 120, 160, 50, 255 }
#define GOLDEN (Color){ 250, 200, 70, 255 }
#define WOOD (Color){ 150, 100, 60, 255 }
#define WOOD_DARK (Color){ 104, 66, 40, 255 }
#define HEART_RED (Color){ 230, 72, 86, 255 }
#define MOSS (Color){ 120, 200, 110, 255 }
#define MOSS_DARK (Color){ 70, 140, 80, 255 }
#define BEETLE (Color){ 150, 60, 50, 255 }
#define BEETLE_DARK (Color){ 90, 36, 32, 255 }
#define SLIME (Color){ 110, 190, 230, 255 }
#define SLIME_DARK (Color){ 60, 120, 180, 255 }
#define INK (Color){ 30, 30, 44, 255 }
#define SHADOW (Color){ 0, 0, 0, 60 }

// --- Sound ------------------------------------------------------------------

typedef struct {
    Sound swing, hit, defeat, hurt, seed, key, door, heart, portal, exit;
    bool ready;
} Sounds;

// Makes a short beep that slides from f0 to f1 Hz. With steps > 0 the pitch
// jumps in that many steps instead of sliding (an arpeggio). `noisy` adds
// a rough, hissing sound.
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
    s.swing = make_tone(900, 300, 0.08f, 0.10f, 0, true);
    s.hit = make_tone(300, 120, 0.10f, 0.22f, 0, false);
    s.defeat = make_tone(500, 60, 0.25f, 0.20f, 0, true);
    s.hurt = make_tone(440, 90, 0.35f, 0.22f, 0, true);
    s.seed = make_tone(988, 1319, 0.14f, 0.16f, 2, false);
    s.key = make_tone(660, 1320, 0.20f, 0.16f, 3, false);
    s.door = make_tone(160, 110, 0.20f, 0.22f, 0, true);
    s.heart = make_tone(523, 784, 0.25f, 0.16f, 3, false);
    s.portal = make_tone(392, 1568, 0.60f, 0.15f, 5, false);
    s.exit = make_tone(1047, 262, 0.50f, 0.15f, 0, false);
    s.ready = true;
    return s;
}

static void play_events(const Sounds *s, unsigned ev)
{
    if (!s->ready) return;
    if (ev & EVENT_SWING) PlaySound(s->swing);
    if (ev & EVENT_DEFEAT) PlaySound(s->defeat);
    else if (ev & EVENT_HIT) PlaySound(s->hit);
    if (ev & EVENT_HURT) PlaySound(s->hurt);
    if (ev & EVENT_PORTAL) PlaySound(s->portal);
    else if (ev & EVENT_SEED) PlaySound(s->seed);
    if (ev & EVENT_KEY) PlaySound(s->key);
    if (ev & EVENT_DOOR) PlaySound(s->door);
    if (ev & EVENT_HEART) PlaySound(s->heart);
    if (ev & EVENT_EXIT) PlaySound(s->exit);
}

static void unload_sounds(Sounds *s)
{
    if (!s->ready) return;
    Sound all[] = { s->swing, s->hit, s->defeat, s->hurt, s->seed, s->key, s->door, s->heart, s->portal, s->exit };
    for (int i = 0; i < (int)(sizeof all / sizeof all[0]); i++) UnloadSound(all[i]);
    CloseAudioDevice();
}

// --- Screen scaling -----------------------------------------------------------

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

// --- Input ------------------------------------------------------------------

// On-screen controls, in virtual pixels. The stick appears wherever a thumb
// touches the left half of the screen.
#define STICK_R 20.0f
static const Vector2 BTN_ATTACK = { 286, 146 };
#define ATTACK_R 22.0f
static const Vector2 BTN_PAUSE = { 306, 14 };

typedef struct {
    bool touch_ui;  // show on-screen controls
    int stick_id;   // touch point holding the stick, -1 when none
    Vector2 stick_origin, stick_pos;
    bool touch_attack, touch_pause;
    bool was_touching, was_attack, was_pause;
    bool attack_pending, start_pending;  // presses waiting for the next step
} InputState;

static bool near(Vector2 a, Vector2 b, float r)
{
    float dx = a.x - b.x, dy = a.y - b.y;
    return dx * dx + dy * dy <= r * r;
}

static void poll_touch(InputState *s, View v, bool playing)
{
    int count = GetTouchPointCount();
    bool stick_alive = false;
    s->touch_attack = s->touch_pause = false;
    for (int i = 0; i < count; i++) {
        int id = GetTouchPointId(i);
        Vector2 p = to_virtual(v, GetTouchPosition(i));
        if (id == s->stick_id) {
            stick_alive = true;
            s->stick_pos = p;
        } else if (near(p, BTN_PAUSE, 16)) {
            s->touch_pause = true;
        } else if (p.x >= VIEW_W / 2.0f) {
            s->touch_attack = true;  // the whole right half attacks
        } else if (s->stick_id < 0 || !stick_alive) {
            s->stick_id = id;
            s->stick_origin = s->stick_pos = p;
            stick_alive = true;
        }
    }
    if (!stick_alive) s->stick_id = -1;

    bool touching = count > 0;
    if (touching) s->touch_ui = true;
    if (playing) {
        if (s->touch_attack && !s->was_attack) s->attack_pending = true;
        if (s->touch_pause && !s->was_pause) s->start_pending = true;
    } else if (touching && !s->was_touching) {
        s->start_pending = true;  // tap anywhere on menus
    }
    s->was_touching = touching;
    s->was_attack = s->touch_attack;
    s->was_pause = s->touch_pause;
}

static GameInput read_input(InputState *s)
{
    GameInput in = { 0 };
    if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) in.move_x -= 1;
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) in.move_x += 1;
    if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) in.move_y -= 1;
    if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) in.move_y += 1;
    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_J) || IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_X))
        s->attack_pending = true;
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_P) || IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACK))
        s->start_pending = true;

    if (s->stick_id >= 0) {
        float dx = (s->stick_pos.x - s->stick_origin.x) / STICK_R;
        float dy = (s->stick_pos.y - s->stick_origin.y) / STICK_R;
        if (fabsf(dx) + fabsf(dy) > 0.25f) {  // small dead zone
            in.move_x += dx;
            in.move_y += dy;
        }
    }

    if (IsGamepadAvailable(0)) {
        float ax = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X);
        float ay = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y);
        if (fabsf(ax) > 0.2f) in.move_x += ax;
        if (fabsf(ay) > 0.2f) in.move_y += ay;
        if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT)) in.move_x -= 1;
        if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) in.move_x += 1;
        if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_UP)) in.move_y -= 1;
        if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_DOWN)) in.move_y += 1;
        if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN) ||
            IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_LEFT))
            s->attack_pending = true;
        if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT)) s->start_pending = true;
    }
    return in;
}

// --- Drawing helpers ----------------------------------------------------------

static void draw_text_shadow(const char *text, int x, int y, int size, Color color)
{
    DrawText(text, x + 1, y + 1, size, INK);
    DrawText(text, x, y, size, color);
}

static void draw_text_centered(const char *text, int y, int size, Color color)
{
    draw_text_shadow(text, (VIEW_W - MeasureText(text, size)) / 2, y, size, color);
}

static unsigned tile_hash(int tx, int ty)
{
    unsigned h = (unsigned)(tx * 73856093) ^ (unsigned)(ty * 19349663);
    return h ^ (h >> 13);
}

static void draw_heart(float x, float y, float s, Color c)
{
    DrawCircle((int)(x + 2 * s), (int)(y + 2 * s), 2.2f * s, c);
    DrawCircle((int)(x + 6 * s), (int)(y + 2 * s), 2.2f * s, c);
    DrawTriangle((Vector2){ x - 0.2f * s, y + 3 * s }, (Vector2){ x + 4 * s, y + 8 * s },
                 (Vector2){ x + 8.2f * s, y + 3 * s }, c);
}

static void draw_key(float x, float y, Color c)
{
    DrawCircleLines((int)x + 3, (int)y + 3, 2.5f, c);
    DrawCircle((int)x + 3, (int)y + 3, 1.5f, c);
    DrawRectangle((int)x + 5, (int)y + 2, 7, 2, c);
    DrawRectangle((int)x + 9, (int)y + 4, 1, 2, c);
    DrawRectangle((int)x + 11, (int)y + 4, 1, 3, c);
}

static void draw_seed(float cx, float cy, float glow)
{
    DrawCircle((int)cx, (int)cy, 5.0f + glow, Fade(SEED, 0.25f));
    DrawEllipse((int)cx, (int)cy, 3, 4, SEED_DARK);
    DrawEllipse((int)cx, (int)cy - 1, 2, 3, SEED);
}

// --- Map --------------------------------------------------------------------

static void draw_floor(int x, int y, int tx, int ty)
{
    DrawRectangle(x, y, TILE, TILE, ((tx + ty) & 1) ? GRASS_A : GRASS_B);
    unsigned h = tile_hash(tx, ty);
    if (h % 3 == 0) {
        int gx = x + 2 + (int)(h >> 4) % 10, gy = y + 4 + (int)(h >> 8) % 9;
        DrawRectangle(gx, gy, 1, 2, GRASS_TUFT);
        DrawRectangle(gx + 2, gy - 1, 1, 3, GRASS_TUFT);
    }
    if (h % 17 == 0) DrawRectangle(x + 4 + (int)(h >> 5) % 8, y + 3 + (int)(h >> 9) % 9, 2, 2, FLOWER);
}

static void draw_map(const Game *g, int cam_x, int cam_y)
{
    int x0 = (int)floorf(cam_x / (float)TILE), x1 = (cam_x + VIEW_W) / TILE + 1;
    int y0 = (int)floorf(cam_y / (float)TILE), y1 = (cam_y + VIEW_H) / TILE + 1;
    float t = g->time;
    for (int ty = y0; ty <= y1; ty++) {
        for (int tx = x0; tx <= x1; tx++) {
            int x = tx * TILE - cam_x, y = ty * TILE - cam_y;
            TileType tile = game_tile_at(g, tx, ty);
            if (tile != TILE_WALL && tile != TILE_WATER) draw_floor(x, y, tx, ty);
            switch (tile) {
            case TILE_WALL: {
                bool top_open = !(game_tile_at(g, tx, ty + 1) == TILE_WALL);
                DrawRectangle(x, y, TILE, TILE, STONE);
                DrawRectangle(x, y, TILE, 1, STONE_TOP);
                int off = (ty & 1) ? 0 : 8;
                DrawRectangle(x, y + 7, TILE, 1, STONE_DARK);
                DrawRectangle(x + off, y, 1, 7, STONE_DARK);
                DrawRectangle(x + ((off + 8) & 15), y + 8, 1, 8, STONE_DARK);
                if (top_open) DrawRectangle(x, y + TILE - 3, TILE, 3, STONE_DARK);  // front face
                break;
            }
            case TILE_TREE:
                DrawEllipse(x + 8, y + 14, 7, 2, SHADOW);
                DrawRectangle(x + 6, y + 9, 4, 6, TRUNK);
                DrawCircle(x + 8, y + 6, 7.5f, LEAVES);
                DrawCircle(x + 6, y + 4, 3.5f, LEAVES_LIGHT);
                break;
            case TILE_WATER: {
                DrawRectangle(x, y, TILE, TILE, WATER);
                float w = sinf(t * 2.0f + tx * 0.9f + ty * 0.6f);
                DrawRectangle(x + 2 + (int)(w * 2), y + 4, 5, 1, WATER_LIGHT);
                DrawRectangle(x + 8 - (int)(w * 2), y + 11, 5, 1, WATER_LIGHT);
                break;
            }
            case TILE_THORNS:
                DrawCircle(x + 5, y + 9, 4.5f, BRAMBLE);
                DrawCircle(x + 11, y + 8, 4.5f, BRAMBLE);
                DrawCircle(x + 8, y + 5, 4.0f, BRAMBLE);
                DrawRectangle(x + 3, y + 5, 1, 1, THORN);
                DrawRectangle(x + 12, y + 4, 1, 1, THORN);
                DrawRectangle(x + 8, y + 10, 1, 1, THORN);
                DrawRectangle(x + 14, y + 10, 1, 1, THORN);
                DrawRectangle(x + 6, y + 2, 1, 1, THORN);
                break;
            case TILE_SEED:
                draw_seed(x + 8.0f, y + 8.0f + sinf(t * 3.0f + tx) * 1.5f, sinf(t * 4.0f + ty) * 1.0f);
                break;
            case TILE_KEY:
                draw_key(x + 2.0f, y + 5.0f + sinf(t * 3.0f + tx) * 1.0f, GOLDEN);
                break;
            case TILE_HEART:
                draw_heart(x + 4.0f, y + 4.0f + sinf(t * 3.0f + tx) * 1.0f, 1.0f, HEART_RED);
                break;
            case TILE_DOOR:
                DrawRectangle(x, y, TILE, TILE, WOOD);
                for (int i = 4; i < TILE; i += 4) DrawRectangle(x + i, y, 1, TILE, WOOD_DARK);
                DrawRectangle(x, y + 3, TILE, 2, STONE_DARK);
                DrawRectangle(x, y + 11, TILE, 2, STONE_DARK);
                DrawCircle(x + 8, y + 7, 1.5f, INK);
                DrawRectangle(x + 7, y + 7, 2, 3, INK);
                break;
            case TILE_PORTAL: {
                bool open = game_portal_open(g);
                DrawCircle(x + 8, y + 8, 8, STONE_DARK);
                if (open) {
                    for (int i = 0; i < 3; i++) {
                        float a = t * (2.0f + i) + i * 2.1f;
                        Color c = i == 0 ? (Color){ 180, 120, 255, 255 }
                                         : i == 1 ? (Color){ 120, 220, 255, 255 } : (Color){ 255, 255, 255, 255 };
                        DrawRing((Vector2){ x + 8.0f, y + 8.0f }, 2.0f + i * 2, 3.0f + i * 2,
                                 a * 57.3f, a * 57.3f + 220, 12, c);
                    }
                } else {
                    DrawCircle(x + 8, y + 8, 5, (Color){ 60, 60, 74, 255 });
                }
                break;
            }
            default:
                break;
            }
        }
    }
}

// --- Characters ---------------------------------------------------------------

static void draw_player(const Game *g, int cam_x, int cam_y)
{
    const Player *p = &g->player;
    if (p->hurt_timer > 0 && fmodf(p->hurt_timer, 0.16f) < 0.08f && g->state == STATE_PLAYING) return;
    float cx = p->x + PLAYER_SIZE / 2.0f - cam_x, cy = p->y + PLAYER_SIZE / 2.0f - cam_y;
    float bob = p->walk_anim > 0 ? fabsf(sinf(p->walk_anim * 0.35f)) * 1.5f : 0.0f;
    bool down = g->state == STATE_DYING || g->state == STATE_GAME_OVER;

    DrawEllipse((int)cx, (int)cy + 5, 6, 2, SHADOW);
    DrawCircle((int)cx, (int)(cy - bob), 6.5f, MOSS_DARK);
    DrawCircle((int)cx, (int)(cy - bob - 0.5f), 5.5f, MOSS);
    // A sprout on top, leaning away from where it walks.
    DrawLine((int)cx, (int)(cy - bob - 6), (int)(cx - p->face_x * 2), (int)(cy - bob - 9), MOSS_DARK);
    DrawEllipse((int)(cx - p->face_x * 2 + 2), (int)(cy - bob - 9), 2.5f, 1.2f, LEAVES_LIGHT);

    if (p->face_y < 0 && p->face_x == 0) return;  // seen from the back: no face
    int ex = (int)(cx + p->face_x * 2), ey = (int)(cy - bob - 1 + p->face_y);
    if (down) {
        DrawText("x", ex - 4, ey - 4, 10, INK);
        DrawText("x", ex + 1, ey - 4, 10, INK);
    } else {
        DrawRectangle(ex - 3, ey - 1, 2, 3, INK);
        DrawRectangle(ex + 1, ey - 1, 2, 3, INK);
    }
}

static void draw_swing(const Game *g, int cam_x, int cam_y)
{
    const Player *p = &g->player;
    if (p->attack_timer <= 0 || g->state != STATE_PLAYING) return;
    float cx = p->x + PLAYER_SIZE / 2.0f - cam_x, cy = p->y + PLAYER_SIZE / 2.0f - cam_y;
    float angle = atan2f(p->face_y, p->face_x) * 57.2958f;
    float progress = 1.0f - p->attack_timer / ATTACK_TIME;
    float start = angle - 80 + progress * 60, end = start + 100;
    DrawRing((Vector2){ cx, cy }, 9, 15, start, end, 10, Fade(RAYWHITE, 0.8f - progress * 0.5f));
    DrawRing((Vector2){ cx, cy }, 13, 15, start, end, 10, Fade(LEAVES_LIGHT, 0.9f));
}

static void draw_enemies(const Game *g, int cam_x, int cam_y)
{
    for (int i = 0; i < g->enemy_count; i++) {
        const Enemy *e = &g->enemies[i];
        if (!e->alive) continue;
        float cx = e->x + ENEMY_SIZE / 2.0f - cam_x, cy = e->y + ENEMY_SIZE / 2.0f - cam_y;
        if (cx < -16 || cy < -16 || cx > VIEW_W + 16 || cy > VIEW_H + 16) continue;
        bool flash = e->hit_flash > 0;
        DrawEllipse((int)cx, (int)cy + 5, 6, 2, SHADOW);
        if (e->kind == ENEMY_BEETLE) {
            bool sideways = fabsf(e->vx) > fabsf(e->vy);
            float rx = sideways ? 6.5f : 5.0f, ry = sideways ? 5.0f : 6.5f;
            float legs = sinf(g->time * 20.0f + i) * 1.0f;
            for (int k = -1; k <= 1; k++) {
                if (sideways) {
                    DrawRectangle((int)(cx + k * 3 + legs), (int)(cy - ry - 1), 1, 2, INK);
                    DrawRectangle((int)(cx + k * 3 - legs), (int)(cy + ry - 1), 1, 2, INK);
                } else {
                    DrawRectangle((int)(cx - rx - 1), (int)(cy + k * 3 + legs), 2, 1, INK);
                    DrawRectangle((int)(cx + rx - 1), (int)(cy + k * 3 - legs), 2, 1, INK);
                }
            }
            DrawEllipse((int)cx, (int)cy, rx, ry, flash ? RAYWHITE : BEETLE_DARK);
            DrawEllipse((int)cx, (int)cy - 1, rx - 1, ry - 1, flash ? RAYWHITE : BEETLE);
            if (sideways) DrawRectangle((int)(cx - rx + 2), (int)cy - 1, (int)(rx * 2) - 4, 1, BEETLE_DARK);
            else DrawRectangle((int)cx, (int)(cy - ry + 2), 1, (int)(ry * 2) - 4, BEETLE_DARK);
        } else {
            float wob = sinf(g->time * 6.0f + i * 1.3f);
            float rx = 6.0f + wob * 0.8f, ry = 5.0f - wob * 0.6f;
            DrawEllipse((int)cx, (int)(cy + 1), rx, ry, flash ? RAYWHITE : SLIME_DARK);
            DrawEllipse((int)cx, (int)cy, rx - 1, ry - 1, flash ? RAYWHITE : SLIME);
            DrawRectangle((int)cx - 3, (int)cy - 3, 2, 1, Fade(RAYWHITE, 0.8f));
            DrawRectangle((int)cx - 2, (int)cy, 1, 2, INK);
            DrawRectangle((int)cx + 2, (int)cy, 1, 2, INK);
            if (e->health < 2) DrawRectangle((int)cx - 1, (int)cy + 3, 3, 1, INK);  // hurt frown
        }
    }
}

// --- HUD and screens ----------------------------------------------------------

static void draw_hud(const Game *g, float portal_message)
{
    for (int i = 0; i < MAX_HEALTH; i++)
        draw_heart(6.0f + i * 11, 6, 1.0f, i < g->player.health ? HEART_RED : Fade(INK, 0.4f));
    draw_seed(46, 11, 0);
    draw_text_shadow(TextFormat("%d/%d", g->seeds, g->level_seeds), 54, 6, 10, RAYWHITE);
    if (g->keys > 0) {
        draw_key(90, 8, GOLDEN);
        draw_text_shadow(TextFormat("%d", g->keys), 105, 6, 10, RAYWHITE);
    }
    const char *map = TextFormat(text_get(TXT_LEVEL), g->level + 1);
    draw_text_shadow(map, VIEW_W - MeasureText(map, 10) - 26, 6, 10, RAYWHITE);
    if (portal_message > 0) draw_text_centered(text_get(TXT_PORTAL_OPEN), 30, 10, Fade(SEED, fminf(1.0f, portal_message)));
}

static void draw_touch_controls(const InputState *s)
{
    if (s->stick_id >= 0) {
        DrawCircleV(s->stick_origin, STICK_R, Fade(RAYWHITE, 0.2f));
        float dx = s->stick_pos.x - s->stick_origin.x, dy = s->stick_pos.y - s->stick_origin.y;
        float len = sqrtf(dx * dx + dy * dy);
        if (len > STICK_R) { dx *= STICK_R / len; dy *= STICK_R / len; }
        DrawCircleV((Vector2){ s->stick_origin.x + dx, s->stick_origin.y + dy }, 8, Fade(RAYWHITE, 0.5f));
    } else {
        Vector2 hint = { 40, 140 };
        DrawCircleV(hint, STICK_R, Fade(RAYWHITE, 0.12f));
        DrawCircleV(hint, 8, Fade(RAYWHITE, 0.25f));
    }
    DrawCircleV(BTN_ATTACK, ATTACK_R, Fade(RAYWHITE, s->touch_attack ? 0.5f : 0.25f));
    DrawRing(BTN_ATTACK, 8, 11, -60, 120, 10, Fade(INK, 0.6f));
    DrawCircleV(BTN_PAUSE, 8, Fade(RAYWHITE, 0.25f));
    DrawRectangle((int)BTN_PAUSE.x - 3, (int)BTN_PAUSE.y - 4, 2, 8, Fade(INK, 0.6f));
    DrawRectangle((int)BTN_PAUSE.x + 1, (int)BTN_PAUSE.y - 4, 2, 8, Fade(INK, 0.6f));
}

static void draw_overlay(const Game *g, bool touch)
{
    TextId press = touch ? TXT_TAP_TO_CONTINUE : TXT_PRESS_TO_CONTINUE;
    switch (g->state) {
    case STATE_TITLE:
        DrawRectangle(0, 0, VIEW_W, VIEW_H, Fade(INK, 0.45f));
        draw_text_centered("MOSSLING", 36, 30, SEED);
        draw_text_centered(text_get(TXT_GOAL), 80, 10, RAYWHITE);
        draw_text_centered(text_get(touch ? TXT_TAP_TO_PLAY : TXT_PRESS_TO_PLAY), 106, 10, GOLDEN);
        if (!touch) draw_text_centered(text_get(TXT_CONTROLS), 156, 10, Fade(RAYWHITE, 0.85f));
        break;
    case STATE_PAUSED:
        DrawRectangle(0, 0, VIEW_W, VIEW_H, Fade(INK, 0.5f));
        draw_text_centered(text_get(TXT_PAUSED), 70, 20, RAYWHITE);
        draw_text_centered(text_get(press), 100, 10, RAYWHITE);
        break;
    case STATE_LEVEL_DONE:
        DrawRectangle(0, 0, VIEW_W, VIEW_H, Fade(RAYWHITE, 0.3f));
        draw_text_centered(text_get(TXT_LEVEL_DONE), 70, 20, SEED);
        break;
    case STATE_GAME_OVER:
        DrawRectangle(0, 0, VIEW_W, VIEW_H, Fade(INK, 0.6f));
        draw_text_centered(text_get(TXT_GAME_OVER), 64, 20, RAYWHITE);
        draw_text_centered(text_get(press), 100, 10, RAYWHITE);
        break;
    case STATE_WON:
        DrawRectangle(0, 0, VIEW_W, VIEW_H, Fade(INK, 0.5f));
        draw_text_centered(text_get(TXT_WON), 50, 20, SEED);
        draw_text_centered(TextFormat(text_get(TXT_TOTAL_SEEDS), g->total_seeds), 82, 10, RAYWHITE);
        draw_text_centered(text_get(press), 110, 10, RAYWHITE);
        break;
    default:
        break;
    }
}

static void draw_game(const Game *g, const InputState *s, float portal_message)
{
    int cam_x = (int)floorf(g->camera_x), cam_y = (int)floorf(g->camera_y);
    ClearBackground(STONE_DARK);
    draw_map(g, cam_x, cam_y);
    draw_enemies(g, cam_x, cam_y);
    if (g->state != STATE_TITLE) {
        draw_player(g, cam_x, cam_y);
        draw_swing(g, cam_x, cam_y);
    }
    if (g->state != STATE_TITLE && g->state != STATE_WON) draw_hud(g, portal_message);
    if (s->touch_ui && g->state == STATE_PLAYING) draw_touch_controls(s);
    draw_overlay(g, s->touch_ui);
}

// --- Main loop --------------------------------------------------------------

int main(int argc, char *argv[])
{
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
#if defined(PLATFORM_ANDROID)
    InitWindow(0, 0, "Mossling");
#else
    InitWindow(VIEW_W * 3, VIEW_H * 3, "Mossling");
    SetWindowMinSize(VIEW_W, VIEW_H);
#endif
    SetExitKey(KEY_NULL);  // Esc pauses instead of closing
    SetTargetFPS(60);
    text_init();

    RenderTexture2D canvas = LoadRenderTexture(VIEW_W, VIEW_H);
    SetTextureFilter(canvas.texture, TEXTURE_FILTER_POINT);
    Sounds sounds = load_sounds();

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

    float accumulator = 0.0f, portal_message = 0.0f;
    bool quit = false;
    while (!quit && !WindowShouldClose()) {
        View view = fit_view();
        poll_touch(&input, view, game.state == STATE_PLAYING);
        GameInput in = read_input(&input);

        // Esc or the Android back button on the title screen closes the game.
        if (game.state == STATE_TITLE && (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACK))) quit = true;

        // Fixed steps keep the game the same at any frame rate.
        float frame = fminf(GetFrameTime(), 0.1f);
        accumulator += frame;
        if (portal_message > 0) portal_message -= frame;
        while (accumulator >= STEP_DT) {
            in.attack_pressed = input.attack_pending;
            in.start_pressed = input.start_pending;
            input.attack_pending = input.start_pending = false;
            game_step(&game, &in);
            play_events(&sounds, game.events);
            if (game.events & EVENT_PORTAL) portal_message = 2.5f;
            accumulator -= STEP_DT;
        }

        BeginTextureMode(canvas);
        draw_game(&game, &input, portal_message);
        EndTextureMode();

        BeginDrawing();
        ClearBackground(BLACK);
        Rectangle src = { 0, 0, (float)VIEW_W, -(float)VIEW_H };
        Rectangle dst = { view.x, view.y, VIEW_W * view.scale, VIEW_H * view.scale };
        DrawTexturePro(canvas.texture, src, dst, (Vector2){ 0, 0 }, 0.0f, WHITE);
        EndDrawing();
    }

    unload_sounds(&sounds);
    UnloadRenderTexture(canvas);
    CloseWindow();
    return 0;
}
