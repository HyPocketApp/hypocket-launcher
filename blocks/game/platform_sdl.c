/* HyPocket Blocks on PC (Linux and Windows): blocks.c with SDL2 around it
 * for the window, the OpenGL context, keyboard, mouse, controllers and
 * sound. There is no app around the game here, so the game draws its own
 * screens (BlocksPlatform.game_screens).
 *
 * Usage: blocks [--world <file>] [--seed <n>] [--chunks <8|16|24>]
 *               [--creative] [--peaceful] [--fps <limit>] [--size <w>x<h>]
 * Without --world the game keeps one world in the user's data folder. Sound
 * files are read from a "media" folder next to the program, if there is one.
 *
 * Controls: WASD to walk, the mouse to look, left button to break (hold to
 * keep hitting), right button to place or eat, Space to jump, the wheel or
 * 1-9 for the hotbar, E for the inventory, Esc for the menu, F3 for the
 * statistics. A controller works too. */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform.h"
#include "shared_io.h"

static SDL_Window* window;
static AudioRing* ring;
static PlatformInput input;
static SDL_GameController* pad;
static int captured;

static void* get_proc(const char* name) { return SDL_GL_GetProcAddress(name); }

static void present(void) { SDL_GL_SwapWindow(window); }

static void capture_pointer(int on) {
    captured = on;
    SDL_SetRelativeMouseMode(on ? SDL_TRUE : SDL_FALSE);
}

static void key_pressed(int key) {
    if (input.key_count < (int)(sizeof input.keys / sizeof input.keys[0])) input.keys[input.key_count++] = key;
}

/* Mouse look: about 0.15 degrees per count, as in the Android app. */
#define LOOK_PER_COUNT 0.0026f
#define STICK_DEAD 0.18f

static float stick(Sint16 v) {
    float f = v / 32767.0f;
    if (f > -STICK_DEAD && f < STICK_DEAD) return 0;
    return f > 0 ? (f - STICK_DEAD) / (1 - STICK_DEAD) : (f + STICK_DEAD) / (1 - STICK_DEAD);
}

