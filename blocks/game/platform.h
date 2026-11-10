/* HyPocket Blocks: what the game needs from the place it runs.
 *
 * The game runs in three ways from the same blocks.c:
 *   - emulated: an x86-64 Linux program run by HyPocket's engine (Box64 and
 *     VirGL) on the phone. blocks.c makes its own EGL context and talks to the
 *     app through shared files. It stays, because it tests the engine.
 *   - native Android: blocks.c built into libblocks.so (android/), called
 *     from the app with a window (platform_android.c).
 *   - PC: a desktop program on SDL2 (platform_sdl.c), Linux and Windows.
 *
 * The native ways build blocks.c with BLOCKS_LIBRARY, make the OpenGL (or
 * OpenGL ES 3) context current, fill a BlocksPlatform and call blocks_main()
 * on that thread. */
#ifndef BLOCKS_PLATFORM_H
#define BLOCKS_PLATFORM_H

#include <stddef.h>
#include <stdint.h>

/* Ring of 16-bit stereo samples: the game writes, the sound device reads.
 * The emulated build shares it with the app as a file
 * (engine/src/main/cpp/audio_output.cpp); the native ways keep it in memory. */
#define AUDIO_MAGIC 0x41425048u /* "HPBA" */
typedef struct {
    uint32_t magic, version, rate, channels, capacity, reserved;
    uint64_t write_pos, read_pos;
    uint32_t underruns;
    uint32_t finished; /* set by the game when it stops writing */
} AudioRing;

/* Reads samples from [ring] into [out] (frames of two int16), silence when
 * the game has not written them yet. For the sound devices' callbacks. */
static inline void audio_ring_read(AudioRing* ring, int16_t* out, int frames) {
    const int16_t* samples = (const int16_t*)((char*)ring + 64);
    uint64_t read = __atomic_load_n(&ring->read_pos, __ATOMIC_ACQUIRE);
    uint64_t write = __atomic_load_n(&ring->write_pos, __ATOMIC_ACQUIRE);
    for (int f = 0; f < frames; f++) {
        if (read < write) {
            uint32_t i = (uint32_t)(read % ring->capacity);
            out[f * 2] = samples[i * 2];
            out[f * 2 + 1] = samples[i * 2 + 1];
            read++;
        } else {
            out[f * 2] = out[f * 2 + 1] = 0;
            if (f == 0) ring->underruns++;
        }
    }
    __atomic_store_n(&ring->read_pos, read, __ATOMIC_RELEASE);
}

/* Keys the game's own screens use (PlatformInput.keys). */
enum {
    BLOCKS_KEY_MENU = 1,   /* Esc: open the menu, or back */
    BLOCKS_KEY_INVENTORY,  /* E or Tab */
    BLOCKS_KEY_CONFIRM,    /* Enter */
    BLOCKS_KEY_UP, BLOCKS_KEY_DOWN, BLOCKS_KEY_LEFT, BLOCKS_KEY_RIGHT,
    BLOCKS_KEY_HOTBAR_1,   /* 1-9: BLOCKS_KEY_HOTBAR_1 + n */
    BLOCKS_KEY_HOTBAR_9 = BLOCKS_KEY_HOTBAR_1 + 8,
    BLOCKS_KEY_DEBUG,      /* F3: the statistics */
};

/* What a platform without an app around the game (the PC) sends to the
 * game's own screens each frame, besides the movement and actions it
 * writes into SharedIo. */
typedef struct {
    int pointer_x, pointer_y; /* pixels from the top left of the frame; -1 when there is none */
    int clicks;               /* left button presses since the last frame, on a free pointer */
    int right_clicks;
    int wheel;                /* wheel steps since the last frame, up positive */
    int keys[16];             /* BLOCKS_KEY_* pressed since the last frame */
    int key_count;
} PlatformInput;

typedef struct {
    /* The context is OpenGL ES 3.0 (else desktop OpenGL 3.3 core or newer). */
    int gles;
    /* Looks up an OpenGL function. */
    void* (*get_proc)(const char* name);
    /* Shows the frame, drawn into framebuffer 0 (the window). */
    void (*present)(void);
    /* Called at the start of each frame: input and window events. [io] is
     * the game's SharedIo (see blocks.c), to fill in like the app does.
     * Returns the window's size, which may have changed. */
    void (*frame)(void* io, int* width, int* height);
    /* A ring for the game's sound, already being played (NULL: silent). */
    AudioRing* (*audio_ring)(void);
    /* The game draws its own screens (menus, inventory, hotbar, meters)
     * over the frame: the PC version has no app around the game to draw
     * them. Then [input] is filled by [frame] and the game tells the
     * platform with [capture_pointer] whether the mouse looks around (1) or
     * points at its screens (0). */
    int game_screens;
    PlatformInput* input;
    void (*capture_pointer)(int captured);
} BlocksPlatform;

/* Set before blocks_main() by the native ways; NULL in the emulated build. */
extern const BlocksPlatform* blocks_platform;

int blocks_main(int argc, char** argv);

#endif