static void frame(void* shared, int* width, int* height) {
    SharedIo* io = shared;
    static Uint32 break_held_since;
    static int trigger_down[2];
    input.clicks = input.right_clicks = input.wheel = input.key_count = 0;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT:
            io->quit = 1;
            break;
        case SDL_KEYDOWN:
            if (e.key.repeat) break;
            switch (e.key.keysym.sym) {
            case SDLK_ESCAPE: key_pressed(BLOCKS_KEY_MENU); break;
            case SDLK_e: case SDLK_TAB: key_pressed(BLOCKS_KEY_INVENTORY); break;
            case SDLK_RETURN: case SDLK_KP_ENTER: key_pressed(BLOCKS_KEY_CONFIRM); break;
            case SDLK_UP: key_pressed(BLOCKS_KEY_UP); break;
            case SDLK_DOWN: key_pressed(BLOCKS_KEY_DOWN); break;
            case SDLK_LEFT: key_pressed(BLOCKS_KEY_LEFT); break;
            case SDLK_RIGHT: key_pressed(BLOCKS_KEY_RIGHT); break;
            case SDLK_F3: key_pressed(BLOCKS_KEY_DEBUG); break;
            case SDLK_F11: {
                Uint32 full = SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP;
                SDL_SetWindowFullscreen(window, full ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
                break;
            }
            default:
                if (e.key.keysym.sym >= SDLK_1 && e.key.keysym.sym <= SDLK_9)
                    key_pressed(BLOCKS_KEY_HOTBAR_1 + (e.key.keysym.sym - SDLK_1));
            }
            break;
        case SDL_MOUSEMOTION:
            if (captured) {
                io->look_yaw += e.motion.xrel * LOOK_PER_COUNT;
                io->look_pitch -= e.motion.yrel * LOOK_PER_COUNT;
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (!captured) {
                if (e.button.button == SDL_BUTTON_LEFT) input.clicks++;
                if (e.button.button == SDL_BUTTON_RIGHT) input.right_clicks++;
            } else if (e.button.button == SDL_BUTTON_LEFT) {
                io->breaks++;
                break_held_since = SDL_GetTicks();
            } else if (e.button.button == SDL_BUTTON_RIGHT) {
                io->places++;
            }
            break;
        case SDL_MOUSEWHEEL:
            input.wheel += e.wheel.y;
            break;
        case SDL_CONTROLLERDEVICEADDED:
            if (!pad) pad = SDL_GameControllerOpen(e.cdevice.which);
            break;
        case SDL_CONTROLLERDEVICEREMOVED:
            if (pad && SDL_GameControllerGetJoystick(pad) &&
                SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad)) == e.cdevice.which) {
                SDL_GameControllerClose(pad);
                pad = NULL;
            }
            break;
        case SDL_CONTROLLERBUTTONDOWN:
            switch (e.cbutton.button) {
            case SDL_CONTROLLER_BUTTON_START: key_pressed(BLOCKS_KEY_MENU); break;
            case SDL_CONTROLLER_BUTTON_Y: key_pressed(BLOCKS_KEY_INVENTORY); break;
            case SDL_CONTROLLER_BUTTON_B: if (!captured) key_pressed(BLOCKS_KEY_MENU); break;
            case SDL_CONTROLLER_BUTTON_A: if (!captured) key_pressed(BLOCKS_KEY_CONFIRM); break;
            case SDL_CONTROLLER_BUTTON_DPAD_UP: key_pressed(BLOCKS_KEY_UP); break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN: key_pressed(BLOCKS_KEY_DOWN); break;
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT: key_pressed(BLOCKS_KEY_LEFT); break;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: key_pressed(BLOCKS_KEY_RIGHT); break;
            case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: input.wheel++; break;
            case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: input.wheel--; break;
            }
            break;
        }
    }
    /* Held mouse button: keep hitting, a blow every quarter second. */
    if (captured && (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_LEFT)) && break_held_since &&
        SDL_GetTicks() - break_held_since >= 250) {
        io->breaks++;
        break_held_since = SDL_GetTicks();
    }
    const Uint8* keys = SDL_GetKeyboardState(NULL);
    float mx = 0, my = 0;
    int jump = 0;
    if (captured) {
        mx = (keys[SDL_SCANCODE_D] ? 1.0f : 0) - (keys[SDL_SCANCODE_A] ? 1.0f : 0);
        my = (keys[SDL_SCANCODE_W] ? 1.0f : 0) - (keys[SDL_SCANCODE_S] ? 1.0f : 0);
        jump = keys[SDL_SCANCODE_SPACE] != 0;
    }
    io->look_rate_yaw = io->look_rate_pitch = 0;
    if (pad && captured) {
        float lx = stick(SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX));
        float ly = stick(SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY));
        if (lx || ly) { mx = lx; my = -ly; }
        io->look_rate_yaw = stick(SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTX)) * 2.6f;
        io->look_rate_pitch = -stick(SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTY)) * 2.0f;
        jump |= SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A);
        const int axes[2] = {SDL_CONTROLLER_AXIS_TRIGGERRIGHT, SDL_CONTROLLER_AXIS_TRIGGERLEFT};
        for (int t = 0; t < 2; t++) {
            int down = SDL_GameControllerGetAxis(pad, axes[t]) > 16000;
            if (down && !trigger_down[t]) {
                if (t == 0) io->breaks++;
                else io->places++;
            }
            trigger_down[t] = down;
        }
    }
    io->move_x = mx;
    io->move_y = my;
    io->jump = jump;
    if (!captured) {
        int x, y, ww, wh, dw, dh;
        SDL_GetMouseState(&x, &y);
        SDL_GetWindowSize(window, &ww, &wh);
        SDL_GL_GetDrawableSize(window, &dw, &dh);
        input.pointer_x = ww > 0 ? x * dw / ww : x;
        input.pointer_y = wh > 0 ? y * dh / wh : y;
    } else {
        input.pointer_x = input.pointer_y = -1;
    }
    SDL_GL_GetDrawableSize(window, width, height);
}

static void on_audio(void* user, Uint8* stream, int bytes) {
    (void)user;
    audio_ring_read(ring, (int16_t*)stream, bytes / 4);
}

static AudioRing* audio_ring(void) {
    SDL_AudioSpec want = {0}, got;
    want.freq = 48000;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 512;
    want.callback = on_audio;
    SDL_AudioDeviceID device = SDL_OpenAudioDevice(NULL, 0, &want, &got, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (!device) return NULL;
    uint32_t capacity = (uint32_t)got.freq / 2;
    ring = calloc(1, 64 + (size_t)capacity * 4);
    ring->version = 1;
    ring->rate = (uint32_t)got.freq;
    ring->channels = 2;
    ring->capacity = capacity;
    ring->magic = AUDIO_MAGIC;
    SDL_PauseAudioDevice(device, 0);
    return ring;
}

static void set_default(const char* name, const char* value) {
    if (!getenv(name)) SDL_setenv(name, value, 1);
}

int main(int argc, char** argv) {
    int w = 1280, h = 720, fps = 0, frames = 0;
    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        const char* next = i + 1 < argc ? argv[i + 1] : NULL;
        if (!strcmp(a, "--creative")) SDL_setenv("HYPOCKET_BLOCKS_CREATIVE", "1", 1);
        else if (!strcmp(a, "--peaceful")) SDL_setenv("HYPOCKET_BLOCKS_PEACEFUL", "1", 1);
        else if (!strcmp(a, "--world") && next) SDL_setenv("HYPOCKET_BLOCKS_SAVE", argv[++i], 1);
        else if (!strcmp(a, "--seed") && next) SDL_setenv("HYPOCKET_BLOCKS_SEED", argv[++i], 1);
        else if (!strcmp(a, "--chunks") && next) SDL_setenv("HYPOCKET_BLOCKS_CHUNKS", argv[++i], 1);
        else if (!strcmp(a, "--fps") && next) fps = atoi(argv[++i]);
        else if (!strcmp(a, "--size") && next) sscanf(argv[++i], "%dx%d", &w, &h);
        else if (!strcmp(a, "--frames") && next) frames = atoi(argv[++i]); /* for tests: walk on its own, then exit */
        else {
            fprintf(stderr, "usage: %s [--world <file>] [--seed <n>] [--chunks <8|16|24>] [--creative] [--peaceful] "
                            "[--fps <limit>] [--size <w>x<h>]\n", argv[0]);
            return 2;
        }
    }
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "blocks: SDL: %s\n", SDL_GetError());
        return 1;
    }
    /* One world kept in the user's data folder unless one is named. */
    char* data = SDL_GetPrefPath("HyPocket", "Blocks");
    if (data) {
        char path[4096];
        snprintf(path, sizeof path, "%sworld.hpw", data);
        set_default("HYPOCKET_BLOCKS_SAVE", path);
        if (!getenv("HYPOCKET_BLOCKS_SEED")) {
            char seed[16];
            snprintf(seed, sizeof seed, "%u", (unsigned)(SDL_GetPerformanceCounter() % 1000000));
            SDL_setenv("HYPOCKET_BLOCKS_SEED", seed, 1);
        }
        SDL_free(data);
    }
    char* base = SDL_GetBasePath();
    if (base) {
        char media[4096];
        snprintf(media, sizeof media, "%smedia", base);
        set_default("HYPOCKET_BLOCKS_MEDIA", media);
        SDL_free(base);
    }
    set_default("HYPOCKET_BLOCKS_CREATURES", "40");

    /* Desktop OpenGL 3.3 core; OpenGL ES 3.0 where that is all there is. */
    int gles = 0;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    window = SDL_CreateWindow("HyPocket Blocks", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h,
                              SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_GLContext context = window ? SDL_GL_CreateContext(window) : NULL;
    if (!context) {
        if (window) SDL_DestroyWindow(window);
        gles = 1;
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
        window = SDL_CreateWindow("HyPocket Blocks", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h,
                                  SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
        context = window ? SDL_GL_CreateContext(window) : NULL;
    }
    if (!context) {
        fprintf(stderr, "blocks: no OpenGL 3.3 or OpenGL ES 3 context: %s\n", SDL_GetError());
        return 1;
    }
    SDL_GL_SetSwapInterval(1);
    for (int i = 0; i < SDL_NumJoysticks() && !pad; i++)
        if (SDL_IsGameController(i)) pad = SDL_GameControllerOpen(i);

    static BlocksPlatform platform;
    platform.gles = gles;
    platform.get_proc = get_proc;
    platform.present = present;
    platform.frame = frame;
    platform.audio_ring = audio_ring;
    platform.game_screens = 1;
    platform.input = &input;
    platform.capture_pointer = capture_pointer;
    blocks_platform = &platform;
    capture_pointer(1);

    int dw, dh;
    SDL_GL_GetDrawableSize(window, &dw, &dh);
    char ws[16], hs[16], fs[16], ns[16];
    snprintf(ws, sizeof ws, "%d", dw);
    snprintf(hs, sizeof hs, "%d", dh);
    snprintf(fs, sizeof fs, "%d", fps);
    snprintf(ns, sizeof ns, "%d", frames);
    char* game_argv[] = {argv[0], ws, hs, fs, ns, NULL};
    int code = blocks_main(frames > 0 ? 5 : 4, game_argv);
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return code;
}
