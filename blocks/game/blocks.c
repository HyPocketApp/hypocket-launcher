/* HyPocket Blocks: a small, original block world used to test HyPocket's whole
 * path (x86-64 -> Box64 -> desktop OpenGL 4.1 -> VirGL -> phone GPU) with a
 * load shaped like a real game instead of a synthetic benchmark.
 *
 * The world is 8 x 8 chunks of 16 x 48 x 16 blocks (16 x 16 in heavy mode,
 * which also adds wandering creatures). Each chunk is meshed once
 * (and again when a block in it changes) into a vertex buffer that holds only
 * the faces next to air or water, with per-vertex ambient occlusion. Frames
 * are drawn into a framebuffer object and shown with eglSwapBuffers, which
 * HyPocket's Mesa turns into a direct present (HYPOCKET_PRESENT), or read
 * back into a shared file otherwise (HYPOCKET_FRAME_PATH, see
 * gl-display-probe).
 *
 * Input and statistics go through a small shared file (HYPOCKET_INPUT_PATH);
 * the layout is below and must match BlocksSession.kt. Sound is synthesized
 * here and mixed on an audio thread into a ring of samples in another shared
 * file (HYPOCKET_AUDIO_PATH), which the app plays (audio_output.cpp). Sound
 * files in HYPOCKET_BLOCKS_MEDIA (blocks/assets/audio) replace or add to the
 * made-up sounds: see "Sound files" in Audio.
 *
 * Usage: blocks <width> <height> [fps limit] [frames]
 * HYPOCKET_BLOCKS_DYNAMIC=1 lowers the 3D resolution when the game misses the
 * FPS limit while waiting for the GPU, and raises it again when it keeps up.
 * With [frames] the program exits after that many frames and walks on its
 * own, for testing on a desktop. With HYPOCKET_BLOCKS_BENCH=1 as well it is a
 * benchmark instead: the camera hovers above the start and turns a full
 * circle over the frames, so every run sees the same views, and it prints
 * the averages at the end.
 *
 * Worlds can be saved: with HYPOCKET_BLOCKS_SAVE=<file> the world is loaded
 * from that file if it holds one, generated otherwise, and written back every
 * 30 seconds after a change and on exit. New worlds take their size from
 * HYPOCKET_BLOCKS_CHUNKS (8, 16 or 24 chunks a side) and their terrain from
 * HYPOCKET_BLOCKS_SEED. HYPOCKET_BLOCKS_CREATURES sets how many creatures
 * wander outside heavy mode. Creatures are drawn with one instanced draw call;
 * HYPOCKET_BLOCKS_SEPARATE_DRAWS=1 draws each box on its own instead, the
 * load of a game with many separate objects (the benchmark uses it).
 *
 * Days pass: a full day takes 15 minutes (HYPOCKET_BLOCKS_DAY_SECONDS changes
 * that), and the sky, fog and sunlight follow the sun. At night lamps light
 * the blocks around them; that light is worked out per block when a chunk is
 * meshed (see chunk_light), so it costs nothing per frame. The time of day is
 * kept in the save. HYPOCKET_BLOCKS_TIME (0-1, 0.5 is noon) sets the starting
 * time, which the benchmark keeps at noon. HYPOCKET_BLOCKS_TIME_LOCK (0-1)
 * holds the time where it is for testing (0 is midnight) without changing the
 * time kept in the save.
 *
 * Breaking a block puts it in the player's inventory (grass gives dirt) and
 * placing one uses it up; the inventory is kept in the save. With
 * HYPOCKET_BLOCKS_CREATIVE=1 blocks are unlimited and nothing is collected.
 * The app sends recipes (its table is BlocksRecipes.kt); the game checks that
 * the inputs and the tool are carried and the workstation is near, and makes
 * the blocks or items.
 *
 * Survival (see Survival below): items that are not blocks (sticks, flint,
 * tools, food, materials, a coat), things lying on the ground, blocks that
 * take several blows or need a pickaxe, workstations (kiln, stove,
 * campfire), hunting the peaceful creatures, and hunger and cold. Creative
 * worlds and the benchmark have none of it.
 *
 * The story (blocks/docs/story.md): ruined villages with notes, night
 * creatures that grab and drag the player, shelters, rifts by the beacons and
 * the other side. HYPOCKET_BLOCKS_PEACEFUL=1 leaves the creatures out, and
 * HYPOCKET_BLOCKS_TEST_ROOM=1 (2: with a doorway) builds a closed room with a
 * lamp around the start, for testing shelters. */
#ifndef BLOCKS_LIBRARY
#include <EGL/egl.h>
#include <EGL/eglext.h>
#endif
#include "third_party/GL/glcorearb.h" /* Khronos, MIT: types and names for OpenGL and OpenGL ES 3 alike */
#include "platform.h"
#include <fcntl.h>
#include <pthread.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <sys/mman.h>
#endif
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#ifdef _WIN32
/* Windows (the PC version, through MinGW): its headers name two macros
 * near and far, and rename() does not replace a file. */
#include <io.h>
#include <windows.h>
#undef near
#undef far
#define fsync(fd) _commit(fd)
static int replace_file(const char* from, const char* to) {
    return MoveFileExA(from, to, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) ? 0 : -1;
}
#else
#define replace_file(from, to) rename(from, to)
#endif

/* Ogg Vorbis decoding for the sound files (see "Sound files" in Audio):
 * stb_vorbis 1.22 by Sean Barrett, MIT or public domain (third_party/). */
#define STB_VORBIS_NO_PUSHDATA_API
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-value"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#ifndef __clang__
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
#include "third_party/stb_vorbis.c"
#pragma GCC diagnostic pop

/* ---- Shared input/statistics file ------------------------------------- */

#include "shared_io.h"

/* ---- OpenGL entry points ---------------------------------------------- */

#define GL_FUNCTIONS(X) \
    X(PFNGLGETSTRINGPROC, glGetString) \
    X(PFNGLGETERRORPROC, glGetError) \
    X(PFNGLVIEWPORTPROC, glViewport) \
    X(PFNGLCLEARCOLORPROC, glClearColor) \
    X(PFNGLCLEARPROC, glClear) \
    X(PFNGLENABLEPROC, glEnable) \
    X(PFNGLDISABLEPROC, glDisable) \
    X(PFNGLBLENDFUNCPROC, glBlendFunc) \
    X(PFNGLDEPTHMASKPROC, glDepthMask) \
    X(PFNGLFINISHPROC, glFinish) \
    X(PFNGLREADPIXELSPROC, glReadPixels) \
    X(PFNGLBLITFRAMEBUFFERPROC, glBlitFramebuffer) \
    X(PFNGLCREATESHADERPROC, glCreateShader) \
    X(PFNGLSHADERSOURCEPROC, glShaderSource) \
    X(PFNGLCOMPILESHADERPROC, glCompileShader) \
    X(PFNGLGETSHADERIVPROC, glGetShaderiv) \
    X(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog) \
    X(PFNGLCREATEPROGRAMPROC, glCreateProgram) \
    X(PFNGLATTACHSHADERPROC, glAttachShader) \
    X(PFNGLLINKPROGRAMPROC, glLinkProgram) \
    X(PFNGLGETPROGRAMIVPROC, glGetProgramiv) \
    X(PFNGLUSEPROGRAMPROC, glUseProgram) \
    X(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation) \
    X(PFNGLUNIFORM1FPROC, glUniform1f) \
    X(PFNGLUNIFORM1IPROC, glUniform1i) \
    X(PFNGLUNIFORM2FPROC, glUniform2f) \
    X(PFNGLUNIFORM3FPROC, glUniform3f) \
    X(PFNGLUNIFORM4FPROC, glUniform4f) \
    X(PFNGLUNIFORMMATRIX4FVPROC, glUniformMatrix4fv) \
    X(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays) \
    X(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray) \
    X(PFNGLGENBUFFERSPROC, glGenBuffers) \
    X(PFNGLBINDBUFFERPROC, glBindBuffer) \
    X(PFNGLBUFFERDATAPROC, glBufferData) \
    X(PFNGLBUFFERSUBDATAPROC, glBufferSubData) \
    X(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer) \
    X(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray) \
    X(PFNGLDRAWELEMENTSPROC, glDrawElements) \
    X(PFNGLDRAWARRAYSPROC, glDrawArrays) \
    X(PFNGLDRAWARRAYSINSTANCEDPROC, glDrawArraysInstanced) \
    X(PFNGLVERTEXATTRIBDIVISORPROC, glVertexAttribDivisor) \
    X(PFNGLGENTEXTURESPROC, glGenTextures) \
    X(PFNGLBINDTEXTUREPROC, glBindTexture) \
    X(PFNGLTEXIMAGE3DPROC, glTexImage3D) \
    X(PFNGLTEXPARAMETERIPROC, glTexParameteri) \
    X(PFNGLGENERATEMIPMAPPROC, glGenerateMipmap) \
    X(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers) \
    X(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer) \
    X(PFNGLGENRENDERBUFFERSPROC, glGenRenderbuffers) \
    X(PFNGLBINDRENDERBUFFERPROC, glBindRenderbuffer) \
    X(PFNGLRENDERBUFFERSTORAGEPROC, glRenderbufferStorage) \
    X(PFNGLFRAMEBUFFERRENDERBUFFERPROC, glFramebufferRenderbuffer) \
    X(PFNGLCHECKFRAMEBUFFERSTATUSPROC, glCheckFramebufferStatus) \
    X(PFNGLFENCESYNCPROC, glFenceSync) \
    X(PFNGLCLIENTWAITSYNCPROC, glClientWaitSync) \
    X(PFNGLDELETESYNCPROC, glDeleteSync) \
    X(PFNGLDELETEFRAMEBUFFERSPROC, glDeleteFramebuffers) \
    X(PFNGLDELETERENDERBUFFERSPROC, glDeleteRenderbuffers)

#define DECLARE(type, name) static type p_##name;
GL_FUNCTIONS(DECLARE)

const BlocksPlatform* blocks_platform;
static int gl_es; /* OpenGL ES 3: the shaders get its header (see compile) */

/* ---- Small helpers ---------------------------------------------------- */

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

static void sleep_ms(double ms) {
    if (ms <= 0) return;
    struct timespec ts = {(time_t)(ms / 1000.0), (long)(fmod(ms, 1000.0) * 1e6)};
    nanosleep(&ts, NULL);
}

static uint32_t hash3(int x, int y, int z) {
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + (uint32_t)z * 2147483647u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

static float rand01(int x, int y, int z) { return (hash3(x, y, z) & 0xffffff) / 16777216.0f; }

/* Smooth value noise in 2D. */
static float noise2(float x, float z, int seed) {
    int xi = (int)floorf(x), zi = (int)floorf(z);
    float fx = x - xi, fz = z - zi;
    fx = fx * fx * (3 - 2 * fx);
    fz = fz * fz * (3 - 2 * fz);
    float a = rand01(xi, seed, zi), b = rand01(xi + 1, seed, zi);
    float c = rand01(xi, seed, zi + 1), d = rand01(xi + 1, seed, zi + 1);
    return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fz;
}

static float fbm(float x, float z, int seed) {
    float sum = 0, amp = 1, norm = 0;
    for (int i = 0; i < 4; i++) {
        sum += noise2(x, z, seed + i * 31) * amp;
        norm += amp;
        amp *= 0.5f;
        x *= 2.03f;
        z *= 2.03f;
    }
    return sum / norm;
}

/* ---- World ------------------------------------------------------------ */

/* Varies the terrain between worlds; 0 is the benchmark's world. */
static int world_seed;
#define SEEDED(n) ((n) + world_seed * 977)

#define CHUNK 16
/* 8 x 8 chunks, 16 x 16 in heavy mode (HYPOCKET_BLOCKS_HEAVY), up to 24 x 24
 * for a large saved world. */
#define MAX_CHUNKS 24
#define MAX_W (CHUNK * MAX_CHUNKS)
static int world_chunks = 8;
#define CHUNKS world_chunks
#define WX (CHUNK * CHUNKS)
#define WZ (CHUNK * CHUNKS)
#define WY 48
#define WATER_LEVEL 15

/* Block ids are stored in save files: only ever add to the end. */
enum { AIR, GRASS, DIRT, STONE, SAND, WOOD, LEAVES, WATER, PLANKS, STONE_BRICKS, BRICKS, GLASS, ROOF, PLASTER, LAMP,
       BEACON_OFF, BEACON_ON, /* the world's goal: see Beacons below; never built or broken by the player */
       SHADE,                 /* dark stone of the other side */
       KILN, STOVE, CAMPFIRE, /* workstations: some recipes are only made near one (see Survival) */
       BLOCK_TYPES };

/* Things that are not blocks and are never in the world: sticks, tools, food
 * and materials. They are carried like blocks (the inventory and the save
 * keep both by id), from ITEM_BASE up so that new blocks still fit below.
 * Only ever add to the end. */
#define ITEM_BASE 64
enum { STICK = ITEM_BASE, FLINT, KNIFE, SAW, AXE, PICK, BERRIES, RAW_MEAT, COOKED_MEAT, FRIED_MEAT, FAT, HIDE, WOOL,
       COAT, ITEM_END };
#define THING_TYPES 96 /* blocks and items: the size of the inventory */

/* Blocks that give light (lamp light, see chunk_light). */
static int gives_light(int b) { return b == LAMP || b == BEACON_ON || b == CAMPFIRE; }

/* What the player carries and builds with, in the order the app shows it
 * (must match BlocksPalette.kt): blocks first, then items. */
static const int palette[] = {PLANKS, STONE_BRICKS, BRICKS, GLASS, ROOF, PLASTER, LAMP,
                              WOOD, STONE, DIRT, SAND, LEAVES, SHADE, KILN, STOVE, CAMPFIRE,
                              STICK, FLINT, KNIFE, SAW, AXE, PICK, BERRIES, RAW_MEAT, COOKED_MEAT, FRIED_MEAT,
                              FAT, HIDE, WOOL, COAT};
#define PALETTE_SIZE ((int)(sizeof palette / sizeof palette[0]))

/* Where a block or item is in palette[], or -1. */
static int palette_index(int thing) {
    for (int k = 0; k < PALETTE_SIZE; k++)
        if (palette[k] == thing) return k;
    return -1;
}

/* Texture layers. */
enum { T_GRASS_TOP, T_GRASS_SIDE, T_DIRT, T_STONE, T_SAND, T_WOOD_SIDE, T_WOOD_TOP, T_LEAVES, T_WATER, T_PLANKS,
       T_STONE_BRICKS, T_BRICKS, T_GLASS, T_ROOF, T_PLASTER, T_LAMP, T_BEACON_OFF, T_BEACON_ON, T_SHADE,
       T_KILN, T_KILN_FRONT, T_STOVE_TOP, T_STOVE_SIDE, LAYERS };

static uint8_t world[MAX_W][WY][MAX_W];

static int block_at(int x, int y, int z) {
    if (y < 0) return STONE;
    if (x < 0 || z < 0 || x >= WX || z >= WZ || y >= WY) return AIR;
    return world[x][y][z];
}

/* Blocks that hide the faces behind them. */
static int is_opaque(int b) { return b != AIR && b != WATER && b != LEAVES && b != GLASS && b != CAMPFIRE; }
static int is_solid(int b) { return b != AIR && b != WATER; }
/* Drawn after the opaque world, blended. */
static int is_clear(int b) { return b == WATER || b == GLASS; }

static int texture_for(int block, int face) {
    /* face: 0 +x, 1 -x, 2 +y (top), 3 -y, 4 +z, 5 -z */
    switch (block) {
    case GRASS: return face == 2 ? T_GRASS_TOP : face == 3 ? T_DIRT : T_GRASS_SIDE;
    case DIRT: return T_DIRT;
    case STONE: return T_STONE;
    case SAND: return T_SAND;
    case WOOD: return face == 2 || face == 3 ? T_WOOD_TOP : T_WOOD_SIDE;
    case LEAVES: return T_LEAVES;
    case WATER: return T_WATER;
    case STONE_BRICKS: return T_STONE_BRICKS;
    case BRICKS: return T_BRICKS;
    case GLASS: return T_GLASS;
    case ROOF: return face == 3 ? T_PLANKS : T_ROOF;
    case PLASTER: return T_PLASTER;
    case LAMP: return T_LAMP;
    case BEACON_OFF: return face == 2 || face == 3 ? T_STONE_BRICKS : T_BEACON_OFF;
    case BEACON_ON: return face == 2 || face == 3 ? T_STONE_BRICKS : T_BEACON_ON;
    case SHADE: return T_SHADE;
    case KILN: return face == 4 ? T_KILN_FRONT : T_KILN;
    case STOVE: return face == 2 ? T_STOVE_TOP : T_STOVE_SIDE;
    default: return T_PLANKS;
    }
}

static void place_tree(int x, int ground, int z) {
    int height = 4 + (int)(rand01(x, SEEDED(7), z) * 2);
    for (int dy = -2; dy <= 2; dy++)
        for (int dx = -2; dx <= 2; dx++)
            for (int dz = -2; dz <= 2; dz++) {
                int y = ground + height + dy;
                int r = abs(dx) + abs(dz) + (dy > 0 ? dy * 2 : 0);
                if (r > 3 || (r == 3 && rand01(x + dx, y, z + dz) < 0.5f)) continue;
                int wx = x + dx, wz = z + dz;
                if (wx < 0 || wz < 0 || wx >= WX || wz >= WZ || y >= WY) continue;
                if (world[wx][y][wz] == AIR) world[wx][y][wz] = LEAVES;
            }
    for (int dy = 1; dy <= height && ground + dy < WY; dy++) world[x][ground + dy][z] = WOOD;
}

static void generate_world(void) {
    static int heights[MAX_W][MAX_W];
    for (int x = 0; x < WX; x++)
        for (int z = 0; z < WZ; z++) {
            float hills = fbm(x / 40.0f, z / 40.0f, SEEDED(1));
            float detail = fbm(x / 12.0f, z / 12.0f, SEEDED(9));
            int h = (int)(2 + hills * 34 + detail * 3);
            if (h > WY - 10) h = WY - 10;
            heights[x][z] = h;
            for (int y = 0; y < WY; y++) {
                int b = AIR;
                if (y <= h) {
                    if (y < h - 3) b = STONE;
                    else if (h <= WATER_LEVEL + 1) b = SAND;
                    else b = y == h ? GRASS : DIRT;
                } else if (y <= WATER_LEVEL) {
                    b = WATER;
                }
                world[x][y][z] = (uint8_t)b;
            }
        }
    for (int x = 2; x < WX - 2; x++)
        for (int z = 2; z < WZ - 2; z++)
            if (world[x][heights[x][z]][z] == GRASS && hash3(x, SEEDED(3), z) % 61 == 0) place_tree(x, heights[x][z], z);
}

/* ---- Meshing ---------------------------------------------------------- */

/* One vertex: position inside the chunk, normal index; texture corner
 * (u + 2 v), layer, ambient occlusion (0-3) and lamp light (0-LIGHT_MAX).
 * 8 bytes. */
typedef struct {
    uint8_t x, y, z, face;
    uint8_t uv, layer, ao, light;
} Vertex;

#define MAX_CHUNK_FIRES 8

typedef struct {
    GLuint vao, vbo;
    int quads;       /* opaque quads, first in the buffer */
    int water_quads; /* water and glass quads, after the opaque ones */
    int dirty; /* 2: its blocks changed, mesh now; 1: only its lamp light did */
    /* Campfires are not cubes: meshing lists them here and they are drawn
     * as boxes with the creatures (see draw_campfire). */
    int fire_count;
    int16_t fires[MAX_CHUNK_FIRES][3];
} Chunk;

static Chunk chunks[MAX_CHUNKS][MAX_CHUNKS];

static const int face_normal[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
/* Four corners per face, counter-clockwise seen from outside. */
static const int face_corners[6][4][3] = {
    {{1, 0, 1}, {1, 0, 0}, {1, 1, 0}, {1, 1, 1}},
    {{0, 0, 0}, {0, 0, 1}, {0, 1, 1}, {0, 1, 0}},
    {{0, 1, 1}, {1, 1, 1}, {1, 1, 0}, {0, 1, 0}},
    {{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}},
    {{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}},
    {{1, 0, 0}, {0, 0, 0}, {0, 1, 0}, {1, 1, 0}},
};
static const uint8_t corner_uv[4][2] = {{0, 1}, {1, 1}, {1, 0}, {0, 0}};

/* Classic voxel ambient occlusion: count the solid blocks around a corner. */
static int corner_ao(int x, int y, int z, int face, const int* c) {
    const int* n = face_normal[face];
    int ox = x + n[0], oy = y + n[1], oz = z + n[2];
    int axis_a[3] = {0}, axis_b[3] = {0};
    int a = 0, b = 0;
    for (int i = 0; i < 3; i++) {
        if (n[i] != 0) continue;
        int d = c[i] ? 1 : -1;
        if (!a) { axis_a[i] = d; a = 1; } else { axis_b[i] = d; b = 1; }
    }
    (void)b;
    int s1 = is_opaque(block_at(ox + axis_a[0], oy + axis_a[1], oz + axis_a[2]));
    int s2 = is_opaque(block_at(ox + axis_b[0], oy + axis_b[1], oz + axis_b[2]));
    int corner = is_opaque(block_at(ox + axis_a[0] + axis_b[0], oy + axis_a[1] + axis_b[1], oz + axis_a[2] + axis_b[2]));
    if (s1 && s2) return 0;
    return 3 - (s1 + s2 + corner);
}

/* ---- Lamp light ------------------------------------------------------- */

/* A lamp lights its own cell at LIGHT_MAX, and each step through a block
 * that is not opaque loses one level, so light reaches LIGHT_MAX - 1 blocks:
 * less than a chunk, so only the chunks next to a lamp's chunk can see it. */
#define LIGHT_MAX 12
#define LIGHT_REACH (LIGHT_MAX - 1)
#define LIGHT_SIDE (CHUNK + 2 * LIGHT_REACH)

/* Lamps in each chunk, so chunks with no lamp near them skip the lighting. */
static uint16_t chunk_lamps[MAX_CHUNKS][MAX_CHUNKS];

static void count_lamps(void) {
    memset(chunk_lamps, 0, sizeof chunk_lamps);
    for (int x = 0; x < WX; x++)
        for (int y = 0; y < WY; y++)
            for (int z = 0; z < WZ; z++)
                if (gives_light(world[x][y][z])) chunk_lamps[x / CHUNK][z / CHUNK]++;
}

static int lamps_near(int ci, int cj) {
    for (int i = ci - 1; i <= ci + 1; i++)
        for (int j = cj - 1; j <= cj + 1; j++)
            if (i >= 0 && j >= 0 && i < CHUNKS && j < CHUNKS && chunk_lamps[i][j]) return 1;
    return 0;
}

/* Lamp light around the chunk being meshed: the chunk plus LIGHT_REACH
 * blocks on each side, enough for every lamp that can reach it. */
static uint8_t light_grid[LIGHT_SIDE][WY][LIGHT_SIDE];
static int light_x0, light_z0, light_on;

static void chunk_light(int ci, int cj) {
    light_on = lamps_near(ci, cj);
    if (!light_on) return;
    light_x0 = ci * CHUNK - LIGHT_REACH;
    light_z0 = cj * CHUNK - LIGHT_REACH;
    memset(light_grid, 0, sizeof light_grid);
    /* Breadth first from every lamp at once: each cell is reached first at
     * its brightest, so it is queued only once. */
    static int32_t queue[LIGHT_SIDE * WY * LIGHT_SIDE];
    int head = 0, tail = 0;
    /* Lamps come only from the neighbouring chunks that have any. */
    for (int i = ci - 1; i <= ci + 1; i++)
        for (int j = cj - 1; j <= cj + 1; j++) {
            if (i < 0 || j < 0 || i >= CHUNKS || j >= CHUNKS || !chunk_lamps[i][j]) continue;
            int x0 = i * CHUNK > light_x0 ? i * CHUNK : light_x0;
            int x1 = (i + 1) * CHUNK < light_x0 + LIGHT_SIDE ? (i + 1) * CHUNK : light_x0 + LIGHT_SIDE;
            int z0 = j * CHUNK > light_z0 ? j * CHUNK : light_z0;
            int z1 = (j + 1) * CHUNK < light_z0 + LIGHT_SIDE ? (j + 1) * CHUNK : light_z0 + LIGHT_SIDE;
            for (int x = x0; x < x1; x++)
                for (int y = 0; y < WY; y++)
                    for (int z = z0; z < z1; z++)
                        if (gives_light(world[x][y][z])) {
                            int lx = x - light_x0, lz = z - light_z0;
                            light_grid[lx][y][lz] = LIGHT_MAX;
                            queue[tail++] = (lx * WY + y) * LIGHT_SIDE + lz;
                        }
        }
    while (head < tail) {
        int c = queue[head++];
        int lz = c % LIGHT_SIDE, y = c / LIGHT_SIDE % WY, lx = c / LIGHT_SIDE / WY;
        int next = light_grid[lx][y][lz] - 1;
        if (next <= 0) continue;
        for (int f = 0; f < 6; f++) {
            int nx = lx + face_normal[f][0], ny = y + face_normal[f][1], nz = lz + face_normal[f][2];
            if (nx < 0 || nz < 0 || ny < 0 || nx >= LIGHT_SIDE || nz >= LIGHT_SIDE || ny >= WY) continue;
            if (light_grid[nx][ny][nz] >= next || is_opaque(block_at(light_x0 + nx, ny, light_z0 + nz))) continue;
            light_grid[nx][ny][nz] = (uint8_t)next;
            queue[tail++] = (nx * WY + ny) * LIGHT_SIDE + nz;
        }
    }
}

static int light_at(int x, int y, int z) {
    int lx = x - light_x0, lz = z - light_z0;
    if (!light_on || y < 0 || y >= WY || lx < 0 || lz < 0 || lx >= LIGHT_SIDE || lz >= LIGHT_SIDE) return 0;
    return light_grid[lx][y][lz];
}

/* Smooth lamp light at a corner: the average over the open cells in front of
 * the face that touch the corner (the same cells ambient occlusion looks at). */
static int corner_light(int x, int y, int z, int face, const int* c) {
    if (!light_on) return 0;
    const int* n = face_normal[face];
    int ox = x + n[0], oy = y + n[1], oz = z + n[2];
    int a[3] = {0}, b[3] = {0}, have_a = 0;
    for (int i = 0; i < 3; i++) {
        if (n[i] != 0) continue;
        int d = c[i] ? 1 : -1;
        if (!have_a) { a[i] = d; have_a = 1; } else b[i] = d;
    }
    int open1 = !is_opaque(block_at(ox + a[0], oy + a[1], oz + a[2]));
    int open2 = !is_opaque(block_at(ox + b[0], oy + b[1], oz + b[2]));
    int sum = light_at(ox, oy, oz), count = 1;
    if (open1) { sum += light_at(ox + a[0], oy + a[1], oz + a[2]); count++; }
    if (open2) { sum += light_at(ox + b[0], oy + b[1], oz + b[2]); count++; }
    if ((open1 || open2) && !is_opaque(block_at(ox + a[0] + b[0], oy + a[1] + b[1], oz + a[2] + b[2]))) {
        sum += light_at(ox + a[0] + b[0], oy + a[1] + b[1], oz + a[2] + b[2]);
        count++;
    }
    return (sum + count / 2) / count;
}

static Vertex* scratch;
static int scratch_quads;
static int max_quads; /* size of the shared index buffer */

static int emit(int count, int cx, int cz, int x, int y, int z, int face, int block) {
    if (scratch_quads >= CHUNK * CHUNK * WY * 3) return count; /* cannot happen for real terrain */
    Vertex* v = scratch + scratch_quads * 4;
    int layer = texture_for(block, face);
    int water = block == WATER;
    for (int i = 0; i < 4; i++) {
        const int* c = face_corners[face][i];
        v[i].x = (uint8_t)(x - cx + c[0]);
        v[i].y = (uint8_t)(y + c[1]);
        v[i].z = (uint8_t)(z - cz + c[2]);
        v[i].face = (uint8_t)face;
        v[i].uv = (uint8_t)(corner_uv[i][0] + 2 * corner_uv[i][1]);
        v[i].layer = (uint8_t)layer;
        v[i].ao = (uint8_t)(water ? 3 : corner_ao(x, y, z, face, c));
        v[i].light = (uint8_t)corner_light(x, y, z, face, c);
    }
    scratch_quads++;
    return count + 1;
}

static void mesh_chunk(int ci, int cj) {
    Chunk* ch = &chunks[ci][cj];
    int cx = ci * CHUNK, cz = cj * CHUNK;
    scratch_quads = 0;
    chunk_light(ci, cj);
    int opaque = 0, water = 0;
    ch->fire_count = 0;
    for (int pass = 0; pass < 2; pass++)
        for (int x = cx; x < cx + CHUNK; x++)
            for (int y = 0; y < WY; y++)
                for (int z = cz; z < cz + CHUNK; z++) {
                    int b = world[x][y][z];
                    if (b == AIR || is_clear(b) != (pass == 1)) continue;
                    if (b == CAMPFIRE) {
                        if (ch->fire_count < MAX_CHUNK_FIRES) {
                            int16_t* f = ch->fires[ch->fire_count++];
                            f[0] = (int16_t)x; f[1] = (int16_t)y; f[2] = (int16_t)z;
                        }
                        continue;
                    }
                    for (int f = 0; f < 6; f++) {
                        const int* n = face_normal[f];
                        int other = block_at(x + n[0], y + n[1], z + n[2]);
                        int visible = b == WATER ? other == AIR || other == GLASS : !is_opaque(other) && other != b;
                        if (b == LEAVES && other == LEAVES) visible = 0;
                        if (y == 0 && f == 3) visible = 0;
                        if (!visible) continue;
                        if (pass == 0) opaque = emit(opaque, cx, cz, x, y, z, f, b);
                        else water = emit(water, cx, cz, x, y, z, f, b);
                    }
                }
    ch->quads = opaque;
    ch->water_quads = water;
    p_glBindBuffer(GL_ARRAY_BUFFER, ch->vbo);
    p_glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)scratch_quads * 4 * sizeof(Vertex), scratch, GL_STATIC_DRAW);
    if (scratch_quads > max_quads) {
        /* Grow the shared index buffer (two triangles per quad). */
        int n = scratch_quads + 1024;
        uint32_t* idx = malloc((size_t)n * 6 * sizeof *idx);
        for (int q = 0; q < n; q++) {
            uint32_t b = (uint32_t)q * 4;
            uint32_t* p = idx + q * 6;
            p[0] = b; p[1] = b + 1; p[2] = b + 2; p[3] = b; p[4] = b + 2; p[5] = b + 3;
        }
        p_glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)n * 6 * sizeof *idx, idx, GL_STATIC_DRAW);
        free(idx);
        max_quads = n;
    }
    ch->dirty = 0;
}

/* ---- Textures (generated, 16 x 16 per layer) ------------------------- */

#define TEX 16

static void make_textures(void) {
    static uint8_t px[LAYERS][TEX][TEX][4];
    for (int l = 0; l < LAYERS; l++)
        for (int y = 0; y < TEX; y++)
            for (int x = 0; x < TEX; x++) {
                float n = rand01(x, y, l * 17 + 3);
                float r, g, b, a = 1.0f;
                int edge = x == 0 || y == 0 || x == TEX - 1 || y == TEX - 1;
                switch (l) {
                case T_GRASS_TOP: r = 0.32f; g = 0.58f; b = 0.24f; n = 0.8f + 0.3f * n; break;
                case T_GRASS_SIDE:
                    if (y < 3 + (int)(rand01(x, 0, 99) * 2)) { r = 0.32f; g = 0.58f; b = 0.24f; }
                    else { r = 0.52f; g = 0.37f; b = 0.24f; }
                    n = 0.8f + 0.3f * n;
                    break;
                case T_DIRT: r = 0.52f; g = 0.37f; b = 0.24f; n = 0.75f + 0.35f * n; break;
                case T_STONE: r = g = b = 0.52f; n = 0.75f + 0.3f * n * (rand01(x / 3, y / 2, 5) > 0.3f ? 1.0f : 0.5f); break;
                case T_SAND: r = 0.86f; g = 0.80f; b = 0.58f; n = 0.9f + 0.15f * n; break;
                case T_WOOD_SIDE: r = 0.42f; g = 0.30f; b = 0.18f; n = (x % 4 == 0 ? 0.7f : 0.95f) + 0.1f * n; break;
                case T_WOOD_TOP: {
                    float dx = x - 7.5f, dy = y - 7.5f;
                    int ring = (int)sqrtf(dx * dx + dy * dy) % 3;
                    r = 0.62f; g = 0.48f; b = 0.30f; n = (ring == 0 ? 0.8f : 1.0f) - 0.1f * n;
                    if (x == 0 || y == 0 || x == TEX - 1 || y == TEX - 1) { r = 0.42f; g = 0.30f; b = 0.18f; }
                    break;
                }
                case T_LEAVES: r = 0.20f; g = 0.45f; b = 0.18f; n = 0.6f + 0.5f * n; break;
                case T_WATER: r = 0.16f; g = 0.32f; b = 0.68f; n = 0.9f + 0.15f * n; a = 0.78f; break;
                case T_STONE_BRICKS: /* large grey blocks, staggered rows */
                    r = g = b = 0.60f;
                    n = (y % 8 == 7 || x % 16 == ((y / 8) % 2 ? 4 : 12)) ? 0.62f : 0.85f + 0.2f * n;
                    break;
                case T_BRICKS: /* small red bricks with light mortar */
                    if (y % 4 == 3 || x % 8 == ((y / 4) % 2 ? 2 : 6)) { r = 0.76f; g = 0.72f; b = 0.66f; n = 0.95f; }
                    else { r = 0.64f; g = 0.30f; b = 0.23f; n = 0.85f + 0.25f * n; }
                    break;
                case T_GLASS: /* a light frame around a pane you can see through */
                    r = 0.78f; g = 0.88f; b = 0.95f; n = 1.0f;
                    if (edge) { r = 0.90f; g = 0.93f; b = 0.96f; }
                    else if ((x + y == 9 || x + y == 10 || x + y == 20) && x > 2 && y > 2) { r = g = b = 1.0f; a = 0.6f; }
                    else a = 0.18f;
                    break;
                case T_ROOF: /* overlapping clay tiles */
                    r = 0.66f; g = 0.30f; b = 0.19f;
                    n = (y % 4 == 3) ? 0.6f : (y % 4 == 0 ? 1.05f : 0.9f) + 0.1f * n;
                    if (x % 4 == ((y / 4) % 2 ? 1 : 3) && y % 4 != 3) n *= 0.8f;
                    break;
                case T_PLASTER: r = 0.88f; g = 0.86f; b = 0.80f; n = 0.92f + 0.08f * n; break;
                case T_LAMP: { /* a glowing panel in a dark frame */
                    float dx = x - 7.5f, dy = y - 7.5f;
                    r = 1.0f; g = 0.86f; b = 0.52f;
                    n = 1.0f - 0.025f * sqrtf(dx * dx + dy * dy) + 0.05f * n;
                    if (edge || x == 1 || y == 1 || x == TEX - 2 || y == TEX - 2) { r = 0.35f; g = 0.26f; b = 0.16f; n = 1.0f; }
                    break;
                }
                case T_BEACON_OFF: /* a cold grey crystal in a stone frame */
                case T_BEACON_ON: { /* the same crystal, shining */
                    int lit = l == T_BEACON_ON;
                    float dx = x - 7.5f, dy = y - 7.5f, d = fabsf(dx) + fabsf(dy);
                    if (edge || x == 1 || y == 1 || x == TEX - 2 || y == TEX - 2 || d > 8.5f) {
                        r = g = b = 0.50f; n = 0.85f + 0.2f * n; /* stone */
                    } else if (lit) {
                        r = 0.75f; g = 0.97f; b = 1.0f; n = 1.05f - 0.04f * d + 0.05f * n;
                    } else {
                        r = 0.32f; g = 0.34f; b = 0.42f; n = 0.9f + 0.2f * n - 0.02f * d;
                    }
                    break;
                }
                case T_SHADE: /* near-black violet stone with faint veins */
                    r = 0.17f; g = 0.13f; b = 0.23f; n = 0.75f + 0.35f * n;
                    if ((x * 3 + y * 5 + (int)(rand01(x / 4, y / 4, 13) * 7)) % 11 == 0) { r = 0.42f; g = 0.30f; b = 0.62f; n = 1.0f; }
                    break;
                case T_KILN: /* rough stones held with clay */
                case T_KILN_FRONT: { /* the same, with a glowing mouth */
                    r = 0.56f; g = 0.47f; b = 0.40f;
                    n = ((x + (y / 4) * 3) % 6 == 0 || y % 4 == 0) ? 0.6f : 0.82f + 0.25f * n;
                    float dx = x - 7.5f, dy = y - 9.5f;
                    if (l == T_KILN_FRONT && dx * dx * 0.6f + dy * dy < 14) { r = 1.0f; g = 0.48f; b = 0.16f; n = 0.75f + 0.3f * n; }
                    break;
                }
                case T_STOVE_TOP: { /* dark iron with two rings */
                    r = 0.20f; g = 0.20f; b = 0.22f; n = 0.85f + 0.2f * n;
                    for (int k = 0; k < 2; k++) {
                        float dx = x - (k ? 11.0f : 4.5f), dy = y - (k ? 4.5f : 10.5f), d = sqrtf(dx * dx + dy * dy);
                        if (d > 2.0f && d < 3.2f) { r = 0.45f; g = 0.43f; b = 0.42f; n = 1.0f; }
                    }
                    if (edge) { r = g = b = 0.12f; n = 1.0f; }
                    break;
                }
                case T_STOVE_SIDE: /* iron, with an oven door */
                    r = 0.24f; g = 0.24f; b = 0.26f; n = 0.85f + 0.15f * n;
                    if (x > 2 && x < 13 && y > 5 && y < 13) { r = 0.30f; g = 0.30f; b = 0.32f; }
                    if (y == 4 && x > 3 && x < 12) { r = 0.65f; g = 0.63f; b = 0.60f; n = 1.0f; } /* the handle */
                    if (edge) { r = g = b = 0.12f; n = 1.0f; }
                    break;
                default: /* planks */
                    r = 0.70f; g = 0.54f; b = 0.32f;
                    n = (y % 4 == 3 || (x == ((y / 4) % 2 ? 3 : 11) )) ? 0.65f : 0.9f + 0.12f * n;
                    break;
                }
                px[l][y][x][0] = (uint8_t)fminf(255, r * n * 255);
                px[l][y][x][1] = (uint8_t)fminf(255, g * n * 255);
                px[l][y][x][2] = (uint8_t)fminf(255, b * n * 255);
                px[l][y][x][3] = (uint8_t)(a * 255);
            }
    GLuint tex;
    p_glGenTextures(1, &tex);
    p_glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
    p_glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, TEX, TEX, LAYERS, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    p_glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
    p_glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    p_glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    p_glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    p_glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

/* ---- Shaders ---------------------------------------------------------- */

static const char* world_vs =
    "#version 330 core\n"
    "layout(location = 0) in vec4 pos_face;\n"
    "layout(location = 1) in vec4 uv_layer_ao_light;\n"
    "uniform mat4 view_proj;\n"
    "uniform vec3 chunk_origin;\n"
    "uniform vec3 eye;\n"
    "uniform float glow_layer;\n"
    "uniform float glow_layer2;\n"
    "uniform vec3 ambient;\n" /* sunlight or moonlight, from the time of day */
    "out vec3 uvl;\n"
    "out vec3 light;\n"
    "out float dist;\n"
    "const float face_light[6] = float[](0.80, 0.80, 1.00, 0.50, 0.65, 0.65);\n"
    "void main() {\n"
    "    vec3 world = chunk_origin + pos_face.xyz;\n"
    "    gl_Position = view_proj * vec4(world, 1.0);\n"
    "    float corner = uv_layer_ao_light.x;\n"
    "    uvl = vec3(mod(corner, 2.0), floor(corner * 0.5), uv_layer_ao_light.y);\n"
    "    float shade = face_light[int(pos_face.w)] * (0.55 + 0.15 * uv_layer_ao_light.z);\n"
    "    vec3 lamp = pow(uv_layer_ao_light.w / 12.0, 1.6) * 1.15 * (0.7 + 0.1 * uv_layer_ao_light.z) * vec3(1.0, 0.82, 0.56);\n"
    "    bool glow = abs(uv_layer_ao_light.y - glow_layer) < 0.5 || abs(uv_layer_ao_light.y - glow_layer2) < 0.5;\n"
    "    light = glow ? vec3(1.2) : max(shade * ambient, lamp);\n"
    "    dist = length(world - eye);\n"
    "}\n";

static const char* world_fs =
    "#version 330 core\n"
    "uniform sampler2DArray tex;\n"
    "uniform vec3 fog_color;\n"
    "uniform float fog_end;\n"
    "in vec3 uvl;\n"
    "in vec3 light;\n"
    "in float dist;\n"
    "out vec4 color;\n"
    "void main() {\n"
    "    vec4 t = texture(tex, uvl);\n"
    "    float fog = clamp((dist - fog_end * 0.6) / (fog_end * 0.4), 0.0, 1.0);\n"
    "    color = vec4(mix(t.rgb * light, fog_color, fog), t.a);\n"
    "}\n";

/* Crosshair: two thin rectangles in the middle of the screen. */
static const char* hud_vs =
    "#version 330 core\n"
    "uniform vec2 pixel;\n"
    "const vec2 quad[6] = vec2[](vec2(-1,-1), vec2(1,-1), vec2(1,1), vec2(-1,-1), vec2(1,1), vec2(-1,1));\n"
    "void main() {\n"
    "    vec2 q = quad[gl_VertexID % 6];\n"
    "    vec2 size = gl_VertexID < 6 ? vec2(10.0, 1.5) : vec2(1.5, 10.0);\n"
    "    gl_Position = vec4(q * size * pixel, 0.0, 1.0);\n"
    "}\n";

static const char* hud_fs =
    "#version 330 core\n"
    "out vec4 color;\n"
    "void main() { color = vec4(1.0, 1.0, 1.0, 0.85); }\n";

/* Sun and moon: two discs far away, drawn behind everything right after the
 * sky is cleared (no depth), in one draw call. A direction with w = 0 is not
 * moved by the camera's position, only turned with it. */
static const char* sky_vs =
    "#version 330 core\n"
    "uniform mat4 view_proj;\n"
    "uniform vec3 sun_dir;\n"
    "uniform float aspect;\n"
    "const vec2 quad[6] = vec2[](vec2(-1,-1), vec2(1,-1), vec2(1,1), vec2(-1,-1), vec2(1,1), vec2(-1,1));\n"
    "out vec2 q;\n"
    "flat out int body;\n"
    "void main() {\n"
    "    body = gl_VertexID / 6;\n"
    "    q = quad[gl_VertexID % 6];\n"
    "    vec4 c = view_proj * vec4(body == 0 ? sun_dir : -sun_dir, 0.0);\n"
    "    float size = body == 0 ? 0.11 : 0.075;\n"
    "    gl_Position = c.w > 0.01 ? vec4(c.xy / c.w + q * size * vec2(1.0 / aspect, 1.0), 0.0, 1.0) : vec4(2.0, 2.0, 0.0, 1.0);\n"
    "}\n";

static const char* sky_fs =
    "#version 330 core\n"
    "uniform vec4 sun_color;\n"
    "uniform vec4 moon_color;\n"
    "in vec2 q;\n"
    "flat in int body;\n"
    "out vec4 color;\n"
    "void main() {\n"
    "    float r = length(q);\n"
    "    if (body == 0) {\n" /* a bright core in a soft glow */
    "        float a = smoothstep(1.0, 0.45, r) * 0.55 + smoothstep(0.5, 0.42, r) * 0.45;\n"
    "        color = vec4(sun_color.rgb, sun_color.a * a);\n"
    "    } else {\n" /* a pale disc with two darker patches */
    "        float spots = smoothstep(0.32, 0.22, length(q - vec2(0.25, 0.2))) + smoothstep(0.22, 0.14, length(q + vec2(0.3, 0.1)));\n"
    "        color = vec4(moon_color.rgb * (1.0 - 0.18 * spots), moon_color.a * smoothstep(0.62, 0.56, r));\n"
    "    }\n"
    "}\n";

/* The shaders are written for OpenGL 3.3 core; OpenGL ES 3.0 takes the same
 * code with its own first line and default precisions. */
static GLuint compile(GLenum type, const char* source) {
    GLuint shader = p_glCreateShader(type);
    static const char desktop[] = "#version 330 core\n";
    static const char es[] = "#version 300 es\nprecision highp float;\nprecision highp int;\n"
                             "precision mediump sampler2DArray;\n";
    if (gl_es && strncmp(source, desktop, sizeof desktop - 1) == 0) {
        const char* parts[2] = {es, source + sizeof desktop - 1};
        p_glShaderSource(shader, 2, parts, NULL);
    } else {
        p_glShaderSource(shader, 1, &source, NULL);
    }
    p_glCompileShader(shader);
    GLint ok = 0;
    p_glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        p_glGetShaderInfoLog(shader, sizeof log, NULL, log);
        printf("blocks: shader error: %s\n", log);
    }
    return shader;
}

static GLuint link_program(const char* vs, const char* fs) {
    GLuint program = p_glCreateProgram();
    p_glAttachShader(program, compile(GL_VERTEX_SHADER, vs));
    p_glAttachShader(program, compile(GL_FRAGMENT_SHADER, fs));
    p_glLinkProgram(program);
    GLint ok = 0;
    p_glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        printf("blocks: link failed\n");
        exit(1);
    }
    return program;
}

/* ---- Player ----------------------------------------------------------- */

typedef struct {
    float x, y, z; /* feet */
    float vy;
    float yaw, pitch;
    int on_ground;
} Player;

#define PLAYER_HALF 0.3f
#define PLAYER_HEIGHT 1.8f
#define EYE_HEIGHT 1.62f

/* A campfire is low: stones and logs, walked over rather than into. */
#define CAMPFIRE_HEIGHT 0.25f

static int collides(float x, float y, float z) {
    int x0 = (int)floorf(x - PLAYER_HALF), x1 = (int)floorf(x + PLAYER_HALF);
    int y0 = (int)floorf(y), y1 = (int)floorf(y + PLAYER_HEIGHT);
    int z0 = (int)floorf(z - PLAYER_HALF), z1 = (int)floorf(z + PLAYER_HALF);
    for (int bx = x0; bx <= x1; bx++)
        for (int by = y0; by <= y1; by++)
            for (int bz = z0; bz <= z1; bz++) {
                int b = block_at(bx, by, bz);
                if (b == CAMPFIRE ? y < by + CAMPFIRE_HEIGHT : is_solid(b)) return 1;
            }
    /* Keep the player inside the world. */
    return x < 0.5f || z < 0.5f || x > WX - 0.5f || z > WZ - 0.5f;
}

static void move_player(Player* p, float dx, float dy, float dz) {
    /* One axis at a time, in small steps, so fast moves do not pass walls. */
    int steps = (int)ceilf(fmaxf(fmaxf(fabsf(dx), fabsf(dy)), fabsf(dz)) / 0.25f);
    if (steps < 1) steps = 1;
    for (int i = 0; i < steps; i++) {
        if (!collides(p->x + dx / steps, p->y, p->z)) p->x += dx / steps;
        if (!collides(p->x, p->y, p->z + dz / steps)) p->z += dz / steps;
        if (!collides(p->x, p->y + dy / steps, p->z)) {
            p->y += dy / steps;
            p->on_ground = 0;
        } else {
            if (dy < 0) p->on_ground = 1;
            p->vy = 0;
            break;
        }
    }
}

/* Steps through the grid along the view ray (Amanatides & Woo). Returns 1 if
 * a block was hit within reach, with the block and the cell before it. */
static int raycast(const Player* p, int hit[3], int before[3]) {
    float ox = p->x, oy = p->y + EYE_HEIGHT, oz = p->z;
    float dx = cosf(p->pitch) * sinf(p->yaw), dy = sinf(p->pitch), dz = -cosf(p->pitch) * cosf(p->yaw);
    int x = (int)floorf(ox), y = (int)floorf(oy), z = (int)floorf(oz);
    int sx = dx > 0 ? 1 : -1, sy = dy > 0 ? 1 : -1, sz = dz > 0 ? 1 : -1;
    float tx = dx != 0 ? ((sx > 0 ? x + 1 - ox : ox - x) / fabsf(dx)) : 1e9f;
    float ty = dy != 0 ? ((sy > 0 ? y + 1 - oy : oy - y) / fabsf(dy)) : 1e9f;
    float tz = dz != 0 ? ((sz > 0 ? z + 1 - oz : oz - z) / fabsf(dz)) : 1e9f;
    float ddx = dx != 0 ? 1 / fabsf(dx) : 1e9f, ddy = dy != 0 ? 1 / fabsf(dy) : 1e9f, ddz = dz != 0 ? 1 / fabsf(dz) : 1e9f;
    int px = x, py = y, pz = z;
    for (float t = 0; t < 6.0f;) {
        int b = block_at(x, y, z);
        if (b != AIR && b != WATER) {
            hit[0] = x; hit[1] = y; hit[2] = z;
            before[0] = px; before[1] = py; before[2] = pz;
            return y >= 0;
        }
        px = x; py = y; pz = z;
        if (tx < ty && tx < tz) { x += sx; t = tx; tx += ddx; }
        else if (ty < tz) { y += sy; t = ty; ty += ddy; }
        else { z += sz; t = tz; tz += ddz; }
    }
    return 0;
}

static void mark_dirty(int x, int z) {
    int ci = x / CHUNK, cj = z / CHUNK;
    chunks[ci][cj].dirty = 2;
    /* Faces and ambient occlusion of the neighbours can change too. */
    if (x % CHUNK == 0 && ci > 0) chunks[ci - 1][cj].dirty = 2;
    if (x % CHUNK == CHUNK - 1 && ci < CHUNKS - 1) chunks[ci + 1][cj].dirty = 2;
    if (z % CHUNK == 0 && cj > 0) chunks[ci][cj - 1].dirty = 2;
    if (z % CHUNK == CHUNK - 1 && cj < CHUNKS - 1) chunks[ci][cj + 1].dirty = 2;
}

static int world_changed; /* since the last save */

/* ---- Inventory -------------------------------------------------------- */

#define MAX_CARRIED 9999
#define MAX_HEALTH 10.0f
static uint16_t inventory[THING_TYPES]; /* blocks and items carried, by id */
static int creative;                    /* unlimited blocks, nothing collected */

/* Adds n of a block or item to what the player carries, up to MAX_CARRIED. */
static void give(int thing, int n) {
    inventory[thing] = (uint16_t)(inventory[thing] + n > MAX_CARRIED ? MAX_CARRIED : inventory[thing] + n);
}

/* Which workstations stand within reach of x, y, z: SURVIVAL_NEAR_* bits. */
static int stations_near(float x, float y, float z) {
    int bx = (int)floorf(x), by = (int)floorf(y), bz = (int)floorf(z), found = 0;
    for (int dx = -3; dx <= 3; dx++)
        for (int dy = -1; dy <= 2; dy++)
            for (int dz = -3; dz <= 3; dz++) {
                int b = block_at(bx + dx, by + dy, bz + dz);
                found |= b == KILN ? SURVIVAL_NEAR_KILN : b == STOVE ? SURVIVAL_NEAR_STOVE : b == CAMPFIRE ? SURVIVAL_NEAR_CAMPFIRE : 0;
            }
    return found;
}

/* Makes blocks or items from others if the inputs (and the tool) are
 * carried and the workstation is near: c holds the palette index and
 * amount of up to two inputs (255 for none), then of the output, then the
 * palette index of the workstation and of the tool (255 for none).
 * stations: what stations_near says where the player stands. */
static int craft(const uint8_t* c, int stations) {
    int in[2] = {-1, -1}, need[2] = {0, 0};
    for (int k = 0; k < 2; k++) {
        if (c[k * 2] == 255) continue;
        if (c[k * 2] >= PALETTE_SIZE || c[k * 2 + 1] == 0) return 0;
        in[k] = palette[c[k * 2]];
        need[k] = c[k * 2 + 1];
    }
    if (in[0] < 0 || c[4] >= PALETTE_SIZE || c[5] == 0) return 0;
    if (c[7] != 255 && (c[7] >= PALETTE_SIZE || !inventory[palette[c[7]]])) return 0;
    if (c[6] != 255) {
        int station = c[6] < PALETTE_SIZE ? palette[c[6]] : AIR;
        int bit = station == KILN ? SURVIVAL_NEAR_KILN : station == STOVE ? SURVIVAL_NEAR_STOVE
                : station == CAMPFIRE ? SURVIVAL_NEAR_CAMPFIRE : 0;
        if (!(stations & bit)) return 0;
    }
    int out = palette[c[4]];
    if (out == in[0] || out == in[1] || in[0] == in[1]) return 0;
    for (int k = 0; k < 2; k++)
        if (in[k] >= 0 && inventory[in[k]] < need[k]) return 0;
    if (inventory[out] + c[5] > MAX_CARRIED) return 0;
    for (int k = 0; k < 2; k++)
        if (in[k] >= 0) inventory[in[k]] -= (uint16_t)need[k];
    inventory[out] += c[5];
    return 1;
}

/* What breaking a block gives: the block itself, except grass gives dirt. */
static int drop_for(int block) {
    if (block == GRASS) return DIRT;
    if (block == AIR || block == WATER) return AIR;
    return block;
}

static void set_block(int x, int y, int z, int b) {
    if (x < 0 || z < 0 || x >= WX || z >= WZ || y < 0 || y >= WY) return;
    int old = world[x][y][z], ci = x / CHUNK, cj = z / CHUNK;
    world[x][y][z] = (uint8_t)b;
    world_changed = 1;
    if (gives_light(old)) chunk_lamps[ci][cj]--;
    if (gives_light(b)) chunk_lamps[ci][cj]++;
    mark_dirty(x, z);
    /* Near a lamp, any change can move light up to LIGHT_REACH blocks away. */
    if (gives_light(old) || gives_light(b) || lamps_near(ci, cj))
        for (int i = (x - LIGHT_REACH) / CHUNK; i <= (x + LIGHT_REACH) / CHUNK; i++)
            for (int j = (z - LIGHT_REACH) / CHUNK; j <= (z + LIGHT_REACH) / CHUNK; j++)
                if (i >= 0 && j >= 0 && i < CHUNKS && j < CHUNKS && !chunks[i][j].dirty) chunks[i][j].dirty = 1;
}

/* Water flows into a gap opened below sea level beside it: the gap and the
 * open cells it reaches sideways or downwards fill, up to a limit, so
 * digging next to the sea floods the hole instead of leaving a dry pocket. */
#define FLOOD_MAX 512
static void flood_from(int x, int y, int z) {
    static int16_t queue[FLOOD_MAX][3];
    if (y > WATER_LEVEL || block_at(x, y, z) != AIR) return;
    if (block_at(x + 1, y, z) != WATER && block_at(x - 1, y, z) != WATER && block_at(x, y, z + 1) != WATER &&
        block_at(x, y, z - 1) != WATER && block_at(x, y + 1, z) != WATER)
        return;
    int head = 0, tail = 0;
    set_block(x, y, z, WATER);
    queue[tail][0] = (int16_t)x; queue[tail][1] = (int16_t)y; queue[tail][2] = (int16_t)z; tail++;
    static const int step[5][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}, {0, -1, 0}};
    while (head < tail) {
        int cx = queue[head][0], cy = queue[head][1], cz = queue[head][2];
        head++;
        for (int k = 0; k < 5 && tail < FLOOD_MAX; k++) {
            int nx = cx + step[k][0], ny = cy + step[k][1], nz = cz + step[k][2];
            if (ny < 0 || nx < 0 || nz < 0 || nx >= WX || nz >= WZ || block_at(nx, ny, nz) != AIR) continue;
            set_block(nx, ny, nz, WATER);
            queue[tail][0] = (int16_t)nx; queue[tail][1] = (int16_t)ny; queue[tail][2] = (int16_t)nz; tail++;
        }
    }
}

/* ---- Time of day ------------------------------------------------------ */

/* 0 is midnight, 0.25 sunrise, 0.5 noon, 0.75 sunset. */
static float day_time = 0.3f;
static float time_lock = -1; /* >= 0: the time shown, held for testing */

/* The time the world is shown at: the held time, or the real one. */
static float shown_time(void) { return time_lock >= 0 ? time_lock : day_time; }
static uint32_t day_count; /* full days passed */

typedef struct {
    float sky[3];      /* clear and fog colour */
    float ambient[3];  /* sun or moon light on blocks and creatures */
    float sun_dir[3];  /* towards the sun; the moon is opposite */
    float sun_color[4], moon_color[4];
} Sky;

static float mixf(float a, float b, float t) { return a + (b - a) * t; }
static float smooth01(float e0, float e1, float x) {
    float t = (x - e0) / (e1 - e0);
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    return t * t * (3 - 2 * t);
}

static Sky sky_at(float t) {
    Sky s;
    float a = (t - 0.25f) * 6.2831853f;
    float dx = cosf(a), dy = sinf(a), dz = 0.35f, len = sqrtf(dx * dx + dy * dy + dz * dz);
    s.sun_dir[0] = dx / len; s.sun_dir[1] = dy / len; s.sun_dir[2] = dz / len;
    float e = s.sun_dir[1];                /* the sun's height, -1 to 1 */
    float day = smooth01(-0.12f, 0.22f, e); /* 0 at night, 1 in the day */
    /* Sunrise and sunset: strongest with the sun on the horizon, fading as
     * it climbs and soon after it sets. */
    float dusk = (1.0f - smooth01(0.0f, 0.2f, fabsf(e))) * smooth01(-0.16f, 0.0f, e);
    static const float day_sky[3] = {0.55f, 0.73f, 0.92f}, night_sky[3] = {0.035f, 0.045f, 0.11f};
    static const float dusk_sky[3] = {0.92f, 0.55f, 0.38f};
    static const float day_light[3] = {1.0f, 1.0f, 1.0f}, night_light[3] = {0.13f, 0.15f, 0.24f};
    static const float dusk_light[3] = {1.0f, 0.78f, 0.62f};
    for (int k = 0; k < 3; k++) {
        s.sky[k] = mixf(mixf(night_sky[k], day_sky[k], day), dusk_sky[k], dusk * 0.6f);
        s.ambient[k] = mixf(mixf(night_light[k], day_light[k], day), dusk_light[k] * day, dusk * 0.5f);
    }
    s.sun_color[0] = 1.0f; s.sun_color[1] = mixf(0.95f, 0.62f, dusk); s.sun_color[2] = mixf(0.78f, 0.35f, dusk);
    s.sun_color[3] = smooth01(-0.12f, 0.02f, e);
    s.moon_color[0] = 0.86f; s.moon_color[1] = 0.88f; s.moon_color[2] = 0.95f;
    s.moon_color[3] = smooth01(-0.12f, 0.02f, -e) * (1.0f - 0.6f * day);
    return s;
}

/* The other side has no sun and no moon: a dim violet night that never ends. */
static Sky other_sky(void) {
    Sky s;
    memset(&s, 0, sizeof s);
    s.sky[0] = 0.10f; s.sky[1] = 0.07f; s.sky[2] = 0.15f;
    s.ambient[0] = 0.24f; s.ambient[1] = 0.19f; s.ambient[2] = 0.32f;
    s.sun_dir[1] = -1;
    return s;
}

/* ---- Saved worlds ----------------------------------------------------- */

/* File layout (little endian): this header, then the blocks run-length
 * encoded as (count 1-255, block id) byte pairs, x outermost, then y, then z.
 * Must match BlocksWorlds.kt, which reads the header. */
#define SAVE_MAGIC 0x57425048u /* "HPBW" */
#define SAVE_VERSION 6
/* Version 1 had no time of day (those fields were reserved and zero) and
 * starts in the morning; versions 1 and 2 had no inventory and start with an
 * empty one. Version 3 adds the inventory after the block data: one 16-bit
 * count per block id, inventory_size bytes in all (block ids only ever grow,
 * so a longer list comes from a newer game and its extra counts are dropped).
 * Version 4 keeps the time played and the goal's state (beacons); earlier
 * versions get their beacons on load. Version 5 keeps the same layout and
 * adds goal_flags bits: GOAL_RUINS, GOAL_AWAY and, from NOTE_SHIFT up, the
 * notes found; earlier versions get their ruins on load. Version 6 keeps
 * items in the inventory (ids from ITEM_BASE) and adds, after it, a
 * SurvivalSave (food, warmth, health, what was picked up); earlier versions
 * start full and warm. */
typedef struct {
    uint32_t magic;
    uint32_t version;
    int32_t chunks; /* a side */
    int32_t seed;
    float x, y, z, yaw, pitch; /* the player */
    uint32_t data_size;        /* bytes of block data after the header */
    float time_of_day;         /* version 2: 0-1, 0.5 is noon */
    uint32_t days;             /* version 2: full days passed */
    uint32_t inventory_size;   /* version 3: bytes of inventory after the block data */
    float play_seconds;        /* version 4: time played in this world */
    uint32_t goal_flags;       /* version 4: GOAL_PLACED | GOAL_DONE; version 5: see below */
    float done_seconds;        /* version 4: time played when the last beacon was lit */
} SaveHeader;

#define GOAL_PLACED 1u /* the beacons were put in the world */
#define GOAL_DONE 2u   /* every beacon is lit */
#define GOAL_RUINS 4u  /* version 5: the ruins were put in the world */
#define GOAL_AWAY 8u   /* version 5: saved on the other side (the position is where the player was taken) */
#define NOTE_SHIFT 16  /* version 5: goal_flags bit NOTE_SHIFT + n: note n was found */
static float play_seconds, done_seconds;
static uint32_t goal_flags;

/* Version 6: after the inventory. It starts with its own size, so later
 * versions can add to the end. */
typedef struct {
    uint32_t size;
    float food, warmth, health;
    uint32_t tools_taken;    /* bit k: found tool k was picked up (see Pickups) */
    uint32_t daily_day;      /* the day the daily pickups below were taken on */
    uint32_t daily_taken[2]; /* bit k: daily pickup k was taken that day */
} SurvivalSave;

#define MAX_FOOD 10.0f
#define MAX_WARMTH 10.0f
static float food = MAX_FOOD, warmth = MAX_WARMTH, saved_health = 10.0f;
static uint32_t tools_taken, daily_day, daily_taken[2];

/* Writes to a temporary file and renames it over the old save, so a save
 * cut short (the app killed mid-write) never destroys the previous one. */
/* The blocks, run-length encoded as in a save (malloc'd; the size in *size). */
static uint8_t* encode_world(size_t* size) {
    size_t cells = (size_t)WX * WY * WZ;
    uint8_t* data = malloc(cells * 2);
    if (!data) return NULL;
    *size = 0;
    int run_block = -1, run = 0;
    for (int x = 0; x < WX; x++)
        for (int y = 0; y < WY; y++)
            for (int z = 0; z < WZ; z++) {
                int b = world[x][y][z];
                if (b == run_block && run < 255) { run++; continue; }
                if (run) { data[(*size)++] = (uint8_t)run; data[(*size)++] = (uint8_t)run_block; }
                run_block = b;
                run = 1;
            }
    if (run) { data[(*size)++] = (uint8_t)run; data[(*size)++] = (uint8_t)run_block; }
    return data;
}

/* The other side (see below): while the player is there, the world they
 * came from waits here, encoded, with where they were taken. */
static int away; /* on the other side: all mist */
static uint8_t* home_data;
static size_t home_size;
static int home_chunks;
static float home_x, home_y, home_z;

static int save_world(const char* path, const Player* p) {
    size_t size = 0;
    uint8_t* data = away ? NULL : encode_world(&size);
    if (away) { /* save the world left behind, with the player where they were taken */
        data = malloc(home_size ? home_size : 1);
        if (data) memcpy(data, home_data, home_size);
        size = home_size;
    }
    if (!data) return 0;
    SaveHeader h = {SAVE_MAGIC, SAVE_VERSION, away ? home_chunks : world_chunks, world_seed,
                    away ? home_x : p->x, away ? home_y : p->y, away ? home_z : p->z, p->yaw, p->pitch,
                    (uint32_t)size, day_time, day_count, (uint32_t)sizeof inventory, play_seconds,
                    goal_flags | (away ? GOAL_AWAY : 0), done_seconds};
    char tmp[4096];
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE* f = fopen(tmp, "wb");
    SurvivalSave sv = {sizeof sv, food, warmth, saved_health, tools_taken, daily_day, {daily_taken[0], daily_taken[1]}};
    int ok = f && fwrite(&h, sizeof h, 1, f) == 1 && fwrite(data, 1, size, f) == size &&
             fwrite(inventory, sizeof inventory, 1, f) == 1 && fwrite(&sv, sizeof sv, 1, f) == 1;
    if (f) {
        ok = fflush(f) == 0 && ok;
        ok = fsync(fileno(f)) == 0 && ok;
        ok = fclose(f) == 0 && ok;
    }
    free(data);
    if (ok) ok = replace_file(tmp, path) == 0;
    if (!ok) {
        unlink(tmp);
        printf("blocks: could not save the world to %s\n", path);
    }
    return ok;
}

/* Loads a saved world and the player's place in it. Returns 0, leaving the
 * world untouched, when there is no valid save. */
static int load_world(const char* path, Player* p) {
    FILE* f = fopen(path, "rb");
    if (!f) return 0;
    SaveHeader h;
    int ok = fread(&h, sizeof h, 1, f) == 1 && h.magic == SAVE_MAGIC && h.version >= 1 && h.version <= SAVE_VERSION &&
             h.chunks >= 4 && h.chunks <= MAX_CHUNKS && h.data_size <= (uint32_t)MAX_W * WY * MAX_W * 2;
    uint8_t* data = ok ? malloc(h.data_size ? h.data_size : 1) : NULL;
    ok = ok && data && fread(data, 1, h.data_size, f) == h.data_size;
    uint16_t carried[256] = {0};
    if (ok && h.version >= 3)
        ok = h.inventory_size <= sizeof carried && fread(carried, 1, h.inventory_size, f) == h.inventory_size;
    SurvivalSave sv = {0, MAX_FOOD, MAX_WARMTH, 10.0f, 0, 0, {0, 0}};
    if (ok && h.version >= 6) {
        ok = fread(&sv.size, sizeof sv.size, 1, f) == 1 && sv.size >= sizeof sv.size && sv.size <= 4096;
        size_t rest = (sv.size < sizeof sv ? sv.size : sizeof sv) - sizeof sv.size;
        ok = ok && fread((char*)&sv + sizeof sv.size, 1, rest, f) == rest;
    }
    fclose(f);
    if (ok) {
        /* Decode into a copy first: a bad file must not leave half a world. */
        int side = CHUNK * h.chunks;
        size_t cells = (size_t)side * WY * side, done = 0;
        uint8_t* cellv = malloc(cells);
        ok = cellv != NULL;
        for (uint32_t i = 0; ok && i + 1 < h.data_size; i += 2) {
            int run = data[i], b = data[i + 1];
            if (run == 0 || b >= BLOCK_TYPES || done + run > cells) { ok = 0; break; }
            memset(cellv + done, b, run);
            done += run;
        }
        ok = ok && done == cells;
        if (ok) {
            world_chunks = h.chunks;
            world_seed = h.seed;
            size_t k = 0;
            for (int x = 0; x < side; x++)
                for (int y = 0; y < WY; y++)
                    for (int z = 0; z < side; z++) world[x][y][z] = cellv[k++];
            p->x = h.x; p->y = h.y; p->z = h.z;
            p->yaw = h.yaw; p->pitch = h.pitch;
            p->vy = 0;
            memset(inventory, 0, sizeof inventory);
            if (h.version >= 3)
                for (int b = 0; b < THING_TYPES; b++) inventory[b] = carried[b] > MAX_CARRIED ? MAX_CARRIED : carried[b];
            if (h.version >= 6) {
                food = sv.food >= 0 && sv.food <= MAX_FOOD ? sv.food : MAX_FOOD;
                warmth = sv.warmth >= 0 && sv.warmth <= MAX_WARMTH ? sv.warmth : MAX_WARMTH;
                saved_health = sv.health > 0 && sv.health <= 10.0f ? sv.health : 10.0f;
                tools_taken = sv.tools_taken;
                daily_day = sv.daily_day;
                daily_taken[0] = sv.daily_taken[0];
                daily_taken[1] = sv.daily_taken[1];
            }
            if (h.version >= 4) {
                play_seconds = h.play_seconds >= 0 ? h.play_seconds : 0;
                goal_flags = h.goal_flags;
                done_seconds = h.done_seconds;
            }
            if (h.version >= 2 && h.time_of_day >= 0 && h.time_of_day < 1) {
                day_time = h.time_of_day;
                day_count = h.days;
            }
        }
        free(cellv);
    }
    free(data);
    if (!ok) printf("blocks: %s is not a valid saved world, making a new one\n", path);
    return ok;
}

/* ---- Beacons: the world's goal ----------------------------------------- */

/* Five dark beacons stand on stone pillars in a ring around the start, where
 * the seed puts them (so saves need not keep where). Near an unlit one there
 * is a mist, where night creatures come out even by day. Lighting a beacon
 * (BEACON_COST) clears its mist, lights the blocks around it and keeps night
 * creatures BEACON_CALM blocks away: around it is a sanctuary, free of mist and
 * lit even at night, whose edge night creatures do not cross. Lighting all
 * five finishes the world. */
#define BEACONS 5
#define BEACON_CALM 40.0f
#define MIST_NEAR 22.0f /* thick mist this close to an unlit beacon */
#define MIST_FAR 40.0f  /* none beyond */
#define BEACON_LAMPS 2
#define BEACON_GLASS 3
static int beacon_x[BEACONS], beacon_y[BEACONS], beacon_z[BEACONS]; /* beacon_y: -1 when missing */
/* Beside each unlit beacon, a rift: a tear in the air where the mist of the
 * other side leaks through. Lighting the beacon seals it. Drawn as boxes. */
static float rift_x[BEACONS], rift_y[BEACONS], rift_z[BEACONS];
/* The portal they came through, which no beacon closes: the way to the other
 * side. portal_here: shown in this world. */
static float portal_x, portal_y, portal_z, portal_yaw;
static int portal_here;

static void beacon_spot(int k, int* x, int* z) {
    float a = k * 1.2566371f + rand01(world_seed, 5, 77) * 6.2831853f;
    float r = WX * 0.36f;
    int bx = (int)(WX / 2 + cosf(a) * r), bz = (int)(WZ / 2 + sinf(a) * r);
    *x = bx < 8 ? 8 : bx > WX - 9 ? WX - 9 : bx;
    *z = bz < 8 ? 8 : bz > WZ - 9 ? WZ - 9 : bz;
}

/* Builds the pillars and the dark beacons on top (new worlds, and worlds
 * saved before beacons existed). */
static void place_beacons(void) {
    for (int k = 0; k < BEACONS; k++) {
        int x, z;
        beacon_spot(k, &x, &z);
        int y = WY - 7;
        while (y > 0 && (world[x][y][z] == AIR || world[x][y][z] == LEAVES || world[x][y][z] == WOOD)) y--;
        /* Clear any tree around, then the pillar and the beacon. */
        for (int dx = -2; dx <= 2; dx++)
            for (int dz = -2; dz <= 2; dz++)
                for (int dy = 1; dy <= 6 && y + dy < WY; dy++) {
                    int b = world[x + dx][y + dy][z + dz];
                    if (b == LEAVES || b == WOOD) world[x + dx][y + dy][z + dz] = AIR;
                }
        for (int dy = 1; dy <= 3; dy++) world[x][y + dy][z] = STONE_BRICKS;
        world[x][y + 4][z] = BEACON_OFF;
    }
}

static void find_beacons(void) {
    for (int k = 0; k < BEACONS; k++) {
        beacon_spot(k, &beacon_x[k], &beacon_z[k]);
        beacon_y[k] = -1;
        for (int y = WY - 1; y >= 0 && beacon_y[k] < 0; y--) {
            int b = world[beacon_x[k]][y][beacon_z[k]];
            if (b == BEACON_OFF || b == BEACON_ON) beacon_y[k] = y;
        }
        int rx = beacon_x[k] + 2, rz = beacon_z[k];
        int y = 0;
        while (y < WY - 1 && world[rx][y][rz] != AIR) y++;
        rift_x[k] = rx + 0.5f;
        rift_y[k] = (float)y;
        rift_z[k] = rz + 0.5f;
    }
}

/* The portal: between two beacons, nearer the start, where the seed puts it. */
static float portal_spot(int* x, int* z) {
    float a = 2.5f * 1.2566371f + rand01(world_seed, 5, 77) * 6.2831853f, r = WX * 0.28f;
    *x = (int)(WX / 2 + cosf(a) * r);
    *z = (int)(WZ / 2 + sinf(a) * r);
    *x = *x < 4 ? 4 : *x > WX - 5 ? WX - 5 : *x;
    *z = *z < 4 ? 4 : *z > WZ - 5 ? WZ - 5 : *z;
    return a;
}

/* Clears trees around x, z, so what stands there can be seen. */
static void clear_trees(int x, int z, int r) {
    for (int dx = -r; dx <= r; dx++)
        for (int dz = -r; dz <= r; dz++)
            for (int y = 1; y < WY; y++) {
                int cx = x + dx, cz = z + dz;
                if (cx < 0 || cz < 0 || cx >= WX || cz >= WZ) continue;
                if (world[cx][y][cz] == LEAVES || world[cx][y][cz] == WOOD) world[cx][y][cz] = AIR;
            }
}

static void find_portal(void) {
    int x, z;
    float a = portal_spot(&x, &z);
    int y = WY - 1;
    while (y > 0 && (world[x][y][z] == AIR || world[x][y][z] == LEAVES || world[x][y][z] == WOOD)) y--;
    portal_x = x + 0.5f;
    portal_y = y + 1.0f;
    portal_z = z + 0.5f;
    portal_yaw = a;
    portal_here = 1;
}

static int beacon_lit(int k) {
    return beacon_y[k] >= 0 && world[beacon_x[k]][beacon_y[k]][beacon_z[k]] == BEACON_ON;
}

static int beacons_present(void) {
    int n = 0;
    for (int k = 0; k < BEACONS; k++) n += beacon_y[k] >= 0;
    return n;
}

static int beacons_lit(void) {
    int n = 0;
    for (int k = 0; k < BEACONS; k++) n += beacon_lit(k);
    return n;
}

/* The unlit beacon nearest to x, z (or -1), and its distance in *d. */
static int nearest_unlit(float x, float z, float* d) {
    int best = -1;
    *d = 1e9f;
    for (int k = 0; k < BEACONS; k++) {
        if (beacon_y[k] < 0 || beacon_lit(k)) continue;
        float dx = beacon_x[k] + 0.5f - x, dz = beacon_z[k] + 0.5f - z, dist = sqrtf(dx * dx + dz * dz);
        if (dist < *d) { *d = dist; best = k; }
    }
    return best;
}

/* The lit beacon nearest to x, z (or -1), and its distance in *d. */
static int nearest_lit(float x, float z, float* d) {
    int best = -1;
    *d = 1e9f;
    for (int k = 0; k < BEACONS; k++) {
        if (!beacon_lit(k)) continue;
        float dx = beacon_x[k] + 0.5f - x, dz = beacon_z[k] + 0.5f - z, dist = sqrtf(dx * dx + dz * dz);
        if (dist < *d) { *d = dist; best = k; }
    }
    return best;
}

/* A lit beacon's sanctuary: 1 well inside BEACON_CALM, fading to 0 at its edge. */
static float sanctuary_at(float x, float z) {
    float d;
    if (away || nearest_lit(x, z, &d) < 0) return 0;
    float t = (d - (BEACON_CALM - 10)) / 10;
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    return 1 - t * t * (3 - 2 * t);
}

/* How thick the mist is at x, z: 1 near an unlit beacon, 0 away from them
 * and inside a lit beacon's sanctuary. */
static float mist_at(float x, float z) {
    if (away) return 1;
    float d;
    if (nearest_unlit(x, z, &d) < 0) return 0;
    float t = (d - MIST_NEAR) / (MIST_FAR - MIST_NEAR);
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    return (1 - t * t * (3 - 2 * t)) * (1 - sanctuary_at(x, z));
}

/* Whether a lit beacon keeps night creatures away from x, z. */
static int calm_at(float x, float z) {
    for (int k = 0; k < BEACONS; k++) {
        if (!beacon_lit(k)) continue;
        float dx = beacon_x[k] + 0.5f - x, dz = beacon_z[k] + 0.5f - z;
        if (dx * dx + dz * dz < BEACON_CALM * BEACON_CALM) return 1;
    }
    return 0;
}

/* ---- Ruins and notes ----------------------------------------------------- */

/* Villages wrecked by the night creatures stand between the start and the
 * beacons, where the seed puts them. Each house belonged to someone: some
 * left a note or a phone behind, some only broken furniture. Notes lie in the
 * open too. They are not blocks: they are drawn as small boxes where the
 * seed puts them (the lowest air in that column, so on the floor), and found
 * ones are kept in goal_flags. Their words are in the app (StoryNotes.kt):
 * the game only says which one was opened. Note NOTES - 1 lies on the other
 * side. */
#define NOTES 9
#define NOTE_AWAY (NOTES - 1)
#define HOUSES_PER_VILLAGE 4
enum { NOTE_PAPER, NOTE_PHONE };
static const int note_kind[NOTES] = {NOTE_PAPER, NOTE_PAPER, NOTE_PHONE, NOTE_PAPER, NOTE_PHONE,
                                     NOTE_PAPER, NOTE_PHONE, NOTE_PAPER, NOTE_PAPER};
static float note_x[NOTES], note_y[NOTES], note_z[NOTES], note_yaw[NOTES];
static int note_here[NOTES]; /* in the world being shown */

static int villages(void) { return CHUNKS >= 24 ? 4 : CHUNKS >= 16 ? 3 : 2; }

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static void village_center(int v, int* x, int* z) {
    float a = (v + 0.5f) * 6.2831853f / villages() + rand01(world_seed, 9, 41) * 6.2831853f, r = WX * 0.2f;
    *x = clampi((int)(WX / 2 + cosf(a) * r), 10, WX - 11);
    *z = clampi((int)(WZ / 2 + sinf(a) * r), 10, WZ - 11);
}

static void house_spot(int h, int* x, int* z) {
    static const int off[HOUSES_PER_VILLAGE][2] = {{-8, -7}, {7, -8}, {-7, 8}, {8, 7}};
    int cx, cz;
    village_center(h / HOUSES_PER_VILLAGE, &cx, &cz);
    int jx = (int)(rand01(world_seed + h, 11, 43) * 5) - 2, jz = (int)(rand01(world_seed + h, 13, 47) * 5) - 2;
    *x = clampi(cx + off[h % HOUSES_PER_VILLAGE][0] + jx, 6, WX - 7);
    *z = clampi(cz + off[h % HOUSES_PER_VILLAGE][1] + jz, 6, WZ - 7);
}

/* The lowest air in a column: the floor a note lies on. */
static int lowest_air(int x, int z) {
    for (int y = 0; y < WY; y++)
        if (world[x][y][z] == AIR) return y;
    return WY - 1;
}

/* The ground under trees: the highest block that is not air, leaves or wood. */
static int ground_top(int x, int z) {
    int y = WY - 1;
    while (y > 0 && (world[x][y][z] == AIR || world[x][y][z] == LEAVES || world[x][y][z] == WOOD)) y--;
    return y;
}

static int is_natural(int b) {
    return b == AIR || b == GRASS || b == DIRT || b == STONE || b == SAND || b == WOOD || b == LEAVES || b == WATER;
}

/* One ruined house: a floor, broken walls, what is left of the roof and of
 * the furniture, and rubble outside. Skipped on water, and where anything
 * not natural stands (an old world's buildings are never touched). */
static void build_house(int h) {
    int cx, cz;
    house_spot(h, &cx, &cz);
    int half = rand01(world_seed, h, 45) < 0.5f ? 2 : 3, g = ground_top(cx, cz);
    if (g <= WATER_LEVEL + 1 || g > WY - 9) return;
    for (int dx = -half - 1; dx <= half + 1; dx++)
        for (int dz = -half - 1; dz <= half + 1; dz++)
            for (int dy = -1; dy <= 6; dy++)
                if (!is_natural(world[cx + dx][g + dy][cz + dz])) return;
    static const int walls[4] = {PLASTER, BRICKS, PLANKS, STONE_BRICKS};
    int mat = walls[hash3(world_seed, h, 49) % 4];
    for (int dx = -half; dx <= half; dx++)
        for (int dz = -half; dz <= half; dz++) {
            int x = cx + dx, z = cz + dz;
            for (int dy = -3; dy <= -1; dy++)
                if (world[x][g + dy][z] == AIR || world[x][g + dy][z] == WATER) world[x][g + dy][z] = DIRT;
            world[x][g][z] = PLANKS;
            for (int dy = 1; dy <= 6; dy++) world[x][g + dy][z] = AIR;
            int edge = abs(dx) == half || abs(dz) == half, corner = abs(dx) == half && abs(dz) == half;
            if (edge)
                for (int dy = 1; dy <= 3; dy++) {
                    float r = rand01(x, g + dy, z + world_seed);
                    int b = r > 0.1f + 0.24f * (dy - 1) || (corner && dy < 3) ? mat : AIR;
                    if (dz == half && dx == 0 && dy <= 2) b = AIR; /* the doorway */
                    else if (dy == 2 && (dx == 0 || dz == 0) && !corner) b = r > 0.6f ? GLASS : AIR;
                    world[x][g + dy][z] = (uint8_t)b;
                }
            if (rand01(x, g + 4, z + world_seed) < 0.45f) world[x][g + 4][z] = ROOF;
        }
    /* Furniture, broken: a table on its side, an emptied shelf, a bed. */
    world[cx - half + 1][g + 1][cz - half + 1] = PLANKS;
    world[cx - half + 2][g + 1][cz - half + 1] = WOOD;
    world[cx + half - 1][g + 1][cz - half + 1] = PLANKS;
    if (rand01(cx, g, cz) < 0.5f) world[cx + half - 1][g + 2][cz - half + 1] = PLANKS;
    world[cx - half + 1][g + 1][cz + half - 1] = PLASTER;
    if (rand01(h, world_seed, 103) < 0.55f) world[cx + half - 1][g + 1][cz + half - 1] = STOVE; /* the old stove */
    /* Rubble outside. */
    for (int k = 0; k < 5; k++) {
        float a = rand01(h, k, 51 + world_seed) * 6.2831853f, d = half + 1.5f + 2 * rand01(h, k, 53);
        int x = clampi((int)(cx + cosf(a) * d), 1, WX - 2), z = clampi((int)(cz + sinf(a) * d), 1, WZ - 2);
        int y = lowest_air(x, z);
        if (y < WY - 1 && is_natural(world[x][y - 1][z]) && world[x][y - 1][z] != WATER) world[x][y][z] = (uint8_t)mat;
    }
}

static void place_ruins(void) {
    for (int h = 0; h < villages() * HOUSES_PER_VILLAGE; h++) build_house(h);
    /* Clearings around the rifts and the portal (they came with the ruins). */
    for (int k = 0; k < BEACONS; k++) {
        int x, z;
        beacon_spot(k, &x, &z);
        clear_trees(x + 2, z, 4);
    }
    int px, pz;
    portal_spot(&px, &pz);
    clear_trees(px, pz, 5);
}

/* Where the notes of this world lie: one near the start, five in houses
 * (others stay empty), one dropped in the open, one at a beacon. */
static void find_notes(float spawn_x, float spawn_z) {
    static const int in_house[5] = {0, 2, 3, 5, 6};
    for (int n = 0; n < NOTES; n++) {
        int x, z;
        if (n == 0) {
            x = clampi((int)spawn_x + 4, 1, WX - 2);
            z = clampi((int)spawn_z + 3, 1, WZ - 2);
        } else if (n <= 5) {
            house_spot(in_house[n - 1], &x, &z);
            x += 1;
        } else if (n == 6) {
            int vx, vz, bx, bz;
            village_center(0, &vx, &vz);
            beacon_spot(0, &bx, &bz);
            x = vx + (bx - vx) * 3 / 5;
            z = vz + (bz - vz) * 3 / 5;
        } else if (n == 7) {
            beacon_spot(1, &x, &z);
            x += 1;
        } else {
            note_here[n] = 0; /* on the other side */
            continue;
        }
        x = clampi(x, 1, WX - 2);
        z = clampi(z, 1, WZ - 2);
        if (n == 0 || n >= 6) { /* in the open: the nearest spot with only sky above, off the water */
            for (int r = 0, found = 0; r <= 5 && !found; r++)
                for (int dx = -r; dx <= r && !found; dx++)
                    for (int dz = -r; dz <= r && !found; dz++) {
                        int cx = clampi(x + dx, 2, WX - 3), cz = clampi(z + dz, 2, WZ - 3), y = lowest_air(cx, cz), open = 1;
                        for (int k = y; k < WY && open; k++) open = world[cx][k][cz] == AIR;
                        for (int k = 0; k < 9 && open; k++) /* and room around it */
                            for (int h = 0; h < 3 && open && y + h < WY; h++)
                                open = !is_solid(world[cx + k % 3 - 1][y + h][cz + k / 3 - 1]);
                        if (open && y > 0 && world[cx][y - 1][cz] != WATER) { x = cx; z = cz; found = 1; }
                    }
        }
        note_x[n] = x + 0.5f;
        note_z[n] = z + 0.5f;
        note_y[n] = (float)lowest_air(x, z);
        note_yaw[n] = rand01(world_seed, n, 55) * 6.2831853f;
        note_here[n] = 1;
    }
}

/* Where a ray from o along d meets the box lo-hi within max_t (in *t), or 0. */
static int ray_box(const float* o, const float* d, const float* lo, const float* hi, float max_t, float* t) {
    float t0 = 0, t1 = max_t;
    for (int k = 0; k < 3; k++) {
        if (fabsf(d[k]) < 1e-6f) {
            if (o[k] < lo[k] || o[k] > hi[k]) return 0;
            continue;
        }
        float a = (lo[k] - o[k]) / d[k], b = (hi[k] - o[k]) / d[k];
        if (a > b) { float tmp = a; a = b; b = tmp; }
        if (a > t0) t0 = a;
        if (b < t1) t1 = b;
        if (t0 > t1) return 0;
    }
    *t = t0;
    return 1;
}

/* The note under the crosshair within max_t (a generous box: they are small), or -1. */
static int note_on_ray(const float* o, const float* d, float max_t, float* t) {
    int best = -1;
    *t = max_t;
    for (int n = 0; n < NOTES; n++) {
        if (!note_here[n]) continue;
        float lo[3] = {note_x[n] - 0.35f, note_y[n], note_z[n] - 0.35f};
        float hi[3] = {note_x[n] + 0.35f, note_y[n] + 0.3f, note_z[n] + 0.35f}, tn;
        if (ray_box(o, d, lo, hi, *t, &tn)) { *t = tn; best = n; }
    }
    return best;
}

/* ---- Survival -------------------------------------------------------- */

/* Survival (blocks/docs/story.md, eighth round): tools make things possible,
 * workstations make crafting depend on the place, and the player gets
 * hungry and cold.
 *
 * Things lie on the ground: sticks (near trees) and flint, which move every
 * day, and the tools people left in the ruins (a knife and a saw), found
 * once. Like notes they are not blocks: small boxes picked up with the
 * break action. The first NEAR_START of each day lie near the start, so a
 * new player can make a knife and a pickaxe at once. */
#define DAILY_PICKUPS 64
#define FOUND_TOOLS 2
#define MAX_PICKUPS (DAILY_PICKUPS + FOUND_TOOLS)
typedef struct {
    float x, y, z, yaw;
    int item;  /* STICK, FLINT, KNIFE or SAW */
    int index; /* daily: bit in daily_taken; found tool: bit in tools_taken (with FOUND_BIT) */
} Pickup;
#define FOUND_BIT 0x100
#define NEAR_START 6 /* three sticks and three flints: enough for a knife and a pickaxe */
static Pickup pickups[MAX_PICKUPS];
static int pickup_count;
static int pickups_day = -1; /* the day the pickups were placed for (-1: place them again) */

static int pickup_taken(const Pickup* k) {
    return k->index & FOUND_BIT ? (tools_taken >> (k->index & 0xff)) & 1 : (daily_taken[k->index / 32] >> (k->index % 32)) & 1;
}

static void take_pickup(const Pickup* k) {
    if (k->index & FOUND_BIT) tools_taken |= 1u << (k->index & 0xff);
    else daily_taken[k->index / 32] |= 1u << (k->index % 32);
}

/* The top of the ground in a column (the highest solid block), or -1 when
 * nothing can lie there (water, leaves). */
static int pickup_ground(int x, int z) {
    int y = WY - 2;
    while (y > 0 && !is_solid(world[x][y][z])) y--;
    int b = world[x][y][z];
    return b == LEAVES || b == WOOD || world[x][y + 1][z] != AIR ? -1 : y;
}

static int tree_near(int x, int y, int z) {
    for (int dx = -3; dx <= 3; dx++)
        for (int dz = -3; dz <= 3; dz++)
            if (block_at(x + dx, y + 1, z + dz) == WOOD) return 1;
    return 0;
}

/* Places the day's sticks and flint, and the found tools, from the seed. */
static void place_pickups(float spawn_x, float spawn_z) {
    pickup_count = 0;
    if (day_count != daily_day) { /* a new day: everything lies somewhere else again */
        daily_day = day_count;
        daily_taken[0] = daily_taken[1] = 0;
    }
    int daily = world_chunks * world_chunks / 2;
    daily = daily < 24 ? 24 : daily > DAILY_PICKUPS ? DAILY_PICKUPS : daily;
    int salt = world_seed + (int)day_count * 7919;
    for (int i = 0; i < daily; i++) {
        int item = i < NEAR_START ? (i < NEAR_START / 2 ? STICK : FLINT) : rand01(salt, i, 81) < 0.62f ? STICK : FLINT;
        for (int t = 0; t < 16; t++) {
            int x, z;
            if (i < NEAR_START) { /* near the start */
                x = clampi((int)spawn_x - 6 + (int)(rand01(salt, i * 16 + t, 83) * 13), 1, WX - 2);
                z = clampi((int)spawn_z - 6 + (int)(rand01(salt, i * 16 + t, 85) * 13), 1, WZ - 2);
            } else {
                x = 1 + (int)(rand01(salt, i * 16 + t, 87) * (WX - 2));
                z = 1 + (int)(rand01(salt, i * 16 + t, 89) * (WZ - 2));
            }
            int y = pickup_ground(x, z);
            if (y <= WATER_LEVEL - 1) continue;
            if (item == STICK && i >= NEAR_START && t < 10 && !tree_near(x, y, z)) continue; /* sticks fall from trees */
            pickups[pickup_count++] = (Pickup){x + 0.3f + 0.4f * rand01(salt, i, 91), y + 1.0f,
                                               z + 0.3f + 0.4f * rand01(salt, i, 93),
                                               rand01(salt, i, 95) * 6.2831853f, item, i};
            break;
        }
    }
    /* The found tools: a knife and a saw, each on a house's floor. */
    int houses = villages() * HOUSES_PER_VILLAGE;
    int knife_house = hash3(world_seed, 97, 3) % houses;
    int saw_house = (knife_house + 1 + hash3(world_seed, 99, 5) % (houses - 1)) % houses;
    for (int k = 0; k < FOUND_TOOLS; k++) {
        int x, z;
        house_spot(k ? saw_house : knife_house, &x, &z);
        x = clampi(x - 1, 1, WX - 2);
        int y = lowest_air(x, z);
        pickups[pickup_count++] = (Pickup){x + 0.5f, (float)y, z + 0.4f, rand01(world_seed, k, 101) * 6.2831853f,
                                           k ? SAW : KNIFE, FOUND_BIT | k};
    }
    pickups_day = (int)day_count;
    printf("blocks: %d things on the ground on day %u\n", pickup_count, day_count + 1);
}

/* The pickup a ray from o along d meets first within max_t (in *t), or -1. */
static int pickup_on_ray(const float* o, const float* d, float max_t, float* t) {
    int best = -1;
    *t = max_t;
    for (int i = 0; i < pickup_count; i++) {
        const Pickup* k = &pickups[i];
        if (pickup_taken(k)) continue;
        float lo[3] = {k->x - 0.3f, k->y, k->z - 0.3f}, hi[3] = {k->x + 0.3f, k->y + 0.2f, k->z + 0.3f}, tn;
        if (ray_box(o, d, lo, hi, *t, &tn)) { *t = tn; best = i; }
    }
    return best;
}

/* Blows it takes to break a block with what the player carries; 0: it
 * cannot be broken without a tool (the one in *tool). Stone and anything
 * built of it needs a pickaxe; an axe fells wood with one blow. Shade stone
 * crumbles by hand, slowly: on the other side a shelter must be possible
 * with bare hands. */
static int blows_for(int b, int* tool) {
    *tool = AIR;
    if (creative) return 1;
    switch (b) {
    case STONE: case STONE_BRICKS: case BRICKS: case PLASTER: case ROOF: case KILN: case STOVE:
        *tool = PICK;
        return inventory[PICK] ? 2 : 0;
    case SHADE: return inventory[PICK] ? 2 : 6;
    case WOOD: return inventory[AXE] ? 1 : 5;
    case PLANKS: return inventory[AXE] ? 1 : 3;
    default: return 1;
    }
}

/* What else breaking a block gives, now and then: leaves drop sticks and
 * berries, and stone broken with a pickaxe chips off flint. */
static void extra_drops(int block, int x, int y, int z) {
    float r = rand01(x + world_seed, y + (int)day_count * 31, z);
    if (block == LEAVES) {
        if (r < 0.35f) give(STICK, 1);
        else if (r < 0.52f) give(BERRIES, 1);
    } else if (block == STONE && r < 0.25f) {
        give(FLINT, 1);
    }
}

/* How much eating something fills the player (0: not food). Cooked food
 * fills more, and meat fried in fat the most. */
static float food_value(int item) {
    switch (item) {
    case BERRIES: return 1.0f;
    case RAW_MEAT: return 1.5f;
    case COOKED_MEAT: return 4.0f;
    case FRIED_MEAT: return 6.0f;
    default: return 0;
    }
}

/* ---- Shelters ---------------------------------------------------------- */

/* A shelter is a closed room: the mist does not come in and night creatures
 * can neither find nor reach the player there (a lamp inside is only for
 * seeing). The air around the player is filled outwards, through anything
 * that is not a wall (leaves and water let the mist through); if it runs out
 * within SHELTER_CELLS without leaving the box around the player, the room
 * is closed. */
#define SHELTER_CELLS 600
#define SHELTER_R 12
#define SHELTER_SIDE (SHELTER_R * 2 + 1)

static int is_wall(int b) { return is_solid(b) && b != LEAVES; }

static int shelter_at(float px, float py, float pz) {
    static uint16_t seen[SHELTER_SIDE][SHELTER_SIDE][SHELTER_SIDE];
    static uint16_t mark;
    static int16_t queue[SHELTER_CELLS + 8][3];
    if (++mark == 0) { memset(seen, 0, sizeof seen); mark = 1; }
    int ox = (int)floorf(px), oy = (int)floorf(py + 0.2f), oz = (int)floorf(pz);
    int head = 0, tail = 0;
    for (int dy = 0; dy <= 1; dy++) {
        if (is_wall(block_at(ox, oy + dy, oz))) continue;
        seen[SHELTER_R][SHELTER_R + dy][SHELTER_R] = mark;
        queue[tail][0] = 0; queue[tail][1] = (int16_t)dy; queue[tail][2] = 0;
        tail++;
    }
    while (head < tail) {
        int cx = queue[head][0], cy = queue[head][1], cz = queue[head][2];
        head++;
        for (int f = 0; f < 6; f++) {
            int nx = cx + face_normal[f][0], ny = cy + face_normal[f][1], nz = cz + face_normal[f][2];
            int wx = ox + nx, wy = oy + ny, wz = oz + nz;
            if (abs(nx) > SHELTER_R - 1 || abs(ny) > SHELTER_R - 1 || abs(nz) > SHELTER_R - 1 || wy >= WY) return 0;
            uint16_t* s = &seen[nx + SHELTER_R][ny + SHELTER_R][nz + SHELTER_R];
            if (*s == mark) continue;
            *s = mark;
            int b = block_at(wx, wy, wz);
            if (is_wall(b)) continue;
            if (tail >= SHELTER_CELLS) return 0; /* too big to be a room */
            queue[tail][0] = (int16_t)nx; queue[tail][1] = (int16_t)ny; queue[tail][2] = (int16_t)nz;
            tail++;
        }
    }
    return tail > 0;
}

/* ---- Creatures and particles ----------------------------------------- */

/* Two kinds of creatures. Peaceful ones wander (heavy mode fills the world
 * with them) and run away when touched or hit. Night ones (the story calls
 * them Nocturnes; people in it call them many things) come out in the dark
 * and the mist, away from lamps. Once one finds the player it does not stop:
 * it follows, grabs and drags them into the mist (see the grab in main). A
 * blow only dazes it; they cannot be killed. They also take any peaceful
 * creature they find. They vanish at daybreak (outside the mist), near a
 * lamp (outside the mist) and near a lit beacon. All creatures think and move
 * on the CPU and are drawn together with one instanced draw call. */
enum { KIND_PEACEFUL, KIND_NIGHT };

typedef struct {
    float x, y, z, vy, yaw, target_yaw, speed, timer, phase;
    int on_ground, color, kind, alive;
    float hp, flee, cooldown, check; /* unused; seconds to keep fleeing, until the next grab, the next lamp check */
    float stun;                      /* seconds it stays dazed after a blow */
    int hunting;                     /* it found the player and follows them */
    int prey;                        /* 1 + the peaceful creature it goes after, or 0 */
} Creature;

#define MAX_CREATURES 300
#define MAX_NIGHT 16
static Creature creatures[MAX_CREATURES];
static int creature_count;
static Creature night[MAX_NIGHT];
static int night_limit = 10; /* at once; 0 when nothing may attack */

static const float creature_colors[6][3] = {
    {0.86f, 0.62f, 0.45f}, {0.55f, 0.72f, 0.88f}, {0.92f, 0.85f, 0.55f},
    {0.70f, 0.55f, 0.85f}, {0.60f, 0.82f, 0.60f}, {0.90f, 0.60f, 0.70f},
};
static const float night_color[3] = {0.50f, 0.40f, 0.72f}; /* dim purple at night, lit only by the moon */

/* The grab: which night creature holds the player (-1: none), for how long,
 * how far they have got free (0-1), and seconds of safety after getting free. */
static int grab_by = -1;
static float grab_time, grab_escape, grab_grace, drag_yaw;
#define GRAB_SECONDS 9.0f  /* held this long, the player is taken */
#define CHASE_SPEED 3.6f   /* the player walks at 4.3 */
#define NIGHT_SIGHT 22.0f  /* how far they notice the player */
#define NIGHT_LOSE 48.0f   /* how far the player must get to lose them */

static void spawn_puff(float x, float y, float z, const float* color, int count, uint32_t tick);

/* Top of the highest solid block at or below height y in column x, z. */
static float ground_below(float x, float y, float z) {
    int bx = (int)floorf(x), bz = (int)floorf(z);
    for (int by = (int)floorf(y + 0.5f); by >= 0; by--)
        if (is_solid(block_at(bx, by, bz))) return by + 1.0f;
    return 0;
}

/* Whether a lamp is within r blocks (a box) of x, y, z. Cheap away from lamps. */
static int lamp_near(float x, float y, float z, int r) {
    int bx = (int)floorf(x), by = (int)floorf(y), bz = (int)floorf(z), any = 0;
    for (int i = (bx - r) / CHUNK; i <= (bx + r) / CHUNK && !any; i++)
        for (int j = (bz - r) / CHUNK; j <= (bz + r) / CHUNK && !any; j++)
            if (i >= 0 && j >= 0 && i < CHUNKS && j < CHUNKS && chunk_lamps[i][j]) any = 1;
    if (!any) return 0;
    for (int dx = -r; dx <= r; dx++)
        for (int dy = -r / 2; dy <= r / 2; dy++)
            for (int dz = -r; dz <= r; dz++)
                if (gives_light(block_at(bx + dx, by + dy, bz + dz))) return 1;
    return 0;
}

static void spawn_creatures(int count) {
    creature_count = 0;
    for (int tries = 0; creature_count < count && tries < count * 50; tries++) {
        int x = 2 + (int)(rand01(tries, 11, 3) * (WX - 4)), z = 2 + (int)(rand01(tries, 13, 5) * (WZ - 4));
        int y = WY - 1;
        while (y > 0 && world[x][y][z] == AIR) y--;
        if (world[x][y][z] != GRASS) continue;
        Creature* c = &creatures[creature_count++];
        memset(c, 0, sizeof(*c));
        c->x = x + 0.5f;
        c->z = z + 0.5f;
        c->y = y + 1.0f;
        c->yaw = c->target_yaw = rand01(tries, 17, 7) * 6.2832f;
        c->color = creature_count % 6;
        c->timer = rand01(tries, 19, 9) * 3;
        c->kind = KIND_PEACEFUL;
        c->alive = 1;
    }
}

/* Peaceful creatures taken in the night come back by day, far from the player. */
static void respawn_creatures(float dt, int is_night, float px, float pz, uint32_t tick) {
    static float timer = 15;
    if (is_night || (timer -= dt) > 0) return;
    timer = 15;
    for (int i = 0; i < creature_count; i++) {
        Creature* c = &creatures[i];
        if (c->alive) continue;
        int x = 2 + (int)(rand01((int)tick, i, 61) * (WX - 4)), z = 2 + (int)(rand01((int)tick, i, 63) * (WZ - 4));
        float dx = x - px, dz = z - pz;
        if (dx * dx + dz * dz < 24 * 24) return;
        int y = WY - 1;
        while (y > 0 && world[x][y][z] == AIR) y--;
        if (world[x][y][z] != GRASS) return;
        int color = c->color;
        memset(c, 0, sizeof *c);
        c->x = x + 0.5f;
        c->y = y + 1.0f;
        c->z = z + 0.5f;
        c->color = color;
        c->kind = KIND_PEACEFUL;
        c->alive = 1;
        return;
    }
}

static float wrap_angle(float a) {
    while (a > 3.14159265f) a -= 6.2831853f;
    while (a < -3.14159265f) a += 6.2831853f;
    return a;
}

/* Turns towards target_yaw and moves at speed: hops up single blocks and
 * turns back from water and the world's edge. Then falls onto the ground. */
static void step_creature(Creature* c, float dt) {
    c->yaw += wrap_angle(c->target_yaw - c->yaw) * (dt * 4 < 1 ? dt * 4 : 1);
    float nx = c->x + sinf(c->yaw) * c->speed * dt, nz = c->z - cosf(c->yaw) * c->speed * dt;
    int feet = (int)floorf(c->y + 0.1f);
    int ahead = block_at((int)floorf(nx), feet, (int)floorf(nz));
    if (nx < 1 || nz < 1 || nx > WX - 1 || nz > WZ - 1 || ahead == WATER ||
        block_at((int)floorf(nx), feet - 1, (int)floorf(nz)) == WATER) {
        c->target_yaw = c->yaw + 3.1416f; /* turn back from edges and water */
        c->timer = 1.5f;
    } else if (is_solid(ahead)) {
        int tall = c->kind == KIND_NIGHT ? 2 : 1; /* room above the step for its height */
        int room = 1;
        for (int k = 1; k <= tall; k++) room &= !is_solid(block_at((int)floorf(nx), feet + k, (int)floorf(nz)));
        if (c->on_ground && room) {
            c->vy = 6.5f; /* hop up one block */
            c->on_ground = 0;
        } else {
            c->target_yaw = c->yaw + 1.6f;
        }
    } else {
        c->x = nx;
        c->z = nz;
    }
    c->vy -= 20.0f * dt;
    c->y += c->vy * dt;
    float ground = ground_below(c->x, c->y, c->z);
    if (c->y <= ground) {
        c->y = ground;
        c->vy = 0;
        c->on_ground = 1;
    } else {
        c->on_ground = 0;
    }
    if (c->speed > 0) c->phase += dt * c->speed * 8;
}

/* Makes c run away from x, z for a while. */
static void scare(Creature* c, float x, float z, float seconds) {
    c->target_yaw = atan2f(c->x - x, -(c->z - z));
    c->flee = seconds;
    c->speed = c->kind == KIND_NIGHT ? 3.0f : 3.4f;
}

static void update_creatures(float dt, uint32_t tick, float px, float pz) {
    for (int i = 0; i < creature_count; i++) {
        Creature* c = &creatures[i];
        if (!c->alive) continue;
        float dx = c->x - px, dz = c->z - pz;
        /* Bumping into one sends it running. */
        if (c->flee <= 0 && dx * dx + dz * dz < 0.8f) scare(c, px, pz, 2.0f);
        if (c->flee > 0) {
            c->flee -= dt;
            if (c->flee <= 0) c->timer = 0;
        } else {
            c->timer -= dt;
            if (c->timer <= 0) { /* wander somewhere else, or rest */
                float r = rand01(i, (int)tick, 23);
                c->target_yaw = c->yaw + (r - 0.5f) * 4.0f;
                c->speed = r < 0.25f ? 0 : 0.6f + 1.2f * rand01(i, (int)tick, 29);
                c->timer = 1.0f + 3.0f * rand01(i, (int)tick, 31);
            }
        }
        step_creature(c, dt);
    }
}

/* Night creatures: come out in the dark and the mist, hunt the player and
 * grab them (grab_by). is_night: the sun is down; sheltered: the player is in
 * a closed, lit room, where they cannot be found or reached. Returns 1 when
 * one grabs the player this frame. */
static int update_night(float dt, uint32_t tick, int is_night, float px, float py, float pz, int sheltered) {
    static float spawn_timer;
    int grabbed = 0, alive = 0;
    for (int i = 0; i < MAX_NIGHT; i++) alive += night[i].alive;
    /* A new one now and then, out of sight range, never near a lamp. */
    spawn_timer -= dt;
    /* In the dark, or in a beacon's mist even by day. */
    int dark_here = is_night || mist_at(px, pz) > 0.5f;
    /* On the other side there are more of them, and they come sooner. */
    int limit = away && night_limit ? MAX_NIGHT * 3 / 4 : night_limit;
    float chase = away ? 4.9f : CHASE_SPEED, sight = away ? 14.0f : NIGHT_SIGHT;
    if (dark_here && alive < limit && spawn_timer <= 0) {
        spawn_timer = away ? 1.5f : 3.0f;
        for (int k = 0; k < MAX_NIGHT; k++) {
            if (night[k].alive) continue;
            float a = rand01((int)tick, k, 51) * 6.2832f, d = 18 + 12 * rand01((int)tick, k, 53);
            int x = (int)floorf(px + sinf(a) * d), z = (int)floorf(pz - cosf(a) * d);
            if (x < 2 || z < 2 || x >= WX - 2 || z >= WZ - 2) break;
            int y = WY - 3;
            while (y > 0 && !is_solid(world[x][y][z])) y--;
            if (world[x][y][z] == WATER || world[x][y][z] == LEAVES || is_solid(world[x][y + 1][z]) ||
                is_solid(world[x][y + 2][z]) || calm_at(x + 0.5f, z + 0.5f) ||
                (mist_at(x + 0.5f, z + 0.5f) < 0.5f && lamp_near(x + 0.5f, y + 1.0f, z + 0.5f, 9)))
                break;
            Creature* c = &night[k];
            memset(c, 0, sizeof *c);
            c->x = x + 0.5f;
            c->y = y + 1.0f;
            c->z = z + 0.5f;
            c->kind = KIND_NIGHT;
            c->alive = 1;
            c->yaw = c->target_yaw = a + 3.1416f;
            c->check = rand01(k, (int)tick, 57);
            break;
        }
    }
    for (int i = 0; i < MAX_NIGHT; i++) {
        Creature* c = &night[i];
        if (!c->alive) continue;
        /* Daylight (outside the mist), lamps (outside the mist) and lit
         * beacons drive them away: they fade into smoke. In the mist a lamp
         * is not enough, which is what shelters are for. */
        c->check -= dt;
        int fade = 0, look = 0;
        if (c->check <= 0) {
            c->check = 0.5f;
            look = 1;
            float m = mist_at(c->x, c->z);
            fade = (!is_night && m < 0.3f) || calm_at(c->x, c->z) || (m < 0.5f && lamp_near(c->x, c->y + 1, c->z, 6));
        }
        if (fade) {
            spawn_puff(c->x, c->y + 0.9f, c->z, night_color, 12, tick + i);
            c->alive = 0;
            if (grab_by == i) grab_by = -1; /* the player drops free */
            continue;
        }
        if (grab_by == i) { /* dragging the player into the mist */
            float ox = c->x, oz = c->z;
            c->target_yaw = drag_yaw;
            c->speed = 2.3f;
            c->phase += dt * 6;
            step_creature(c, dt);
            /* Blocked: it feels its way round. */
            if ((c->x - ox) * (c->x - ox) + (c->z - oz) * (c->z - oz) < 0.25f * c->speed * c->speed * dt * dt) drag_yaw += 2.0f * dt;
            continue;
        }
        float dx = px - c->x, dz = pz - c->z, d = sqrtf(dx * dx + dz * dz);
        if (sheltered || d > NIGHT_LOSE) c->hunting = 0;
        else if (d < sight) c->hunting = 1;
        if (c->stun > 0) c->stun -= dt;
        if (c->flee > 0) {
            c->flee -= dt;
        } else if (c->hunting) { /* it does not stop */
            c->target_yaw = atan2f(dx, -dz);
            c->speed = d < 0.9f ? 0 : chase;
            c->prey = 0;
        } else {
            /* Any living thing will do: the nearest peaceful creature. */
            if (look && !c->prey) {
                float best = 12 * 12;
                for (int k = 0; k < creature_count; k++) {
                    const Creature* q = &creatures[k];
                    float qx = q->x - c->x, qz = q->z - c->z;
                    if (q->alive && qx * qx + qz * qz < best) { best = qx * qx + qz * qz; c->prey = k + 1; }
                }
            }
            Creature* q = c->prey ? &creatures[c->prey - 1] : NULL;
            if (q && !q->alive) q = NULL, c->prey = 0;
            if (q) {
                float qx = q->x - c->x, qz = q->z - c->z;
                c->target_yaw = atan2f(qx, -qz);
                c->speed = 2.6f;
                if (qx * qx + qz * qz < 1.0f) { /* taken into the mist, both gone */
                    static const float fur[3] = {0.8f, 0.7f, 0.6f};
                    spawn_puff(q->x, q->y + 0.5f, q->z, fur, 6, tick + i);
                    spawn_puff(c->x, c->y + 0.9f, c->z, night_color, 12, tick + i + 3);
                    q->alive = 0;
                    c->alive = 0;
                    continue;
                }
            } else {
                c->timer -= dt;
                if (c->timer <= 0) {
                    /* Wander; on the other side they search, drifting towards the player. */
                    float turn = (rand01(i, (int)tick, 59) - 0.5f) * 3;
                    c->target_yaw = away ? atan2f(dx, -dz) + turn * 0.6f : c->yaw + turn;
                    c->speed = away ? 1.8f : 1.0f;
                    c->timer = 2;
                }
            }
        }
        /* A lit beacon's sanctuary: they stop at its edge and watch. */
        if (c->speed > 0 && !calm_at(c->x, c->z) &&
            calm_at(c->x + sinf(c->yaw) * 0.8f, c->z - cosf(c->yaw) * 0.8f)) {
            c->speed = 0;
            c->target_yaw = atan2f(dx, -dz);
            c->prey = 0;
        }
        c->cooldown -= dt;
        if (c->hunting && d < 1.25f && fabsf(py - c->y) < 1.6f && c->stun <= 0 && c->cooldown <= 0 && grab_by < 0 &&
            grab_grace <= 0 && !sheltered) {
            grab_by = i;
            grab_time = grab_escape = 0;
            /* Off towards the nearest mist, or simply away the way it came. */
            float bd;
            int k = nearest_unlit(c->x, c->z, &bd);
            drag_yaw = k >= 0 ? atan2f(beacon_x[k] + 0.5f - c->x, -(beacon_z[k] + 0.5f - c->z)) : c->yaw + 3.1416f;
            grabbed = 1;
        }
        step_creature(c, dt);
    }
    return grabbed;
}

/* The nearest creature along a ray within max_t, with the distance in *t:
 * which = index, *is_night tells the array. -1 when none. */
static int creature_on_ray(float ox, float oy, float oz, float dx, float dy, float dz, float max_t, float* t,
                           int* is_night) {
    int best = -1;
    *t = max_t;
    for (int pass = 0; pass < 2; pass++) {
        int n = pass ? MAX_NIGHT : creature_count;
        for (int i = 0; i < n; i++) {
            const Creature* c = pass ? &night[i] : &creatures[i];
            if (!c->alive) continue;
            float half_w = pass ? 0.4f : 0.45f, low = c->y, high = c->y + (pass ? 1.75f : 0.9f);
            float lo[3] = {c->x - half_w, low, c->z - half_w}, hi[3] = {c->x + half_w, high, c->z + half_w};
            float o[3] = {ox, oy, oz}, dir[3] = {dx, dy, dz}, t0 = 0, t1 = *t;
            int miss = 0;
            for (int k = 0; k < 3 && !miss; k++) {
                if (fabsf(dir[k]) < 1e-6f) {
                    if (o[k] < lo[k] || o[k] > hi[k]) miss = 1;
                    continue;
                }
                float a = (lo[k] - o[k]) / dir[k], b = (hi[k] - o[k]) / dir[k];
                if (a > b) { float tmp = a; a = b; b = tmp; }
                if (a > t0) t0 = a;
                if (b < t1) t1 = b;
                if (t0 > t1) miss = 1;
            }
            if (!miss) {
                *t = t0;
                best = i;
                *is_night = pass;
            }
        }
    }
    return best;
}

typedef struct {
    float x, y, z, vx, vy, vz, life;
    float color[3];
    float gravity; /* downward pull; smoke has none and drifts up */
    float size, grow; /* half size, and how fast it grows (smoke spreads) */
} Particle;

#define MAX_PARTICLES 256
static Particle particles[MAX_PARTICLES];

static void block_color(int block, float* out) {
    static const float colors[BLOCK_TYPES][3] = {
        {0, 0, 0}, {0.32f, 0.58f, 0.24f}, {0.52f, 0.37f, 0.24f}, {0.52f, 0.52f, 0.52f}, {0.86f, 0.80f, 0.58f},
        {0.42f, 0.30f, 0.18f}, {0.20f, 0.45f, 0.18f}, {0.16f, 0.32f, 0.68f}, {0.70f, 0.54f, 0.32f},
        {0.55f, 0.55f, 0.55f}, {0.62f, 0.32f, 0.25f}, {0.82f, 0.90f, 0.95f}, {0.64f, 0.29f, 0.19f},
        {0.86f, 0.84f, 0.78f}, {1.00f, 0.84f, 0.50f}, {0.36f, 0.38f, 0.46f}, {0.75f, 0.97f, 1.00f},
        {0.17f, 0.13f, 0.23f}, {0.56f, 0.47f, 0.40f}, {0.24f, 0.24f, 0.26f}, {1.00f, 0.55f, 0.15f},
    };
    memcpy(out, colors[block < BLOCK_TYPES ? block : PLANKS], sizeof(colors[0]));
}

/* Bits of a broken block, flying out and falling. */
static void spawn_particles(int bx, int by, int bz, int block, uint32_t tick) {
    int made = 0;
    for (int i = 0; i < MAX_PARTICLES && made < 20; i++) {
        Particle* p = &particles[i];
        if (p->life > 0) continue;
        float r1 = rand01(i, (int)tick, 41), r2 = rand01(i, (int)tick, 43), r3 = rand01(i, (int)tick, 47);
        p->x = bx + 0.2f + 0.6f * r1;
        p->y = by + 0.2f + 0.6f * r2;
        p->z = bz + 0.2f + 0.6f * r3;
        p->vx = (r1 - 0.5f) * 4;
        p->vy = 2 + 3 * r2;
        p->vz = (r3 - 0.5f) * 4;
        p->life = 0.6f + 0.6f * r2;
        p->gravity = 18;
        p->size = 0.06f;
        p->grow = 0;
        block_color(block, p->color);
        float shade = 0.75f + 0.4f * r3;
        for (int k = 0; k < 3; k++) p->color[k] *= shade;
        made++;
    }
}

/* A puff of bits in one colour: a creature hit, or one fading away. */
static void spawn_puff(float x, float y, float z, const float* color, int count, uint32_t tick) {
    int made = 0;
    for (int i = 0; i < MAX_PARTICLES && made < count; i++) {
        Particle* p = &particles[i];
        if (p->life > 0) continue;
        float r1 = rand01(i, (int)tick, 61), r2 = rand01(i, (int)tick, 67), r3 = rand01(i, (int)tick, 71);
        p->x = x + (r1 - 0.5f) * 0.6f;
        p->y = y + (r2 - 0.5f) * 0.8f;
        p->z = z + (r3 - 0.5f) * 0.6f;
        p->vx = (r1 - 0.5f) * 3;
        p->vy = 1 + 2 * r2;
        p->vz = (r3 - 0.5f) * 3;
        p->life = 0.4f + 0.5f * r3;
        p->gravity = 18;
        p->size = 0.06f;
        p->grow = 0;
        for (int k = 0; k < 3; k++) p->color[k] = color[k] * (0.8f + 0.4f * r2);
        made++;
    }
}

static void update_particles(float dt) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle* p = &particles[i];
        if (p->life <= 0) continue;
        p->life -= dt;
        p->vy -= p->gravity * dt;
        p->size += p->grow * dt;
        float nx = p->x + p->vx * dt, ny = p->y + p->vy * dt, nz = p->z + p->vz * dt;
        int b = block_at((int)floorf(nx), (int)floorf(ny), (int)floorf(nz));
        if (is_solid(b) && b != CAMPFIRE) { /* smoke and sparks leave through the fire */
            p->vx *= 0.4f;
            p->vz *= 0.4f;
            p->vy = 0;
        } else {
            p->x = nx;
            p->y = ny;
            p->z = nz;
        }
    }
}

/* Smoke from a campfire: grey puffs that rise slowly, sway and spread, and
 * now and then a spark, glowing (see BOX_GLOW), that flies up and falls. */
static void spawn_smoke(float x, float y, float z, int spark, uint32_t tick) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle* p = &particles[i];
        if (p->life > 0) continue;
        float r1 = rand01(i, (int)tick, 83), r2 = rand01(i, (int)tick, 89), r3 = rand01(i, (int)tick, 97);
        p->x = x + (r1 - 0.5f) * 0.25f;
        p->y = y;
        p->z = z + (r3 - 0.5f) * 0.25f;
        if (spark) {
            p->vx = (r1 - 0.5f) * 1.2f;
            p->vy = 2.5f + 1.5f * r2;
            p->vz = (r3 - 0.5f) * 1.2f;
            p->life = 0.5f + 0.4f * r2;
            p->gravity = 4;
            p->size = 0.025f;
            p->grow = 0;
            p->color[0] = 3.0f; p->color[1] = 2.55f + 0.3f * r2; p->color[2] = 2.12f;
        } else {
            p->vx = (r1 - 0.5f) * 0.3f;
            p->vy = 0.7f + 0.5f * r2;
            p->vz = (r3 - 0.5f) * 0.3f;
            p->life = 1.5f + 0.9f * r2;
            p->gravity = 0;
            p->size = 0.035f + 0.02f * r3;
            p->grow = 0.035f;
            float grey = 0.50f + 0.15f * r2;
            p->color[0] = grey; p->color[1] = grey; p->color[2] = grey * 1.04f;
        }
        return;
    }
}

/* Boxes for creatures and particles. The corners come from gl_VertexID; the
 * center, half size, rotation and colour come from uniforms (one draw call
 * per box) or from per-instance attributes (one draw call for all). */
#define BOX_BODY \
    "out float light;\n" \
    "out float dist;\n" \
    "const vec3 normals[6] = vec3[](vec3(1,0,0), vec3(-1,0,0), vec3(0,1,0), vec3(0,-1,0), vec3(0,0,1), vec3(0,0,-1));\n" \
    "const vec2 quad[6] = vec2[](vec2(0,0), vec2(1,0), vec2(1,1), vec2(0,0), vec2(1,1), vec2(0,1));\n" \
    "void main() {\n" \
    "    int face = gl_VertexID / 6;\n" \
    "    vec2 q = quad[gl_VertexID % 6];\n" \
    "    vec3 n = normals[face];\n" \
    "    vec3 t = abs(n.y) > 0.5 ? vec3(1,0,0) : vec3(0,1,0);\n" \
    "    vec3 b = cross(n, t);\n" \
    "    vec3 p = (n + (q.x * 2.0 - 1.0) * t + (q.y * 2.0 - 1.0) * b) * half_size;\n" \
    "    float c = cos(yaw), s = sin(yaw);\n" \
    "    p = vec3(c * p.x - s * p.z, p.y, s * p.x + c * p.z);\n" \
    "    vec3 rn = vec3(c * n.x - s * n.z, n.y, s * n.x + c * n.z);\n" \
    "    vec3 world = center + p;\n" \
    "    gl_Position = view_proj * vec4(world, 1.0);\n" \
    "    light = 0.45 + 0.55 * max(dot(rn, normalize(vec3(0.4, 0.8, 0.3))), 0.0);\n" \
    "    dist = length(world - eye);\n" \
    "    BOX_COLOR\n" \
    "}\n"

static const char* box_vs =
    "#version 330 core\n"
    "#define BOX_COLOR\n"
    "uniform mat4 view_proj;\n"
    "uniform vec3 center;\n"
    "uniform vec3 half_size;\n"
    "uniform float yaw;\n"
    "uniform vec3 eye;\n"
    BOX_BODY;

static const char* box_fs =
    "#version 330 core\n"
    "uniform vec3 color;\n"
    "uniform vec3 fog_color;\n"
    "uniform float fog_end;\n"
    "uniform vec3 ambient;\n"
    "in float light;\n"
    "in float dist;\n"
    "out vec4 frag;\n"
    "void main() {\n"
    "    float fog = clamp((dist - fog_end * 0.6) / (fog_end * 0.4), 0.0, 1.0);\n"
    "    frag = vec4(mix(color.r > 1.5 ? color - 2.0 : color * light * ambient, fog_color, fog), 1.0);\n"
    "}\n";

/* Per instance: center (3 floats), half size (3), yaw (1), colour (3).
 * BOX_GLOW: a colour whose red is above 1.5 is that colour minus 2, shown
 * unlit (the night creatures' eyes). */
#define INSTANCE_FLOATS 10
static const char* instanced_vs =
    "#version 330 core\n"
    "#define BOX_COLOR v_color = color;\n"
    "uniform mat4 view_proj;\n"
    "uniform vec3 eye;\n"
    "layout(location = 0) in vec3 center;\n"
    "layout(location = 1) in vec3 half_size;\n"
    "layout(location = 2) in float yaw;\n"
    "layout(location = 3) in vec3 color;\n"
    "out vec3 v_color;\n"
    BOX_BODY;

static const char* instanced_fs =
    "#version 330 core\n"
    "uniform vec3 fog_color;\n"
    "uniform float fog_end;\n"
    "uniform vec3 ambient;\n"
    "in vec3 v_color;\n"
    "in float light;\n"
    "in float dist;\n"
    "out vec4 frag;\n"
    "void main() {\n"
    "    float fog = clamp((dist - fog_end * 0.6) / (fog_end * 0.4), 0.0, 1.0);\n"
    "    frag = vec4(mix(v_color.r > 1.5 ? v_color - 2.0 : v_color * light * ambient, fog_color, fog), 1.0);\n"
    "}\n";

/* A campfire as boxes: a ring of stones, three logs crossed over embers and
 * a flame of stacked, flickering boxes that glow (red above 1.5, see
 * BOX_GLOW). t is in seconds; each fire flickers on its own. glow (0-1)
 * is how much the fire, not the sky, lights its stones and logs: at night
 * they show warm in its light instead of dark. */
#define CAMPFIRE_BOXES 14
#define MAX_DRAWN_FIRES 16
static int draw_campfire(float (*out)[INSTANCE_FLOATS], int n, int bx, int by, int bz, float t, float glow) {
    float x = bx + 0.5f, y = (float)by, z = bz + 0.5f;
    float seed = rand01(bx, by, bz) * 6.28f;
    for (int i = 0; i < 6; i++) { /* stones */
        float a = seed + i * 1.047f, shade = 0.40f + 0.12f * rand01(bx + i, by, bz);
        float stone[INSTANCE_FLOATS] = {x + cosf(a) * 0.36f, y + 0.07f, z + sinf(a) * 0.36f, 0.1f, 0.07f, 0.08f, a,
                                        shade, shade, shade * 1.05f};
        memcpy(out[n++], stone, sizeof stone);
    }
    int first = n - 6;
    for (int i = 0; i < 3; i++) { /* logs, crossed like a star */
        float a = seed + i * 1.047f;
        float log[INSTANCE_FLOATS] = {x, y + 0.07f + 0.04f * i, z, 0.05f, 0.05f, 0.3f, a, 0.38f, 0.26f, 0.15f};
        memcpy(out[n++], log, sizeof log);
    }
    if (glow > 0) /* firelit: warm and unlit (BOX_GLOW) */
        for (int i = first; i < n; i++) {
            float* c = out[i] + 7;
            c[0] = 2 + c[0] * (0.55f + 0.5f * glow);
            c[1] = 2 + c[1] * (0.35f + 0.2f * glow);
            c[2] = 2 + c[2] * 0.25f;
        }
    float embers[INSTANCE_FLOATS] = {x, y + 0.05f, z, 0.16f, 0.03f, 0.16f, seed, 2.9f, 2.25f, 2.05f};
    memcpy(out[n++], embers, sizeof embers);
    /* The flame: wide and orange below, thin and yellow on top, each layer
     * swaying and changing height on its own. */
    static const float layers[4][5] = { /* half width, height, red, green, blue */
        {0.17f, 0.10f, 3.0f, 2.42f, 2.08f}, {0.13f, 0.10f, 3.0f, 2.55f, 2.12f},
        {0.09f, 0.09f, 3.0f, 2.70f, 2.20f}, {0.05f, 0.08f, 3.0f, 2.85f, 2.35f}};
    float base = y + 0.12f;
    for (int i = 0; i < 4; i++) {
        const float* l = layers[i];
        float flick = sinf(t * (7.0f + i * 2.3f) + seed + i) * 0.5f + sinf(t * (11.0f + i) + seed * 2) * 0.5f;
        float h = l[1] * (1.0f + 0.35f * flick), w = l[0] * (1.0f + 0.12f * flick);
        float sway = 0.025f * i * sinf(t * 3.1f + seed);
        float flame[INSTANCE_FLOATS] = {x + sway, base + h, z + sway * 0.6f, w, h, w, t * (0.8f + 0.3f * i) + seed,
                                        l[2], l[3], l[4]};
        memcpy(out[n++], flame, sizeof flame);
        base += h * 1.6f;
    }
    return n;
}

/* ---- The other side ---------------------------------------------------- */

/* Where the night creatures come from, and where they drag what they take.
 * A small world of dark stone, always night and thick with mist, with pieces
 * of our world lying around. Light does not keep them away there and they
 * are faster than the player: the only safety is a shelter, and the only way
 * back is a light somewhere in the mist, which hums. Caught there, the player
 * wakes at the start of their own world. It is made again on each visit (not
 * saved); the player's world waits in home_data meanwhile. */
#define OTHER_CHUNKS 6
static float exit_x, exit_y, exit_z;
static int home_creatures;

static void remesh_world(void) {
    for (int i = 0; i < CHUNKS; i++)
        for (int j = 0; j < CHUNKS; j++) {
            p_glBindVertexArray(chunks[i][j].vao);
            mesh_chunk(i, j);
        }
}

/* The column's top block. */
static int top_at(int x, int z) {
    int y = WY - 1;
    while (y > 0 && world[x][y][z] == AIR) y--;
    return y;
}

static void generate_other_side(int visit) {
    int seed = world_seed * 31 + visit * 7 + 3;
    for (int x = 0; x < WX; x++)
        for (int z = 0; z < WZ; z++) {
            float hills = fbm(x / 18.0f, z / 18.0f, seed), rough = fbm(x / 5.0f, z / 5.0f, seed + 1);
            int h = 9 + (int)(hills * 12 + rough * 2);
            if (hash3(x, seed, z) % 89 == 0) h += 6 + hash3(z, seed, x) % 7; /* jagged spires */
            if (h > WY - 4) h = WY - 4;
            for (int y = 0; y < WY; y++) world[x][y][z] = (uint8_t)(y <= h ? (y < h - 4 ? STONE : SHADE) : AIR);
        }
    /* What they took from our world: bits of walls and dead trees. */
    static const int taken[4] = {PLASTER, BRICKS, PLANKS, ROOF};
    for (int k = 0; k < 14; k++) {
        int x = 3 + (int)(rand01(seed, k, 3) * (WX - 6)), z = 3 + (int)(rand01(seed, k, 5) * (WZ - 6)), y = top_at(x, z);
        int b = taken[hash3(seed, k, 7) % 4], len = 2 + k % 3, along = k % 2;
        if (k % 4 == 3) { /* a dead tree */
            for (int dy = 1; dy <= 3 + k % 2 && y + dy < WY; dy++) world[x][y + dy][z] = WOOD;
            continue;
        }
        for (int i = 0; i < len; i++)
            for (int dy = 1; dy <= 2; dy++) {
                int wx = along ? x + i : x, wz = along ? z : z + i;
                if (wx < WX && wz < WZ && y + dy < WY && rand01(wx, y + dy, wz) > 0.25f) world[wx][top_at(wx, wz) + 1][wz] = (uint8_t)b;
            }
    }
    /* The way back: far from where the player wakes, on flattened ground. */
    float a = rand01(seed, 9, 11) * 6.2831853f, r = WX * 0.38f;
    int ex = (int)(WX / 2 + cosf(a) * r), ez = (int)(WZ / 2 + sinf(a) * r), ey = top_at(ex, ez);
    for (int dx = -2; dx <= 2; dx++)
        for (int dz = -2; dz <= 2; dz++) {
            for (int y = ey + 1; y < WY; y++) world[ex + dx][y][ez + dz] = AIR;
            world[ex + dx][ey][ez + dz] = SHADE;
        }
    exit_x = ex + 0.5f;
    exit_y = ey + 1.0f;
    exit_z = ez + 0.5f;
}

/* Leaves the player's world for the other side: dragged there, or through the portal. */
static void enter_other_side(Player* p, int visit) {
    if (away) return;
    home_data = encode_world(&home_size);
    if (!home_data) return;
    home_chunks = world_chunks;
    home_x = p->x; home_y = p->y; home_z = p->z;
    world_chunks = home_chunks < OTHER_CHUNKS ? home_chunks : OTHER_CHUNKS;
    generate_other_side(visit);
    find_beacons(); /* none there */
    count_lamps();
    remesh_world();
    home_creatures = creature_count;
    creature_count = 0;
    for (int i = 0; i < MAX_NIGHT; i++) night[i].alive = 0;
    for (int n = 0; n < NOTES; n++) note_here[n] = 0;
    portal_here = 0;
    /* Wake in the middle; a note lies close by. */
    int x = WX / 2, z = WZ / 2;
    p->x = x + 0.5f; p->z = z + 0.5f; p->y = top_at(x, z) + 1.0f; p->vy = 0;
    int nx = x + 3, nz = z + 2;
    note_x[NOTE_AWAY] = nx + 0.5f; note_z[NOTE_AWAY] = nz + 0.5f;
    note_y[NOTE_AWAY] = (float)lowest_air(nx, nz);
    note_yaw[NOTE_AWAY] = 0.7f;
    note_here[NOTE_AWAY] = 1;
    grab_by = -1;
    grab_grace = 4.0f;
    away = 1;
    printf("blocks: woke up on the other side\n");
}

/* Back to the player's world: where they were taken (through the light), or
 * at the start (caught again). */
static void leave_other_side(Player* p, int to_start, float spawn_x, float spawn_y, float spawn_z) {
    if (!away) return;
    world_chunks = home_chunks;
    size_t done = 0, cells = (size_t)WX * WY * WZ, k = 0;
    for (size_t i = 0; i + 1 < home_size && done < cells; i += 2) {
        int run = home_data[i], b = home_data[i + 1];
        for (int r = 0; r < run && done < cells; r++, done++) {
            int x = (int)(k / ((size_t)WY * WZ)), y = (int)(k / WZ % WY), z = (int)(k % WZ);
            world[x][y][z] = (uint8_t)b;
            k++;
        }
    }
    free(home_data);
    home_data = NULL;
    away = 0;
    find_beacons();
    count_lamps();
    remesh_world();
    creature_count = home_creatures;
    pickups_day = -1; /* the world is back: its pickups too */
    for (int i = 0; i < MAX_NIGHT; i++) night[i].alive = 0;
    grab_by = -1;
    grab_grace = 4.0f;
    if (goal_flags & GOAL_RUINS) {
        find_notes(spawn_x, spawn_z);
        find_portal();
    }
    if (to_start) { p->x = spawn_x; p->y = spawn_y; p->z = spawn_z; }
    else { p->x = home_x; p->y = home_y; p->z = home_z; }
    /* Out of the portal, not back into it. */
    float dx = p->x - portal_x, dz = p->z - portal_z;
    if (portal_here && dx * dx + dz * dz < 4.0f) {
        p->x = portal_x + sinf(portal_yaw) * 2.5f;
        p->z = portal_z - cosf(portal_yaw) * 2.5f;
        p->y = WY - 2;
        while (p->y > 1 && !collides(p->x, p->y - 1, p->z)) p->y -= 1;
    }
    p->vy = 0;
    printf("blocks: back from the other side%s\n", to_start ? ", at the start" : "");
}

/* ---- Matrices --------------------------------------------------------- */

static void mat_mul(float* out, const float* a, const float* b) {
    float r[16];
    for (int c = 0; c < 4; c++)
        for (int row = 0; row < 4; row++) {
            float s = 0;
            for (int k = 0; k < 4; k++) s += a[k * 4 + row] * b[c * 4 + k];
            r[c * 4 + row] = s;
        }
    memcpy(out, r, sizeof r);
}

static void view_projection(float* out, const Player* p, float aspect, float far) {
    float fy = 1.0f / tanf(35.0f * 3.14159265f / 180.0f), near = 0.1f;
    float proj[16] = {fy / aspect, 0, 0, 0, 0, fy, 0, 0, 0, 0, -(far + near) / (far - near), -1,
                      0, 0, -2 * far * near / (far - near), 0};
    float cy = cosf(p->yaw), sy = sinf(p->yaw), cp = cosf(p->pitch), sp = sinf(p->pitch);
    /* Rows of the camera basis: right, up, back. */
    float rx = cy, ry = 0, rz = sy;
    float fx = cp * sy, fyv = sp, fz = -cp * cy;
    float ux = ry * fz - rz * fyv, uy = rz * fx - rx * fz, uz = rx * fyv - ry * fx;
    float ex = p->x, ey = p->y + EYE_HEIGHT, ez = p->z;
    float view[16] = {rx, ux, -fx, 0, ry, uy, -fyv, 0, rz, uz, -fz, 0,
                      -(rx * ex + ry * ey + rz * ez), -(ux * ex + uy * ey + uz * ez), fx * ex + fyv * ey + fz * ez, 1};
    mat_mul(out, proj, view);
}

/* ---- Audio ------------------------------------------------------------ */

/* The ring of samples (AudioRing) is in platform.h. In the emulated build
 * it is a file shared with the app and must match
 * engine/src/main/cpp/audio_output.cpp. */

enum { SND_STEP, SND_STEP2, SND_BREAK, SND_PLACE, SND_JUMP, SND_SPLASH, SND_CHIRP, SOUNDS };
/* Their file names, under sfx/ (see Sound files). */
static const char* const sound_names[SOUNDS] = {"step", "step2", "break", "place", "jump", "splash", "chirp"};

typedef struct {
    float* data;
    int length;
} Sound;

typedef struct {
    const Sound* sound;
    int pos;
    float left, right, pitch, frac;
} Voice;

#define VOICES 24

/* Sounds that keep going, louder or softer as the game asks: the wind, and
 * the crackle of the nearest campfire. */
enum { LOOP_WIND, LOOP_CAMPFIRE, LOOPS };
static const char* const loop_files[LOOPS] = {"ambient/wind.ogg", "sfx/campfire.ogg"};

typedef struct {
    Sound sound; /* empty: the wind is made up as it plays; the crackle never is */
    int pos;
    float gain;
    volatile float target; /* set by the game loop */
} Loop;

static struct {
    AudioRing* ring;
    int16_t* samples;
    int rate;
    Sound sounds[SOUNDS];
    Voice voices[VOICES];
    Loop loops[LOOPS];
    Sound speech; /* a voice reading a note, from a file */
    int speech_pos;
    pthread_mutex_t lock;
    pthread_t thread;
    volatile int running;
    float wind_state, wind_gust, wind_phase;
    uint32_t noise;
    double next_chirp;
    uint64_t mixed;
    int peak_voices;
    volatile float volume; /* 0-1, from the in-game settings */
} audio = {.lock = PTHREAD_MUTEX_INITIALIZER, .noise = 22222, .volume = 1.0f};

static float audio_noise(void) {
    audio.noise = audio.noise * 1664525u + 1013904223u;
    return (int32_t)audio.noise / 2147483648.0f;
}

/* All sounds are made here from noise and sine waves: no sound files. */
static Sound make_sound(float seconds, int kind) {
    Sound s;
    s.length = (int)(seconds * audio.rate);
    s.data = calloc((size_t)s.length, sizeof(float));
    float lp = 0, lp2 = 0, phase = 0;
    for (int i = 0; i < s.length; i++) {
        float t = (float)i / audio.rate, k = (float)i / s.length;
        float n = audio_noise(), v = 0;
        switch (kind) {
        case SND_STEP: case SND_STEP2: { /* a soft crunch on grass */
            float cut = kind == SND_STEP ? 0.18f : 0.24f;
            lp += cut * (n - lp);
            v = lp * expf(-t * 45.0f) * 2.2f;
            break;
        }
        case SND_BREAK: { /* crumbling: noise with clicks, darker as it ends */
            lp += (0.5f - 0.4f * k) * (n - lp);
            float clicks = (i % (audio.rate / 37) < 40) ? 1.6f : 1.0f;
            v = lp * clicks * expf(-t * 11.0f) * 1.6f;
            break;
        }
        case SND_PLACE: /* a wooden knock */
            phase += 2 * 3.14159265f * (180.0f - 60.0f * k) / audio.rate;
            lp += 0.3f * (n - lp);
            v = (sinf(phase) * 0.9f + lp * 0.6f) * expf(-t * 30.0f);
            break;
        case SND_JUMP: /* a short whoosh */
            lp += (0.05f + 0.25f * k) * (n - lp);
            v = lp * sinf(3.14159265f * k) * 1.4f;
            break;
        case SND_SPLASH: /* water: bright noise, slower decay */
            lp += 0.6f * (n - lp);
            lp2 += 0.08f * (lp - lp2);
            v = (lp - lp2) * expf(-t * 5.0f) * 1.8f;
            break;
        case SOUNDS: { /* a campfire: soft hiss, a low rumble and random pops */
            lp += 0.02f * (n - lp);
            lp2 += 0.5f * (n - lp2);
            float pop = audio_noise() > 0.9993f ? 1.0f : 0;
            phase = phase * 0.995f + pop * (0.6f + 0.4f * audio_noise());
            v = lp * 0.9f + lp2 * 0.05f + phase * lp2 * 1.4f;
            break;
        }
        default: { /* a bird: three quick rising chirps */
            float local = fmodf(t, 0.11f);
            float f = 2600.0f + 9000.0f * local;
            phase += 2 * 3.14159265f * f / audio.rate;
            v = local < 0.07f ? sinf(phase) * sinf(3.14159265f * local / 0.07f) * 0.5f : 0;
            break;
        }
        }
        s.data[i] = v;
    }
    return s;
}

/* Starts a sound; pan -1 (left) to 1 (right). Callable from the game loop. */
static void play_sound(int id, float gain, float pan, float pitch) {
    if (!audio.ring) return;
    pthread_mutex_lock(&audio.lock);
    Voice* v = NULL;
    for (int i = 0; i < VOICES && !v; i++)
        if (!audio.voices[i].sound) v = &audio.voices[i];
    if (!v) v = &audio.voices[0]; /* all busy: steal the first */
    if (pan < -1) pan = -1;
    if (pan > 1) pan = 1;
    v->sound = &audio.sounds[id];
    v->pos = 0;
    v->frac = 0;
    v->pitch = pitch;
    v->left = gain * sqrtf(0.5f * (1 - pan));
    v->right = gain * sqrtf(0.5f * (1 + pan));
    pthread_mutex_unlock(&audio.lock);
}

/* Mixes frames into the ring: the active voices plus wind and birds. */
static void mix(uint64_t start, int frames) {
    pthread_mutex_lock(&audio.lock);
    int active = 0;
    for (int i = 0; i < VOICES; i++) active += audio.voices[i].sound != NULL;
    if (active > audio.peak_voices) audio.peak_voices = active;
    for (int f = 0; f < frames; f++) {
        /* Wind: slowly varying, filtered noise. */
        audio.wind_phase += 1.0f / audio.rate;
        float gust = 0.5f + 0.5f * sinf(audio.wind_phase * 0.23f) * sinf(audio.wind_phase * 0.071f + 1.0f);
        audio.wind_state += 0.012f * (audio_noise() - audio.wind_state);
        float l = audio.loops[LOOP_WIND].sound.data ? 0 : audio.wind_state * (0.12f + 0.25f * gust), r = l * 0.85f;
        for (int i = 0; i < LOOPS; i++) {
            Loop* lo = &audio.loops[i];
            if (!lo->sound.data) continue;
            lo->gain += (lo->target - lo->gain) * 0.0002f; /* eases over a few tenths of a second */
            if (lo->gain > 0.001f) {
                float s = lo->sound.data[lo->pos] * lo->gain;
                l += s;
                r += s;
            }
            if (++lo->pos >= lo->sound.length) lo->pos = 0;
        }
        if (audio.speech.data) {
            float s = audio.speech.data[audio.speech_pos];
            l += s;
            r += s;
            if (++audio.speech_pos >= audio.speech.length) {
                free(audio.speech.data);
                audio.speech.data = NULL;
            }
        }
        for (int i = 0; i < VOICES; i++) {
            Voice* v = &audio.voices[i];
            if (!v->sound) continue;
            float s = v->sound->data[v->pos];
            l += s * v->left;
            r += s * v->right;
            v->frac += v->pitch;
            while (v->frac >= 1.0f) { v->frac -= 1.0f; v->pos++; }
            if (v->pos >= v->sound->length) v->sound = NULL;
        }
        /* Soft clip, then 16 bits. */
        l = l / (1.0f + fabsf(l));
        r = r / (1.0f + fabsf(r));
        uint32_t index = (uint32_t)((start + (uint64_t)f) % audio.ring->capacity);
        audio.samples[index * 2] = (int16_t)(l * 32000.0f * audio.volume);
        audio.samples[index * 2 + 1] = (int16_t)(r * 32000.0f * audio.volume);
    }
    pthread_mutex_unlock(&audio.lock);
}

/* Keeps about 60 ms of sound ahead of what the app has played: enough to
 * ride out the thread being late now and then on a busy phone. */
static void* audio_thread(void* unused) {
    (void)unused;
    uint64_t ahead = (uint64_t)audio.rate * 60 / 1000;
    uint64_t write = __atomic_load_n(&audio.ring->read_pos, __ATOMIC_ACQUIRE);
    while (audio.running) {
        uint64_t read = __atomic_load_n(&audio.ring->read_pos, __ATOMIC_ACQUIRE);
        if (read > write) write = read; /* we fell behind: skip ahead */
        if (write - read < ahead) {
            int frames = (int)(ahead - (write - read));
            mix(write, frames);
            write += (uint64_t)frames;
            audio.mixed += (uint64_t)frames;
            __atomic_store_n(&audio.ring->write_pos, write, __ATOMIC_RELEASE);
        }
        double now = now_ms();
        if (now > audio.next_chirp) {
            if (audio.next_chirp > 0)
                play_sound(SND_CHIRP, 0.12f + 0.1f * (audio_noise() + 1), audio_noise(), 0.9f + 0.2f * audio_noise());
            audio.next_chirp = now + 4000 + 6000 * (audio_noise() + 1);
        }
        sleep_ms(3);
    }
    return NULL;
}

/* ---- Sound files ----
 * HYPOCKET_BLOCKS_MEDIA names a folder (the app copies blocks/assets/audio
 * there) of Ogg Vorbis files, decoded once, mixed down to mono and resampled
 * to the ring's rate:
 *   sfx/<name>.ogg      replaces the made-up sound of that name (sound_names)
 *   sfx/campfire.ogg    the crackle near campfires, looped (else made up)
 *   ambient/wind.ogg    the wind, looped (else made up)
 *   voice/note<N>_voz.ogg  read aloud when note N is picked up in the world
 * Without the folder or a file the game sounds as it did. */
static const char* media_dir;

static int load_sound(const char* name, Sound* out) {
    if (!media_dir || audio.rate <= 0) return 0;
    char path[1024];
    snprintf(path, sizeof path, "%s/%s", media_dir, name);
    if (access(path, R_OK) != 0) return 0;
    int channels = 0, rate = 0;
    short* pcm = NULL;
    int frames = stb_vorbis_decode_filename(path, &channels, &rate, &pcm);
    if (frames <= 0 || channels <= 0 || rate <= 0) {
        printf("blocks: sound %s could not be read\n", name);
        free(pcm);
        return 0;
    }
    int length = (int)((int64_t)frames * audio.rate / rate);
    float* data = malloc((size_t)(length > 0 ? length : 1) * sizeof *data);
    for (int i = 0; i < length; i++) {
        double at = (double)i * rate / audio.rate;
        int a = (int)at, b = a + 1 < frames ? a + 1 : a;
        float f = (float)(at - a), va = 0, vb = 0;
        for (int c = 0; c < channels; c++) {
            va += pcm[a * channels + c];
            vb += pcm[b * channels + c];
        }
        data[i] = (va + (vb - va) * f) / (channels * 32768.0f);
    }
    free(pcm);
    out->data = data;
    out->length = length > 0 ? length : 1;
    printf("blocks: sound %s, %.1f s\n", name, (double)length / audio.rate);
    return 1;
}

/* Reads a note aloud, if it has a recording; decoded on its own thread so
 * the game does not stop for it. */
static void* speech_thread(void* arg) {
    char name[64];
    snprintf(name, sizeof name, "voice/note%d_voz.ogg", (int)(intptr_t)arg);
    Sound s = {0};
    if (load_sound(name, &s)) {
        pthread_mutex_lock(&audio.lock);
        free(audio.speech.data);
        audio.speech = s;
        audio.speech_pos = 0;
        pthread_mutex_unlock(&audio.lock);
    }
    return NULL;
}

static void read_note_aloud(int note) {
    if (!audio.ring || !media_dir) return;
    pthread_t t;
    if (pthread_create(&t, NULL, speech_thread, (void*)(intptr_t)note) == 0) pthread_detach(t);
}

static void stop_speech(void) {
    if (!audio.ring) return;
    pthread_mutex_lock(&audio.lock);
    free(audio.speech.data);
    audio.speech.data = NULL;
    pthread_mutex_unlock(&audio.lock);
}

/* Starts the sound on a ring that is ready to be read. */
static void start_audio_ring(AudioRing* ring) {
    audio.rate = (int)ring->rate;
    static const float lengths[SOUNDS] = {0.12f, 0.12f, 0.45f, 0.16f, 0.25f, 0.6f, 0.33f};
    media_dir = getenv("HYPOCKET_BLOCKS_MEDIA");
    for (int i = 0; i < SOUNDS; i++) {
        char name[64];
        snprintf(name, sizeof name, "sfx/%s.ogg", sound_names[i]);
        if (!load_sound(name, &audio.sounds[i])) audio.sounds[i] = make_sound(lengths[i], i);
    }
    for (int i = 0; i < LOOPS; i++) load_sound(loop_files[i], &audio.loops[i].sound);
    if (!audio.loops[LOOP_CAMPFIRE].sound.data) audio.loops[LOOP_CAMPFIRE].sound = make_sound(4.0f, SOUNDS);
    audio.loops[LOOP_WIND].target = 0.5f;
    audio.samples = (int16_t*)((char*)ring + 64);
    audio.ring = ring;
    audio.running = 1;
    if (pthread_create(&audio.thread, NULL, audio_thread, NULL) != 0) {
        audio.ring = NULL;
        return;
    }
    printf("blocks: audio %d Hz, %u frames of ring\n", audio.rate, ring->capacity);
}

static void start_audio(void) {
    AudioRing* own = blocks_platform && blocks_platform->audio_ring ? blocks_platform->audio_ring() : NULL;
    if (own) {
        start_audio_ring(own);
        return;
    }
#ifndef _WIN32
    const char* path = getenv("HYPOCKET_AUDIO_PATH");
    if (!path) return;
    int fd = open(path, O_RDWR);
    if (fd < 0) return;
    struct stat st;
    void* map = MAP_FAILED;
    if (fstat(fd, &st) == 0 && st.st_size > 64)
        map = mmap(NULL, (size_t)st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (map == MAP_FAILED) return;
    AudioRing* ring = map;
    if (__atomic_load_n(&ring->magic, __ATOMIC_ACQUIRE) != AUDIO_MAGIC || ring->channels != 2 || !ring->rate ||
        64 + (uint64_t)ring->capacity * 4 > (uint64_t)st.st_size) {
        printf("blocks: audio ring not usable\n");
        return;
    }
    start_audio_ring(ring);
#endif
}

static void stop_audio(void) {
    if (!audio.ring) return;
    audio.running = 0;
    pthread_join(audio.thread, NULL);
    __atomic_store_n(&audio.ring->finished, 1, __ATOMIC_RELEASE);
    printf("blocks: audio mixed %.1f s, up to %d sounds at once, %u underruns\n",
           (double)audio.mixed / audio.rate, audio.peak_voices, audio.ring->underruns);
}

/* Plays a sound coming from a block: panned and quieter with distance. */
static void play_at(int id, const Player* p, float x, float y, float z, float pitch) {
    float dx = x - p->x, dy = y - (p->y + EYE_HEIGHT), dz = z - p->z;
    float dist = sqrtf(dx * dx + dy * dy + dz * dz) + 0.001f;
    float pan = (dx * cosf(p->yaw) + dz * sinf(p->yaw)) / dist;
    play_sound(id, 0.9f / (1.0f + 0.25f * dist), pan * 0.8f, pitch);
}

/* ---- Main ------------------------------------------------------------- */

/* The renderbuffers of each framebuffer made here, to free them with it. */
static struct { GLuint fbo, rb[2]; } targets[4];

static void delete_target(GLuint fbo) {
    for (int i = 0; i < 4; i++)
        if (targets[i].fbo == fbo && fbo) {
            p_glDeleteRenderbuffers(2, targets[i].rb);
            p_glDeleteFramebuffers(1, &targets[i].fbo);
            targets[i].fbo = 0;
        }
}

static int create_target(int width, int height, GLuint* fbo) {
    GLuint rb[2];
    p_glGenFramebuffers(1, fbo);
    p_glBindFramebuffer(GL_FRAMEBUFFER, *fbo);
    p_glGenRenderbuffers(2, rb);
    p_glBindRenderbuffer(GL_RENDERBUFFER, rb[0]);
    p_glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);
    p_glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rb[0]);
    p_glBindRenderbuffer(GL_RENDERBUFFER, rb[1]);
    p_glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    p_glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rb[1]);
    for (int i = 0; i < 4; i++)
        if (!targets[i].fbo) {
            targets[i].fbo = *fbo;
            targets[i].rb[0] = rb[0];
            targets[i].rb[1] = rb[1];
            break;
        }
    return p_glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

/* Writes <save>.thumb: "HPBT", width, height (32-bit, little endian), then
 * RGBA rows from the bottom up, as OpenGL reads them. BlocksWorlds.kt reads it. */
#define THUMB_MAGIC 0x54425048u /* "HPBT" */
static void save_thumbnail(const char* save_path, GLuint source, int w, int h) {
    static GLuint thumb_fbo;
    static int tw, th;
    if (!thumb_fbo) {
        tw = 192;
        th = (192 * h / w) & ~1;
        if (th < 2 || !create_target(tw, th, &thumb_fbo)) return;
    }
    static uint8_t pixels[192 * 192 * 4];
    if (th > 192) return;
    p_glBindFramebuffer(GL_READ_FRAMEBUFFER, source);
    p_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, thumb_fbo);
    p_glBlitFramebuffer(0, 0, w, h, 0, 0, tw, th, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    p_glBindFramebuffer(GL_READ_FRAMEBUFFER, thumb_fbo);
    p_glReadPixels(0, 0, tw, th, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    p_glBindFramebuffer(GL_FRAMEBUFFER, source);
    char path[4096], tmp[4200];
    snprintf(path, sizeof path, "%s.thumb", save_path);
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE* f = fopen(tmp, "wb");
    if (!f) return;
    uint32_t header[3] = {THUMB_MAGIC, (uint32_t)tw, (uint32_t)th};
    int ok = fwrite(header, sizeof header, 1, f) == 1 && fwrite(pixels, (size_t)tw * th * 4, 1, f) == 1;
    ok = fclose(f) == 0 && ok;
    if (!ok || replace_file(tmp, path) != 0) unlink(tmp);
}

static SharedIo* open_io(void) {
    static SharedIo local = {.volume_percent = 100};
    const char* path = getenv("HYPOCKET_INPUT_PATH");
#ifdef _WIN32
    (void)path;
    return &local;
#else
    if (!path) return &local;
    int fd = open(path, O_RDWR);
    if (fd < 0) return &local;
    void* map = mmap(NULL, IO_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (map == MAP_FAILED || ((SharedIo*)map)->magic != IO_MAGIC) return &local;
    return map;
#endif
}

#ifndef BLOCKS_LIBRARY
int main(int argc, char** argv) { return blocks_main(argc, argv); }
#endif

int blocks_main(int argc, char** argv) {
    _Static_assert(sizeof(SharedIo) <= IO_SIZE, "shared file too small");
    _Static_assert(sizeof(SaveHeader) == 64, "the save header must keep its size");
    _Static_assert(__builtin_offsetof(SharedIo, counts) == 128, "must match BlocksSession.kt");
    _Static_assert(__builtin_offsetof(SharedIo, health_tenths) == 160, "must match BlocksSession.kt");
    _Static_assert(__builtin_offsetof(SharedIo, paused) == 176, "must match BlocksSession.kt");
    _Static_assert(__builtin_offsetof(SharedIo, beacons) == 196, "must match BlocksSession.kt");
    _Static_assert(__builtin_offsetof(SharedIo, story_flags) == 232 && sizeof(SharedIo) == IO_SIZE,
                   "must match BlocksSession.kt");
    _Static_assert(__builtin_offsetof(SharedIo, all_counts) == 256 && __builtin_offsetof(SharedIo, food_tenths) == 352,
                   "must match BlocksSession.kt");
    _Static_assert(PALETTE_SIZE <= 48 && BLOCK_TYPES <= ITEM_BASE && ITEM_END <= THING_TYPES, "room for every thing");
    _Static_assert(__builtin_offsetof(SharedIo, crafts) == 52 && __builtin_offsetof(SharedIo, craft) == 56,
                   "must match BlocksSession.kt");
    int w = argc > 1 ? atoi(argv[1]) : 1200;
    int h = argc > 2 ? atoi(argv[2]) : 540;
    int fps_limit = argc > 3 ? atoi(argv[3]) : 0;
    int max_frames = argc > 4 ? atoi(argv[4]) : 0;
    if (w < 64 || h < 64 || w > 4096 || h > 4096) return 2;
    SharedIo* io = open_io();
    const BlocksPlatform* platform = blocks_platform;
    /* present: frames go to a window (or the engine's direct display). */
    int present = platform || getenv("HYPOCKET_PRESENT") != NULL;
    const char* frame_path = present ? NULL : getenv("HYPOCKET_FRAME_PATH");
#ifdef _WIN32
    frame_path = NULL; /* no shared files on Windows */
    (void)frame_path;
#endif

    uint32_t* header = NULL;
    unsigned char* pixels = NULL;
#ifndef _WIN32
    if (frame_path) {
        size_t size = 64 + (size_t)w * h * 4;
        int fd = open(frame_path, O_RDWR | O_CREAT, 0600);
        if (fd < 0 || ftruncate(fd, (off_t)size) != 0) return 1;
        unsigned char* shared = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (shared == MAP_FAILED) return 1;
        header = (uint32_t*)shared;
        pixels = shared + 64;
        header[1] = (uint32_t)w;
        header[2] = (uint32_t)h;
        header[3] = 0;
        header[4] = 1;
        header[0] = 0x42465048;
    }
#endif

    int window_target = 0; /* draw into the pbuffer itself, without our own framebuffer */
#ifdef BLOCKS_LIBRARY
    /* The platform made the context current: only the functions to look up. */
    if (!platform) return 1;
    gl_es = platform->gles;
#define LOAD(type, name) \
    p_##name = (type)platform->get_proc(#name); \
    if (!p_##name) { printf("blocks: missing %s\n", #name); return 1; }
    GL_FUNCTIONS(LOAD)
#undef LOAD
#else
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (!eglInitialize(display, NULL, NULL) || !eglBindAPI(EGL_OPENGL_API)) return 1;
    /* With direct present, draw straight into the pbuffer like a game draws
     * into its window: that needs a depth buffer in the config. */
    const EGLint depth_attribs[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
                                    EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
                                    EGL_DEPTH_SIZE, 24, EGL_NONE};
    const EGLint config_attribs[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
                                     EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_NONE};
    EGLConfig config;
    EGLint count = 0;
    if (present && eglChooseConfig(display, depth_attribs, &config, 1, &count) && count > 0) window_target = 1;
    else eglChooseConfig(display, config_attribs, &config, 1, &count);
    /* With direct present the pbuffer is the frame; otherwise it is unused. */
    const EGLint surface_attribs[] = {EGL_WIDTH, present ? w : 16, EGL_HEIGHT, present ? h : 16, EGL_NONE};
    EGLSurface surface = count ? eglCreatePbufferSurface(display, config, surface_attribs) : EGL_NO_SURFACE;
    const EGLint context_attribs[] = {EGL_CONTEXT_MAJOR_VERSION, 4, EGL_CONTEXT_MINOR_VERSION, 1,
                                      EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_NONE};
    EGLContext context = count ? eglCreateContext(display, config, EGL_NO_CONTEXT, context_attribs) : EGL_NO_CONTEXT;
    if (context == EGL_NO_CONTEXT || !eglMakeCurrent(display, surface, surface, context)) {
        printf("blocks: no OpenGL 4.1 core context (0x%x)\n", eglGetError());
        return 1;
    }
#define LOAD(type, name) \
    p_##name = (type)eglGetProcAddress(#name); \
    if (!p_##name) { printf("blocks: missing %s\n", #name); return 1; }
    GL_FUNCTIONS(LOAD)
#undef LOAD
#endif
    printf("blocks: GL_RENDERER %s\n", p_glGetString(GL_RENDERER));
    printf("blocks: %dx%d, fps limit %d\n", w, h, fps_limit);
    fflush(stdout);

    GLuint fbo = 0;
    printf("blocks: drawing into %s\n", window_target ? "the pbuffer" : "a framebuffer object");
    if (!window_target && !create_target(w, h, &fbo)) {
        printf("blocks: framebuffer %dx%d incomplete\n", w, h);
        return 1;
    }
    /* Dynamic resolution: the 3D scene goes into part of this framebuffer,
     * scaled up to the frame afterwards (the crosshair stays sharp). */
    int dynamic = getenv("HYPOCKET_BLOCKS_DYNAMIC") != NULL;
    GLuint scene_fbo = 0;
    if (dynamic && !create_target(w, h, &scene_fbo)) dynamic = 0;
    float scale = 1.0f;
    printf("blocks: dynamic resolution %s\n", dynamic ? "on" : "off");

    GLuint world_program = link_program(world_vs, world_fs);
    GLuint hud_program = link_program(hud_vs, hud_fs);
    GLint u_view_proj = p_glGetUniformLocation(world_program, "view_proj");
    GLint u_origin = p_glGetUniformLocation(world_program, "chunk_origin");
    GLint u_eye = p_glGetUniformLocation(world_program, "eye");
    GLint u_fog_color = p_glGetUniformLocation(world_program, "fog_color");
    GLint u_fog_end = p_glGetUniformLocation(world_program, "fog_end");
    GLint u_ambient = p_glGetUniformLocation(world_program, "ambient");
    GLuint sky_program = link_program(sky_vs, sky_fs);
    GLint us_view_proj = p_glGetUniformLocation(sky_program, "view_proj");
    GLint us_sun_dir = p_glGetUniformLocation(sky_program, "sun_dir");
    GLint us_sun_color = p_glGetUniformLocation(sky_program, "sun_color");
    GLint us_moon_color = p_glGetUniformLocation(sky_program, "moon_color");
    p_glUseProgram(sky_program);
    p_glUniform1f(p_glGetUniformLocation(sky_program, "aspect"), (float)w / h);
    p_glUseProgram(world_program);
    p_glUniform1i(p_glGetUniformLocation(world_program, "tex"), 0);
    p_glUniform1f(p_glGetUniformLocation(world_program, "glow_layer"), (float)T_LAMP);
    p_glUniform1f(p_glGetUniformLocation(world_program, "glow_layer2"), (float)T_BEACON_ON);
    p_glUseProgram(hud_program);
    p_glUniform2f(p_glGetUniformLocation(hud_program, "pixel"), 2.0f / w * (h / 540.0f), 2.0f / h * (h / 540.0f));
    make_textures();

    int heavy = getenv("HYPOCKET_BLOCKS_HEAVY") != NULL;
    if (heavy) world_chunks = 16;
    const char* chunks_env = getenv("HYPOCKET_BLOCKS_CHUNKS");
    if (chunks_env && atoi(chunks_env) >= 4 && atoi(chunks_env) <= MAX_CHUNKS) world_chunks = atoi(chunks_env);
    const char* seed_env = getenv("HYPOCKET_BLOCKS_SEED");
    if (seed_env) world_seed = abs(atoi(seed_env) % 1000000);
    const char* save_path = getenv("HYPOCKET_BLOCKS_SAVE");
    creative = getenv("HYPOCKET_BLOCKS_CREATIVE") != NULL;
    int separate_draws = getenv("HYPOCKET_BLOCKS_SEPARATE_DRAWS") != NULL;
    GLuint box_program = link_program(box_vs, box_fs);
    GLint ub_view_proj = p_glGetUniformLocation(box_program, "view_proj");
    GLint ub_center = p_glGetUniformLocation(box_program, "center");
    GLint ub_half = p_glGetUniformLocation(box_program, "half_size");
    GLint ub_yaw = p_glGetUniformLocation(box_program, "yaw");
    GLint ub_eye = p_glGetUniformLocation(box_program, "eye");
    GLint ub_color = p_glGetUniformLocation(box_program, "color");
    GLint ub_fog_color = p_glGetUniformLocation(box_program, "fog_color");
    GLint ub_fog_end = p_glGetUniformLocation(box_program, "fog_end");
    GLint ub_ambient = p_glGetUniformLocation(box_program, "ambient");
    GLuint inst_program = link_program(instanced_vs, instanced_fs);
    GLint ui_view_proj = p_glGetUniformLocation(inst_program, "view_proj");
    GLint ui_eye = p_glGetUniformLocation(inst_program, "eye");
    GLint ui_fog_color = p_glGetUniformLocation(inst_program, "fog_color");
    GLint ui_fog_end = p_glGetUniformLocation(inst_program, "fog_end");
    GLint ui_ambient = p_glGetUniformLocation(inst_program, "ambient");
    /* Instances: two boxes per creature, one per particle. */
    enum { MAX_INSTANCES = MAX_CREATURES * 2 + MAX_NIGHT * 4 + MAX_PARTICLES + MAX_PICKUPS * 2 +
                           MAX_DRAWN_FIRES * CAMPFIRE_BOXES + 64 };
    static float instances[MAX_INSTANCES][INSTANCE_FLOATS];
    GLuint inst_vao, inst_vbo;
    p_glGenVertexArrays(1, &inst_vao);
    p_glBindVertexArray(inst_vao);
    p_glGenBuffers(1, &inst_vbo);
    p_glBindBuffer(GL_ARRAY_BUFFER, inst_vbo);
    p_glBufferData(GL_ARRAY_BUFFER, sizeof instances, NULL, GL_STREAM_DRAW);
    {
        const int sizes[4] = {3, 3, 1, 3}, offsets[4] = {0, 3, 6, 7};
        for (int a = 0; a < 4; a++) {
            p_glVertexAttribPointer(a, sizes[a], GL_FLOAT, GL_FALSE, INSTANCE_FLOATS * sizeof(float),
                                    (void*)(offsets[a] * sizeof(float)));
            p_glVertexAttribDivisor(a, 1);
            p_glEnableVertexAttribArray(a);
        }
    }

    double build_start = now_ms();
    Player player = {0};
    int loaded = save_path && load_world(save_path, &player);
    if (!loaded && save_path && access(save_path, F_OK) == 0) {
        /* A save this version cannot read (damaged, or from a newer version):
         * keep it aside instead of writing a new world over it. */
        char aside[4096];
        snprintf(aside, sizeof aside, "%s.unreadable", save_path);
        if (rename(save_path, aside) == 0) printf("blocks: kept the unreadable save as %s\n", aside);
    }
    if (!loaded) generate_world();
    /* The goal: beacons, except in the benchmark, which keeps its scene. */
    if (!(max_frames && getenv("HYPOCKET_BLOCKS_BENCH")) && !(goal_flags & GOAL_PLACED)) {
        place_beacons();
        goal_flags |= GOAL_PLACED;
        world_changed = 1;
    }
    if (!(max_frames && getenv("HYPOCKET_BLOCKS_BENCH")) && !(goal_flags & GOAL_RUINS)) {
        place_ruins();
        goal_flags |= GOAL_RUINS;
        world_changed = 1;
    }
    find_beacons();
    count_lamps();
    GLuint ebo;
    p_glGenBuffers(1, &ebo);
    scratch = malloc((size_t)CHUNK * CHUNK * WY * 3 * 4 * sizeof(Vertex));
    for (int i = 0; i < CHUNKS; i++)
        for (int j = 0; j < CHUNKS; j++) {
            Chunk* ch = &chunks[i][j];
            p_glGenVertexArrays(1, &ch->vao);
            p_glBindVertexArray(ch->vao);
            p_glGenBuffers(1, &ch->vbo);
            p_glBindBuffer(GL_ARRAY_BUFFER, ch->vbo);
            p_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
            p_glVertexAttribPointer(0, 4, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(Vertex), (void*)0);
            p_glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(Vertex), (void*)4);
            p_glEnableVertexAttribArray(0);
            p_glEnableVertexAttribArray(1);
            mesh_chunk(i, j);
        }
    GLuint hud_vao;
    p_glGenVertexArrays(1, &hud_vao);
    double build_ms = now_ms() - build_start;
    int total_quads = 0;
    for (int i = 0; i < CHUNKS; i++)
        for (int j = 0; j < CHUNKS; j++) total_quads += chunks[i][j].quads + chunks[i][j].water_quads;
    printf("blocks: world %s in %.0f ms, %d faces, seed %d\n", loaded ? "loaded" : "built", build_ms, total_quads,
           world_seed);
    fflush(stdout);

    /* The starting point: the grass nearest the middle, spiralling out. New
     * worlds start there, and players come back there after fainting. It is
     * worked out again on each load, so saves need not keep it. */
    float spawn_x = WX / 2 + 0.5f, spawn_y = WY, spawn_z = WZ / 2 + 0.5f;
    for (int r = 0, found = 0; r < WX / 2 && !found; r++)
        for (int dx = -r; dx <= r && !found; dx++)
            for (int dz = -r; dz <= r && !found; dz++) {
                int x = WX / 2 + dx, z = WZ / 2 + dz, y = WY - 3;
                while (y > 0 && world[x][y][z] == AIR) y--;
                if (world[x][y][z] == GRASS && world[x][y + 1][z] == AIR && world[x][y + 2][z] == AIR) {
                    spawn_x = x + 0.5f;
                    spawn_z = z + 0.5f;
                    spawn_y = y + 1.0f;
                    found = 1;
                }
            }
    while (spawn_y > 1 && !collides(spawn_x, spawn_y - 1, spawn_z)) spawn_y -= 1;
    if (!loaded) player = (Player){spawn_x, spawn_y, spawn_z, 0, 0, -0.25f, 0};
    if (goal_flags & GOAL_RUINS) {
        find_notes(spawn_x, spawn_z);
        find_portal();
        if (goal_flags & GOAL_AWAY) { /* saved on the other side: back there */
            goal_flags &= ~GOAL_AWAY;
            enter_other_side(&player, 0);
        }
        printf("blocks: start at %.1f %.1f %.1f, first note at %.1f %.1f %.1f\n", spawn_x, spawn_y, spawn_z, note_x[0],
               note_y[0], note_z[0]);
    }
    if (getenv("HYPOCKET_BLOCKS_TEST_ROOM")) { /* testing: a closed room with a lamp around the start */
        int rx = (int)spawn_x, ry = (int)spawn_y, rz = (int)spawn_z, open = atoi(getenv("HYPOCKET_BLOCKS_TEST_ROOM")) == 2;
        for (int dx = -2; dx <= 2; dx++)
            for (int dz = -2; dz <= 2; dz++)
                for (int dy = -1; dy <= 3; dy++) {
                    int edge = abs(dx) == 2 || abs(dz) == 2 || dy == -1 || dy == 3;
                    world[rx + dx][ry + dy][rz + dz] = (uint8_t)(edge ? PLASTER : AIR);
                }
        world[rx][ry + 3][rz] = LAMP;
        if (open) world[rx + 2][ry][rz] = world[rx + 2][ry + 1][rz] = AIR; /* a doorway */
        count_lamps();
        for (int i = 0; i < CHUNKS; i++)
            for (int j = 0; j < CHUNKS; j++) chunks[i][j].dirty = 2;
    }
    /* A loaded world keeps the direction the player faced (not the pitch,
     * which the app clamps from the default). */
    float base_yaw = io->look_yaw - player.yaw, base_pitch = io->look_pitch;
    int seen_breaks = io->breaks, seen_places = io->places, seen_crafts = io->crafts;

    /* Time of day: the save's, or HYPOCKET_BLOCKS_TIME, or the morning. */
    const char* time_env = getenv("HYPOCKET_BLOCKS_TIME");
    if (time_env && (!loaded || max_frames)) day_time = fmodf(fabsf((float)atof(time_env)), 1.0f);
    float day_seconds = 15 * 60;
    if (getenv("HYPOCKET_BLOCKS_DAY_SECONDS") && atof(getenv("HYPOCKET_BLOCKS_DAY_SECONDS")) >= 10)
        day_seconds = (float)atof(getenv("HYPOCKET_BLOCKS_DAY_SECONDS"));
    const float default_far = heavy ? 128.0f : 72.0f;
    float far = default_far;
    float fire_clock = 0, smoke_wait = 0; /* campfire flicker and smoke */
    int was_paused = 0;
    /* Danger: night creatures and fall damage. Creative worlds have neither;
     * peaceful worlds (HYPOCKET_BLOCKS_PEACEFUL) keep falls but no attackers. */
    int peaceful = getenv("HYPOCKET_BLOCKS_PEACEFUL") != NULL;
    night_limit = creative || peaceful || (max_frames && getenv("HYPOCKET_BLOCKS_BENCH")) ? 0 : heavy ? MAX_NIGHT : 10;
    float health = saved_health, calm = 0; /* calm: seconds until health starts coming back */
    /* Survival: hunger and cold (not in creative worlds or the benchmark). */
    int survival = !creative && !(max_frames && getenv("HYPOCKET_BLOCKS_BENCH"));
    int stations = 0, dig_x = -1, dig_y = -1, dig_z = -1, dig_blows = 0, dig_need = 1, aiming_pickup = 0;
    float cold_rate = 0;
    if (heavy) spawn_creatures(MAX_CREATURES);
    else if (getenv("HYPOCKET_BLOCKS_CREATURES")) {
        int n = atoi(getenv("HYPOCKET_BLOCKS_CREATURES"));
        spawn_creatures(n < 0 ? 0 : n > MAX_CREATURES ? MAX_CREATURES : n);
    }
    printf("blocks: %s mode, %dx%d chunks, %d creatures, view distance %.0f\n", heavy ? "heavy" : "normal",
           world_chunks, world_chunks, creature_count, far);
    double last = now_ms(), stats_start = last, work_total = 0;
    int stats_frames = 0, frames = 0;
    /* Dynamic resolution looks at half a second at a time. */
    double wait_total = 0, window_start = last, window_work = 0, window_wait = 0;
    int window_frames = 0, calm_windows = 0, cooldown_windows = 0, missed_windows = 0;
    int stuck_windows = 0, probing = 0, probe_windows = 0;
    double probe_fps = 0, no_lower_until = 0;
    const int launch_fps_limit = fps_limit;
    double target_fps = fps_limit > 0 ? fps_limit : 60;
    const double run_start = last;
    double log_ms = 0, log_work = 0, log_wait = 0; /* since the last printed statistics */
    int log_frames = 0;
    float log_low_fps = 1e9f;
    GLsync previous = 0;
    io->width = w;
    io->height = h;
    io->build_ms = (float)build_ms;

    start_audio();
    float step_timer = 0, rate_yaw = 0, rate_pitch = 0;
    int bench = max_frames > 0 && getenv("HYPOCKET_BLOCKS_BENCH") != NULL;
    if (bench && !time_env) day_time = 0.5f; /* the same light on every run */
    const char* lock_env = getenv("HYPOCKET_BLOCKS_TIME_LOCK");
    if (lock_env) time_lock = fmodf(fabsf((float)atof(lock_env)), 1.0f);
    const float bench_x = player.x, bench_y = player.y + 10, bench_z = player.z;
    double stats_wait = 0, bench_start = 0, bench_work = 0, bench_wait = 0;
    int bench_frames = 0;
    const int bench_warmup = 60; /* frames left out: shaders, caches, ramp-up */
    int step_variant = 0, was_in_water = 0, jump_was = 0, sheltered = 0, aiming_note = 0;
    double last_save = last;
    if (bench) save_path = NULL; /* the benchmark never touches saves */

    while (!io->quit && (max_frames == 0 || frames < max_frames)) {
        /* Native ways: input and window events; a window can change size. */
        if (platform && platform->frame) {
            int nw = w, nh = h;
            platform->frame(io, &nw, &nh);
            if ((nw != w || nh != h) && nw >= 64 && nh >= 64 && nw <= 8192 && nh <= 8192) {
                w = nw;
                h = nh;
                delete_target(fbo);
                if (!create_target(w, h, &fbo)) break;
                if (scene_fbo) {
                    delete_target(scene_fbo);
                    if (!create_target(w, h, &scene_fbo)) scene_fbo = 0, dynamic = 0;
                }
                p_glUseProgram(sky_program);
                p_glUniform1f(p_glGetUniformLocation(sky_program, "aspect"), (float)w / h);
                p_glUseProgram(hud_program);
                p_glUniform2f(p_glGetUniformLocation(hud_program, "pixel"), 2.0f / w * (h / 540.0f), 2.0f / h * (h / 540.0f));
                io->width = w;
                io->height = h;
            }
        }
        double frame_start = now_ms();
        float dt = (float)((frame_start - last) / 1000.0);
        last = frame_start;
        if (dt > 0.1f) dt = 0.1f;

        /* ---- Settings and pause ---- */
        int paused = io->paused != 0;
        if (was_paused && !paused) stop_speech(); /* the note was closed */
        was_paused = paused;
        int view = io->view_distance;
        far = view > 0 ? (view < 32 ? 32.0f : view > 160 ? 160.0f : (float)view) : default_far;
        int volume = io->volume_percent;
        audio.volume = (volume < 0 ? 0 : volume > 100 ? 100 : volume) / 100.0f;
        int fps_setting = io->fps_setting;
        fps_limit = fps_setting == 0 ? launch_fps_limit : fps_setting < 0 ? 0 : fps_setting < 15 ? 15 : fps_setting;
        target_fps = fps_limit > 0 ? fps_limit : 60;
        if (paused) {
            fps_limit = 30; /* the world stands still: save the battery */
            dt = 0;
        }

        /* ---- Input and movement ---- */
        float mx = io->move_x, my = io->move_y;
        if (max_frames) { /* walk and turn on its own when testing */
            my = 1;
            base_yaw -= 0.4f * dt;
        }
        /* Touch adds look angles; a controller stick turns at a rate. */
        rate_yaw += io->look_rate_yaw * dt;
        rate_pitch += io->look_rate_pitch * dt;
        player.yaw = io->look_yaw - base_yaw + rate_yaw;
        player.pitch = io->look_pitch - base_pitch - 0.25f + rate_pitch;
        if (player.pitch > 1.5f) { rate_pitch -= player.pitch - 1.5f; player.pitch = 1.5f; }
        if (player.pitch < -1.5f) { rate_pitch -= player.pitch + 1.5f; player.pitch = -1.5f; }
        /* Held by a night creature: no walking, and each new press of
         * jump is a struggle to get free. */
        int jump_held = io->jump != 0, jump_pressed = jump_held && !jump_was;
        jump_was = jump_held;
        if (grab_by >= 0) {
            mx = my = 0;
            jump_held = 0;
            if (jump_pressed && !paused) grab_escape += 0.13f;
        }
        float len = sqrtf(mx * mx + my * my);
        if (len > 1) { mx /= len; my /= len; }
        float speed = 4.3f;
        float sy = sinf(player.yaw), cy = cosf(player.yaw);
        float vx = (mx * cy + my * sy) * speed, vz = (mx * sy - my * cy) * speed;
        int in_water = block_at((int)floorf(player.x), (int)floorf(player.y + 0.5f), (int)floorf(player.z)) == WATER;
        if ((jump_held || (max_frames && player.on_ground && grab_by < 0)) && (player.on_ground || in_water)) {
            if (player.on_ground && !in_water) play_sound(SND_JUMP, 0.25f, 0, 1.0f);
            player.vy = in_water ? 3.5f : 8.2f;
            player.on_ground = 0;
        }
        /* Climbing out: in the water, or just above it, with a block ahead,
         * jump lifts the player over a one-block bank. */
        if (jump_held && grab_by < 0 && (vx != 0 || vz != 0)) {
            int fx = (int)floorf(player.x), fz = (int)floorf(player.z);
            int wet = block_at(fx, (int)floorf(player.y + 0.1f), fz) == WATER ||
                      block_at(fx, (int)floorf(player.y - 0.4f), fz) == WATER;
            float vl = sqrtf(vx * vx + vz * vz);
            if (wet && collides(player.x + vx / vl * 0.2f, player.y + 0.05f, player.z + vz / vl * 0.2f) &&
                player.vy < 7.0f)
                player.vy = 7.0f;
        }
        if (in_water && !was_in_water && player.vy < -2) play_sound(SND_SPLASH, 0.6f, 0, 1.0f);
        was_in_water = in_water;
        float fall_speed = -player.vy;
        int was_on_ground = player.on_ground;
        player.vy -= (in_water ? 10.0f : 26.0f) * dt;
        if (player.vy < -40) player.vy = -40;
        if (in_water) { vx *= 0.6f; vz *= 0.6f; if (player.vy < -3) player.vy = -3; }
        move_player(&player, vx * dt, player.vy * dt, vz * dt);
        int damage = 0;
        if (player.y < -10) { player.y = WY; player.vy = 0; }
        /* Falls of more than about four blocks hurt. */
        if (!creative && player.on_ground && !was_on_ground && !in_water && fall_speed > 13.5f)
            damage += 1 + (int)((fall_speed - 13.5f) / 2.2f);
        /* Footsteps while walking on the ground, and a thud on landing. */
        float ground_speed = sqrtf(vx * vx + vz * vz);
        if (player.on_ground && !was_on_ground && fall_speed > 6) {
            play_sound(SND_STEP, 0.55f, 0, 0.8f);
            step_timer = 0.3f;
        } else if (player.on_ground && !in_water && ground_speed > 0.5f) {
            step_timer -= dt * ground_speed / 4.3f;
            if (step_timer <= 0) {
                step_variant ^= 1;
                play_sound(step_variant ? SND_STEP : SND_STEP2, 0.32f, step_variant ? -0.15f : 0.15f,
                           0.9f + 0.2f * rand01(frames, 1, 2));
                step_timer = 0.38f;
            }
        }

        if (bench) { /* hover and turn: the same views on every run */
            player.x = bench_x;
            player.y = bench_y;
            player.z = bench_z;
            player.vy = 0;
            player.yaw = 6.2831853f * frames / max_frames;
            player.pitch = -0.35f;
        }

        /* ---- Break and place ---- */
        int breaks = io->breaks, places = io->places;
        if (breaks != seen_breaks || places != seen_places) {
            int hit[3], before[3];
            int block_hit = raycast(&player, hit, before);
            /* The break action hits a creature under the crosshair if it is
             * nearer than the block. */
            float ex = player.x, ey = player.y + EYE_HEIGHT, ez = player.z;
            float rdx = cosf(player.pitch) * sinf(player.yaw), rdy = sinf(player.pitch), rdz = -cosf(player.pitch) * cosf(player.yaw);
            float creature_t, block_t = 1e9f;
            int is_night_kind = 0, target = -1;
            if (block_hit) {
                float bx = hit[0] + 0.5f - ex, by = hit[1] + 0.5f - ey, bz = hit[2] + 0.5f - ez;
                block_t = sqrtf(bx * bx + by * by + bz * bz) - 0.5f;
            }
            /* A note under the crosshair, nearer than the block, is picked up and read. */
            {
                float o[3] = {ex, ey, ez}, d[3] = {rdx, rdy, rdz}, note_t, face_t = 1e9f;
                if (block_hit) {
                    float lo[3] = {(float)hit[0], (float)hit[1], (float)hit[2]}, hi[3] = {hit[0] + 1.0f, hit[1] + 1.0f, hit[2] + 1.0f};
                    if (!ray_box(o, d, lo, hi, 10.0f, &face_t)) face_t = 1e9f;
                }
                int note = note_on_ray(o, d, 3.5f, &note_t);
                if (note >= 0 && note_t < face_t) {
                    io->note_id = note;
                    io->notes_opened++;
                    if (!(goal_flags & (1u << (NOTE_SHIFT + note)))) {
                        goal_flags |= 1u << (NOTE_SHIFT + note);
                        world_changed = 1;
                    }
                    play_sound(SND_PLACE, 0.4f, 0, 1.8f);
                    printf("blocks: note %d opened\n", note);
                    read_note_aloud(note);
                    block_hit = 0;
                    breaks = seen_breaks; /* nothing else happens with this press */
                    places = seen_places;
                }
                /* Something lying on the ground, nearer than the block, is picked up. */
                int pick = away || (breaks == seen_breaks && places == seen_places) ? -1 : pickup_on_ray(o, d, 3.5f, &note_t);
                if (pick >= 0 && note_t < face_t) {
                    Pickup* k = &pickups[pick];
                    take_pickup(k);
                    if (!creative) give(k->item, 1);
                    io->picked = palette_index(k->item);
                    io->picks++;
                    world_changed = 1;
                    play_sound(SND_PLACE, 0.4f, 0, 1.6f);
                    block_hit = 0;
                    breaks = seen_breaks;
                    places = seen_places;
                }
            }
            /* Place with food chosen eats it, wherever the player looks. */
            if (places != seen_places && !creative) {
                int selected = io->selected;
                int thing = selected >= 0 && selected < PALETTE_SIZE ? palette[selected] : AIR;
                if (thing >= ITEM_BASE) {
                    if (food_value(thing) > 0 && inventory[thing] > 0 && food < MAX_FOOD - 0.25f) {
                        inventory[thing]--;
                        food = fminf(MAX_FOOD, food + food_value(thing));
                        io->eats++;
                        world_changed = 1;
                        play_sound(SND_STEP2, 0.5f, 0, 1.7f);
                        play_sound(SND_STEP, 0.4f, 0, 1.9f);
                    }
                    block_hit = 0; /* items are never placed */
                    places = seen_places;
                }
            }
            if (breaks != seen_breaks) target = creature_on_ray(ex, ey, ez, rdx, rdy, rdz, 3.5f, &creature_t, &is_night_kind);
            if (target >= 0 && creature_t < block_t) {
                Creature* c = is_night_kind ? &night[target] : &creatures[target];
                const float* col = is_night_kind ? night_color : creature_colors[c->color];
                spawn_puff(c->x, c->y + (is_night_kind ? 1.0f : 0.45f), c->z, col, 6, (uint32_t)frames);
                play_at(SND_BREAK, &player, c->x, c->y + 0.5f, c->z, is_night_kind ? 0.7f : 1.5f);
                int blades = inventory[KNIFE] || inventory[AXE];
                if (is_night_kind && grab_by == target) {
                    grab_escape += 0.22f; /* hitting the one that holds you loosens its grip */
                } else if (!is_night_kind && !creative && (c->hp += 1) >= (blades ? 2 : 3)) {
                    /* Hunted: meat, and with a knife also fat and hide or wool. */
                    c->alive = 0;
                    spawn_puff(c->x, c->y + 0.45f, c->z, col, 14, (uint32_t)frames + 3);
                    give(RAW_MEAT, 2);
                    if (inventory[KNIFE]) {
                        give(FAT, 1);
                        give(c->color % 2 ? WOOL : HIDE, 1);
                    }
                    io->picked = palette_index(RAW_MEAT);
                    io->picks++;
                    world_changed = 1;
                } else {
                    /* A night creature is only dazed: they cannot be killed. */
                    scare(c, player.x, player.z, is_night_kind ? 1.5f : 3.0f);
                    if (is_night_kind) c->stun = 2.5f;
                    c->vy = 4.0f; /* knocked back */
                    c->on_ground = 0;
                }
                block_hit = 0; /* the blow went to the creature */
            }
            int target_block = block_hit ? block_at(hit[0], hit[1], hit[2]) : AIR;
            if (block_hit && (target_block == BEACON_OFF || target_block == BEACON_ON)) {
                /* Beacons cannot be broken; placing on a dark one lights it. */
                int can = creative || (inventory[LAMP] >= BEACON_LAMPS && inventory[GLASS] >= BEACON_GLASS);
                if (places != seen_places && target_block == BEACON_OFF && can) {
                    if (!creative) {
                        inventory[LAMP] -= BEACON_LAMPS;
                        inventory[GLASS] -= BEACON_GLASS;
                    }
                    set_block(hit[0], hit[1], hit[2], BEACON_ON);
                    static const float beam[3] = {0.75f, 0.97f, 1.0f};
                    spawn_puff(hit[0] + 0.5f, hit[1] + 0.8f, hit[2] + 0.5f, beam, 24, (uint32_t)frames);
                    play_at(SND_PLACE, &player, hit[0] + 0.5f, hit[1] + 0.5f, hit[2] + 0.5f, 0.6f);
                    play_sound(SND_CHIRP, 0.5f, 0, 1.2f);
                    for (int k = 0; k < BEACONS; k++) /* its rift seals */
                        if (beacon_x[k] == hit[0] && beacon_z[k] == hit[2]) {
                            static const float seal[3] = {0.7f, 0.6f, 0.9f};
                            spawn_puff(rift_x[k], rift_y[k] + 1.1f, rift_z[k], seal, 20, (uint32_t)frames + 5);
                        }
                    for (int i = 0; i < MAX_NIGHT; i++) /* the ones nearby flee into smoke */
                        if (night[i].alive && calm_at(night[i].x, night[i].z)) night[i].check = 0;
                    printf("blocks: beacon lit, %d of %d\n", beacons_lit(), beacons_present());
                    if (beacons_lit() == beacons_present() && !(goal_flags & GOAL_DONE)) {
                        goal_flags |= GOAL_DONE;
                        done_seconds = play_seconds;
                        printf("blocks: every beacon is lit after %.0f s\n", play_seconds);
                    }
                } else {
                    play_at(SND_STEP, &player, hit[0] + 0.5f, hit[1] + 0.5f, hit[2] + 0.5f, 0.5f); /* a dull knock */
                }
                block_hit = 0;
            }
            if (block_hit) {
                if (breaks != seen_breaks) {
                    int broken = block_at(hit[0], hit[1], hit[2]), drop = drop_for(broken), tool;
                    int need = blows_for(broken, &tool);
                    if (hit[0] != dig_x || hit[1] != dig_y || hit[2] != dig_z) dig_blows = 0;
                    dig_x = hit[0]; dig_y = hit[1]; dig_z = hit[2]; dig_need = need;
                    if (need == 0) { /* too hard by hand: a dull knock, and the app says what it needs */
                        play_at(SND_STEP, &player, hit[0] + 0.5f, hit[1] + 0.5f, hit[2] + 0.5f, 0.5f);
                        io->needed = palette_index(tool);
                        io->needs++;
                    } else if (++dig_blows < need) { /* a blow: chips fly */
                        float chip[3];
                        block_color(broken, chip);
                        spawn_puff(hit[0] + 0.5f, hit[1] + 0.5f, hit[2] + 0.5f, chip, 3, (uint32_t)frames);
                        play_at(SND_BREAK, &player, hit[0] + 0.5f, hit[1] + 0.5f, hit[2] + 0.5f, 0.6f + 0.1f * dig_blows);
                    } else {
                        dig_blows = 0;
                        if (!creative && drop != AIR && inventory[drop] < MAX_CARRIED) inventory[drop]++;
                        if (!creative) extra_drops(broken, hit[0], hit[1], hit[2]);
                        spawn_particles(hit[0], hit[1], hit[2], broken, (uint32_t)frames);
                        play_at(SND_BREAK, &player, hit[0] + 0.5f, hit[1] + 0.5f, hit[2] + 0.5f,
                                0.9f + 0.2f * rand01(hit[0], hit[1], hit[2]));
                        set_block(hit[0], hit[1], hit[2], AIR);
                        flood_from(hit[0], hit[1], hit[2]);
                    }
                } else if (block_at(before[0], before[1], before[2]) == AIR || block_at(before[0], before[1], before[2]) == WATER) {
                    int selected = io->selected;
                    if (selected < 0 || selected >= PALETTE_SIZE) selected = 0;
                    int block = palette[selected], was = block_at(before[0], before[1], before[2]);
                    if (creative || inventory[block] > 0) {
                        set_block(before[0], before[1], before[2], block);
                        if (collides(player.x, player.y, player.z)) {
                            set_block(before[0], before[1], before[2], was); /* would trap the player */
                        } else {
                            if (!creative) inventory[block]--;
                            play_at(SND_PLACE, &player, before[0] + 0.5f, before[1] + 0.5f, before[2] + 0.5f, 1.0f);
                        }
                    }
                }
            }
            seen_breaks = io->breaks;
            seen_places = io->places;
        }
        if (!bench) {
            if (time_lock < 0) day_time += dt / day_seconds;
            if (day_time >= 1.0f) { day_time -= 1.0f; day_count++; }
        }
        Sky sk = away ? other_sky() : sky_at(shown_time());
        /* Inside a lit beacon's sanctuary the night is lit by its pale light. */
        float sanctuary = sanctuary_at(player.x, player.z);
        if (sanctuary > 0) {
            static const float beacon_light[3] = {0.46f, 0.53f, 0.62f}, beacon_sky[3] = {0.16f, 0.21f, 0.27f};
            for (int k = 0; k < 3; k++) {
                sk.ambient[k] = fmaxf(sk.ambient[k], beacon_light[k] * sanctuary);
                sk.sky[k] = fmaxf(sk.sky[k], beacon_sky[k] * sanctuary);
            }
        }
        float sky[3] = {sk.sky[0], sk.sky[1], sk.sky[2]};
        if (!paused) play_seconds += dt;
        /* The mist near unlit beacons: a thicker, closer fog in a dim violet. */
        /* A shelter keeps the mist outside. */
        float mist = sheltered ? 0 : mist_at(player.x, player.z), fog_end = far;
        if (mist > 0) {
            float bright = (sk.ambient[0] + sk.ambient[1] + sk.ambient[2]) / 3;
            static const float haze[3] = {0.56f, 0.50f, 0.68f};
            for (int k = 0; k < 3; k++) sky[k] = mixf(sky[k], haze[k] * (0.2f + 0.8f * bright), mist * 0.85f);
            fog_end = mixf(far, 26.0f, mist * 0.8f);
        }
        if (away) fog_end = 20.0f;
        /* The goal, for the HUD's compass. */
        {
            float d;
            int k = nearest_unlit(player.x, player.z, &d);
            io->beacons = beacons_present();
            io->beacons_lit = beacons_lit();
            io->beacon_distance = k >= 0 ? (int)d : 0;
            io->beacon_angle = k >= 0 ? (int)(wrap_angle(atan2f(beacon_x[k] + 0.5f - player.x, -(beacon_z[k] + 0.5f - player.z)) -
                                                         player.yaw) * 57.29578f)
                                      : 0;
            io->goal_done = (goal_flags & GOAL_DONE) != 0;
            io->play_seconds = (int)play_seconds;
            io->done_seconds = (int)done_seconds;
            io->mist_percent = (int)(mist * 100);
            int aim[3], aim_before[3];
            int aimed = raycast(&player, aim, aim_before);
            io->aiming_beacon = aimed && block_at(aim[0], aim[1], aim[2]) == BEACON_OFF;
            float o[3] = {player.x, player.y + EYE_HEIGHT, player.z};
            float dir[3] = {cosf(player.pitch) * sinf(player.yaw), sinf(player.pitch), -cosf(player.pitch) * cosf(player.yaw)};
            float note_t, block_t = 1e9f;
            if (aimed) {
                float lo[3] = {(float)aim[0], (float)aim[1], (float)aim[2]}, hi[3] = {aim[0] + 1.0f, aim[1] + 1.0f, aim[2] + 1.0f};
                if (!ray_box(o, dir, lo, hi, 10.0f, &block_t)) block_t = 1e9f;
            }
            aiming_note = note_on_ray(o, dir, 3.5f, &note_t) >= 0 && note_t < block_t;
            aiming_pickup = !away && pickup_on_ray(o, dir, 3.5f, &note_t) >= 0 && note_t < block_t;
            /* Looking away from a block that was being dug starts it over. */
            if (!aimed || aim[0] != dig_x || aim[1] != dig_y || aim[2] != dig_z) dig_blows = 0;
            io->dig_percent = dig_blows > 0 && dig_need > 0 ? dig_blows * 100 / dig_need : 0;
            io->notes_found = (int)(goal_flags >> NOTE_SHIFT);
        }
        /* ---- Recipes ---- */
        int crafts = io->crafts;
        if (crafts != seen_crafts) {
            uint8_t recipe[8];
            memcpy(recipe, io->craft, sizeof recipe);
            int times = crafts - seen_crafts, made = 0;
            if (times < 1 || times > 64) times = 1;
            int here = stations_near(player.x, player.y + 0.5f, player.z); /* now, not at the last check */
            for (int t = 0; t < times && !creative; t++) made += craft(recipe, here);
            if (made) {
                play_sound(SND_PLACE, 0.35f, 0, 1.4f);
                world_changed = 1; /* the inventory is in the save */
            }
            seen_crafts = crafts;
        }
        int is_night = sk.sun_dir[1] < -0.04f;
        {
            static float shelter_timer;
            if ((shelter_timer -= dt) <= 0 || frames == 0) {
                shelter_timer = 0.25f;
                sheltered = !bench && shelter_at(player.x, player.y, player.z);
                stations = stations_near(player.x, player.y + 0.5f, player.z);
            }
        }
        if (survival && !away && pickups_day != (int)day_count) place_pickups(spawn_x, spawn_z);
        if (!paused) {
            update_creatures(dt, (uint32_t)frames, player.x, player.z);
            if (!away) respawn_creatures(dt, is_night, player.x, player.z, (uint32_t)frames);
            if (update_night(dt, (uint32_t)frames, is_night, player.x, player.y, player.z, sheltered)) {
                io->hurts++; /* the screen flashes */
                play_sound(SND_STEP, 0.9f, 0, 0.45f);
                printf("blocks: grabbed by a night creature\n");
            }
            if (grab_grace > 0) grab_grace -= dt;
        }

        /* ---- The grab ---- */
        int taken = 0;
        if (grab_by >= 0 && !paused) {
            Creature* g = &night[grab_by];
            grab_time += dt;
            grab_escape -= 0.04f * dt; /* the grip tightens again */
            if (grab_escape < 0) grab_escape = 0;
            /* Pulled along behind it: the player follows its path, hopping up
             * where it climbs. Caught on a wall, the grip slips. */
            float tx = g->x - sinf(g->yaw) * 0.8f, tz = g->z + cosf(g->yaw) * 0.8f;
            float gx = tx - player.x, gz = tz - player.z, gap = sqrtf(gx * gx + gz * gz);
            float pull = dt * 6 < 1 ? dt * 6 : 1;
            move_player(&player, gx * pull, 0, gz * pull);
            if (player.on_ground && (g->y > player.y + 0.5f || gap > 1.3f)) { player.vy = 7.5f; player.on_ground = 0; }
            if (gap > 3.0f) { /* the player caught on something: it comes back for them */
                g->x = player.x + sinf(g->yaw) * 0.8f;
                g->z = player.z - cosf(g->yaw) * 0.8f;
                g->y = player.y;
                drag_yaw += 1.2f;
            }
            /* Light close by (not in the mist) makes it let go. */
            if (mist_at(player.x, player.z) < 0.5f && lamp_near(player.x, player.y + 1, player.z, 3)) grab_escape = 1;
            if (grab_escape >= 1) {
                spawn_puff(g->x, g->y + 1.0f, g->z, night_color, 10, (uint32_t)frames);
                scare(g, player.x, player.z, 3.0f);
                g->stun = 3.0f;
                g->vy = 4.0f;
                g->on_ground = 0;
                g->cooldown = 3.0f;
                grab_by = -1;
                grab_grace = 2.5f;
                play_sound(SND_JUMP, 0.6f, 0, 0.7f);
                printf("blocks: got free\n");
            } else if (grab_time >= (away ? GRAB_SECONDS * 0.66f : GRAB_SECONDS)) {
                taken = 1;
            }
        }
        if (taken && !away) { /* dragged through the mist, to the other side */
            io->takes++;
            health = MAX_HEALTH;
            printf("blocks: the player was taken\n");
            enter_other_side(&player, io->takes);
            world_changed = 1;
        } else if (taken) { /* caught over there: wake at the start of the player's own world */
            io->deaths++;
            health = MAX_HEALTH;
            leave_other_side(&player, 1, spawn_x, spawn_y, spawn_z);
            world_changed = 1;
        }
        if (away && !paused) {
            /* The way back hums: follow it. Walking into its light brings the
             * player back where they were taken. */
            static float hum;
            if ((hum -= dt) <= 0) {
                hum = 2.2f;
                play_at(SND_CHIRP, &player, exit_x, exit_y + 1, exit_z, 0.45f);
            }
            float dx = exit_x - player.x, dz = exit_z - player.z;
            if (dx * dx + dz * dz < 1.0f && fabsf(player.y - exit_y) < 2.5f && grab_by < 0) {
                leave_other_side(&player, 0, spawn_x, spawn_y, spawn_z);
                play_sound(SND_CHIRP, 0.7f, 0, 1.3f);
                world_changed = 1;
            }
        } else if (portal_here && grab_by < 0 && grab_grace <= 0 && !paused) {
            /* Walking into the portal: the other side, on purpose. */
            float dx = portal_x - player.x, dz = portal_z - player.z;
            if (dx * dx + dz * dz < 0.8f && fabsf(player.y - portal_y) < 2.0f) {
                play_sound(SND_CHIRP, 0.7f, 0, 0.5f);
                enter_other_side(&player, io->takes + (int)day_count + 1);
                world_changed = 1;
            }
        }
        io->grab_percent = grab_by >= 0 ? (int)(grab_escape * 100) : 0;

        /* ---- Hunger and cold ---- */
        float wear = 0; /* health lost per second to hunger and cold */
        if (survival && !paused) {
            /* Warmth: the night, water, the mist and the other side take it;
             * the day, a shelter and a fire nearby give it back. A coat
             * keeps most of it in. */
            float mist_here = sheltered ? 0 : mist_at(player.x, player.z);
            if (stations) cold_rate = 0.6f;
            else if (in_water) cold_rate = -0.12f;
            else if (away) cold_rate = -0.05f;
            else if (is_night) cold_rate = sheltered || sanctuary > 0.5f ? 0.08f : -(0.045f + 0.02f * mist_here);
            else cold_rate = mist_here > 0.5f ? -0.02f : 0.15f;
            if (cold_rate < 0 && inventory[COAT]) cold_rate *= 0.35f;
            warmth = fminf(MAX_WARMTH, fmaxf(0, warmth + cold_rate * dt));
            /* A full belly lasts a little more than a day; cold makes it empty faster. */
            float hunger = MAX_FOOD / (day_seconds * 1.2f) * (warmth <= 0 ? 2.0f : warmth < 3 ? 1.5f : 1.0f);
            food = fmaxf(0, food - hunger * dt);
            if (food <= 0) wear += 1 / 8.0f;
            if (warmth <= 0) wear += 1 / 10.0f;
            health -= wear * dt;
        }
        /* ---- Health ---- */
        if ((damage > 0 && !creative) || health <= 0) {
            if (damage > 0) {
                health -= damage;
                calm = 6;
                io->hurts++;
                play_sound(SND_STEP, 0.9f, 0, 0.55f);
            }
            if (health <= 0) { /* fainted: wake up at the starting point */
                food = fmaxf(food, 4.0f); /* a little strength comes back */
                warmth = fmaxf(warmth, 6.0f);
                io->deaths++;
                health = MAX_HEALTH;
                player.x = spawn_x;
                player.y = spawn_y;
                player.z = spawn_z;
                player.vy = 0;
                for (int i = 0; i < MAX_NIGHT; i++) night[i].alive = 0;
                grab_by = -1;
                printf("blocks: the player fainted and woke up at the start\n");
            }
        } else if (!paused && (calm -= dt) <= 0 && health < MAX_HEALTH && wear <= 0 &&
                   (!survival || food >= MAX_FOOD / 2)) {
            health += dt / 4.0f; /* a point every four seconds, while fed */
            if (survival) food -= dt / 4.0f * 0.3f;
            if (health > MAX_HEALTH) health = MAX_HEALTH;
        }
        saved_health = health;
        io->food_tenths = (int)ceilf(food * 10 - 0.01f);
        io->warmth_tenths = (int)ceilf(warmth * 10 - 0.01f);
        io->survival_flags = (survival ? stations : 0) | (survival && cold_rate < 0 ? SURVIVAL_COLD : 0) |
                             (survival && stations ? SURVIVAL_WARMING : 0);
        io->health_tenths = (int)ceilf(health * 10 - 0.01f);
        io->story_flags = (grab_by >= 0 ? STORY_GRABBED : 0) | (sheltered ? STORY_SHELTERED : 0) |
                          (aiming_note ? STORY_AIM_NOTE : 0) | (away ? STORY_AWAY : 0) |
                          (aiming_pickup ? STORY_AIM_PICKUP : 0);
        update_particles(dt);
        /* Campfires near the player smoke and spark. */
        fire_clock += dt;
        if ((smoke_wait -= dt) <= 0) {
            smoke_wait = 0.25f;
            int pci = (int)player.x / CHUNK, pcj = (int)player.z / CHUNK, smoking = 0;
            float nearest = 1e9f;
            for (int i = pci - 1; i <= pci + 1; i++)
                for (int j = pcj - 1; j <= pcj + 1; j++) {
                    if (i < 0 || j < 0 || i >= CHUNKS || j >= CHUNKS) continue;
                    const Chunk* ch = &chunks[i][j];
                    for (int f = 0; f < ch->fire_count && smoking < 6; f++, smoking++) {
                        const int16_t* fp = ch->fires[f];
                        float fdx = fp[0] + 0.5f - player.x, fdy = fp[1] + 0.5f - player.y, fdz = fp[2] + 0.5f - player.z;
                        nearest = fminf(nearest, sqrtf(fdx * fdx + fdy * fdy + fdz * fdz));
                        uint32_t tick = (uint32_t)frames * 7u + (uint32_t)f;
                        spawn_smoke(fp[0] + 0.5f, fp[1] + 0.7f, fp[2] + 0.5f, 0, tick);
                        if (rand01(fp[0], (int)tick, fp[2]) < 0.3f) spawn_smoke(fp[0] + 0.5f, fp[1] + 0.3f, fp[2] + 0.5f, 1, tick);
                    }
                }
            /* The crackle: full beside a fire, gone 12 blocks away. */
            float near = fmaxf(0, 1 - nearest / 12);
            audio.loops[LOOP_CAMPFIRE].target = near * near * 0.8f;
        }
        /* Chunks whose blocks changed are meshed now; those where only lamp
         * light moved wait their turn, two a frame, to avoid a hitch. */
        int light_only = 2;
        for (int i = 0; i < CHUNKS; i++)
            for (int j = 0; j < CHUNKS; j++) {
                if (!chunks[i][j].dirty || (chunks[i][j].dirty == 1 && light_only-- <= 0)) continue;
                p_glBindVertexArray(chunks[i][j].vao);
                mesh_chunk(i, j);
            }

        /* ---- Draw ---- */
        /* The 3D resolution setting caps the scale; dynamic resolution stays below it. */
        int cap_percent = io->scale_percent_max;
        float cap = cap_percent >= 50 && cap_percent < 100 ? cap_percent / 100.0f : 1.0f;
        if (cap < 1.0f && !scene_fbo && !create_target(w, h, &scene_fbo)) scene_fbo = 0;
        float draw_scale = dynamic ? fminf(scale, cap) : cap;
        int scaled = scene_fbo && draw_scale < 0.999f;
        int sw = scaled ? ((int)(w * draw_scale) & ~1) : w, sh = scaled ? ((int)(h * draw_scale) & ~1) : h;
        p_glBindFramebuffer(GL_FRAMEBUFFER, scaled ? scene_fbo : fbo);
        p_glViewport(0, 0, sw, sh);
        p_glClearColor(sky[0], sky[1], sky[2], 1.0f);
        p_glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        float vp[16];
        view_projection(vp, &player, (float)w / h, far);
        if (sk.sun_color[3] > 0.01f || sk.moon_color[3] > 0.01f) { /* behind everything */
            p_glEnable(GL_BLEND);
            p_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            p_glUseProgram(sky_program);
            p_glUniformMatrix4fv(us_view_proj, 1, GL_FALSE, vp);
            p_glUniform3f(us_sun_dir, sk.sun_dir[0], sk.sun_dir[1], sk.sun_dir[2]);
            p_glUniform4f(us_sun_color, sk.sun_color[0], sk.sun_color[1], sk.sun_color[2], sk.sun_color[3]);
            p_glUniform4f(us_moon_color, sk.moon_color[0], sk.moon_color[1], sk.moon_color[2], sk.moon_color[3]);
            p_glBindVertexArray(hud_vao);
            p_glDrawArrays(GL_TRIANGLES, 0, 12);
            p_glDisable(GL_BLEND);
        }
        p_glEnable(GL_DEPTH_TEST);
        p_glEnable(GL_CULL_FACE);
        p_glUseProgram(world_program);
        p_glUniformMatrix4fv(u_view_proj, 1, GL_FALSE, vp);
        p_glUniform3f(u_ambient, sk.ambient[0], sk.ambient[1], sk.ambient[2]);
        p_glUniform3f(u_eye, player.x, player.y + EYE_HEIGHT, player.z);
        p_glUniform3f(u_fog_color, sky[0], sky[1], sky[2]);
        p_glUniform1f(u_fog_end, fog_end);

        /* Skip chunks beyond the fog or behind the camera. */
        float fx = cosf(player.pitch) * sinf(player.yaw), fz = -cosf(player.pitch) * cosf(player.yaw);
        float flen = sqrtf(fx * fx + fz * fz);
        if (flen > 1e-4f) { fx /= flen; fz /= flen; }
        int visible[CHUNKS * CHUNKS], visible_count = 0, triangles = 0;
        for (int i = 0; i < CHUNKS; i++)
            for (int j = 0; j < CHUNKS; j++) {
                float ccx = i * CHUNK + CHUNK / 2.0f - player.x, ccz = j * CHUNK + CHUNK / 2.0f - player.z;
                float d = sqrtf(ccx * ccx + ccz * ccz);
                const float radius = CHUNK * 0.71f;
                if (d - radius > far) continue;
                if (d > radius * 1.5f && (ccx * fx + ccz * fz) < -radius) continue;
                visible[visible_count++] = i * CHUNKS + j;
            }
        for (int k = 0; k < visible_count; k++) {
            Chunk* ch = &chunks[visible[k] / CHUNKS][visible[k] % CHUNKS];
            if (!ch->quads) continue;
            p_glBindVertexArray(ch->vao);
            p_glUniform3f(u_origin, (float)(visible[k] / CHUNKS * CHUNK), 0, (float)(visible[k] % CHUNKS * CHUNK));
            p_glDrawElements(GL_TRIANGLES, ch->quads * 6, GL_UNSIGNED_INT, 0);
            triangles += ch->quads * 2;
        }
        /* Creatures and particles: the boxes are listed once, then drawn
         * with one instanced draw call, or one draw call each (the load of
         * many separate objects, for the benchmark). */
        int creatures_drawn = 0, n = 0;
        for (int pass = 0; pass < 2; pass++) {
            int count = pass ? MAX_NIGHT : creature_count;
            for (int i = 0; i < count; i++) {
                const Creature* c = pass ? &night[i] : &creatures[i];
                if (!c->alive) continue;
                float dx = c->x - player.x, dz = c->z - player.z, d = sqrtf(dx * dx + dz * dz);
                if (d > far || (d > 4 && dx * fx + dz * fz < -2)) continue;
                float bob = c->on_ground ? fabsf(sinf(c->phase)) * 0.06f : 0;
                float sy = sinf(c->yaw), cy = cosf(c->yaw);
                if (!pass) {
                    const float* col = creature_colors[c->color];
                    float body[INSTANCE_FLOATS] = {c->x, c->y + 0.38f + bob, c->z, 0.32f, 0.26f, 0.42f, c->yaw,
                                                   col[0], col[1], col[2]};
                    float head[INSTANCE_FLOATS] = {c->x + sy * 0.45f, c->y + 0.62f + bob, c->z - cy * 0.45f,
                                                   0.2f, 0.2f, 0.2f, c->yaw, col[0] * 0.8f, col[1] * 0.8f, col[2] * 0.8f};
                    memcpy(instances[n++], body, sizeof body);
                    memcpy(instances[n++], head, sizeof head);
                    triangles += 24;
                } else {
                    /* Tall and dark, with two glowing eyes (colours above 2 glow, see BOX_GLOW). */
                    const float* col = night_color;
                    float hx = c->x + sy * 0.05f, hy = c->y + 1.48f + bob, hz = c->z - cy * 0.05f;
                    float body[INSTANCE_FLOATS] = {c->x, c->y + 0.62f + bob, c->z, 0.3f, 0.62f, 0.24f, c->yaw,
                                                   col[0], col[1], col[2]};
                    float head[INSTANCE_FLOATS] = {hx, hy, hz, 0.26f, 0.24f, 0.26f, c->yaw,
                                                   col[0] * 1.2f, col[1] * 1.2f, col[2] * 1.2f};
                    memcpy(instances[n++], body, sizeof body);
                    memcpy(instances[n++], head, sizeof head);
                    for (int e = -1; e <= 1; e += 2) {
                        float eye[INSTANCE_FLOATS] = {hx + sy * 0.25f + cy * 0.11f * e, hy + 0.04f, hz - cy * 0.25f + sy * 0.11f * e,
                                                      0.075f, 0.055f, 0.03f, c->yaw, 2.85f, 3.0f, 2.55f};
                        memcpy(instances[n++], eye, sizeof eye);
                    }
                    triangles += 48;
                }
                creatures_drawn++;
            }
        }
        /* Rifts beside unlit beacons: a dark tear with glowing edges, leaking mist. */
        static const float rift_glow[3] = {2.62f, 2.42f, 2.95f}, rift_core[3] = {0.04f, 0.02f, 0.07f};
        static float rift_timer;
        int rift_puff = !paused && (rift_timer -= dt) <= 0;
        if (rift_puff) rift_timer = 0.35f;
        for (int k = 0; k < BEACONS; k++) {
            if (beacon_y[k] < 0 || beacon_lit(k)) continue;
            float dx = rift_x[k] - player.x, dz = rift_z[k] - player.z;
            if (dx * dx + dz * dz > far * far) continue;
            float yaw = k * 1.3f, sy = sinf(yaw), cy = cosf(yaw), wob = 0.06f * sinf(frames * 0.07f + k);
            float core[INSTANCE_FLOATS] = {rift_x[k], rift_y[k] + 1.15f, rift_z[k], 0.32f + wob, 0.85f, 0.04f, yaw,
                                           rift_core[0], rift_core[1], rift_core[2]};
            memcpy(instances[n++], core, sizeof core);
            for (int e = -1; e <= 1; e += 2) {
                float edge[INSTANCE_FLOATS] = {rift_x[k] + cy * (0.34f + wob) * e, rift_y[k] + 1.15f, rift_z[k] + sy * (0.34f + wob) * e,
                                               0.035f, 0.8f, 0.05f, yaw, rift_glow[0], rift_glow[1], rift_glow[2]};
                memcpy(instances[n++], edge, sizeof edge);
            }
            triangles += 36;
            if (rift_puff && dx * dx + dz * dz < 40 * 40) {
                static const float haze[3] = {0.62f, 0.52f, 0.8f};
                spawn_puff(rift_x[k], rift_y[k] + 0.6f + rand01(frames, k, 3) * 1.2f, rift_z[k], haze, 1, (uint32_t)(frames + k));
            }
        }
        /* The portal: no frame, just a tall, jagged tear in the air, black
         * inside with pale cracks of light along its edges, stones it tore
         * loose drifting round it, and mist pouring out. */
        if (portal_here) {
            float dx = portal_x - player.x, dz = portal_z - player.z;
            if (dx * dx + dz * dz < far * far) {
                float yaw = portal_yaw, sy = sinf(yaw), cy = cosf(yaw), t = frames * 0.02f;
                static const float tear_jag[7] = {0.35f, -0.2f, 0.25f, 0.0f, 0.3f, -0.15f, 0.2f};
                for (int i = -3; i <= 3; i++) {
                    float h = 1.75f * (1 - fabsf((float)i) / 4.5f) + tear_jag[i + 3] * 0.4f;
                    float cx = portal_x + cy * i * 0.2f, cz = portal_z + sy * i * 0.2f;
                    float cyy = portal_y + 1.9f + tear_jag[(i + 5) % 7] * 0.3f;
                    float wob = 0.03f * sinf(t * 3 + i);
                    /* A pale edge behind (taller, thinner), the black inside in front. */
                    float edge[INSTANCE_FLOATS] = {cx, cyy, cz, 0.14f + wob, h + 0.08f, 0.03f, yaw, 2.78f, 2.74f, 2.86f};
                    float core[INSTANCE_FLOATS] = {cx, cyy, cz, 0.12f + wob, h, 0.05f, yaw, 0.02f, 0.015f, 0.03f};
                    memcpy(instances[n++], edge, sizeof edge);
                    memcpy(instances[n++], core, sizeof core);
                }
                for (int k = 0; k < 6; k++) {
                    float a = t * (0.4f + 0.07f * k) + k * 1.047f, r = 1.6f + 0.25f * (k % 3);
                    float stone[INSTANCE_FLOATS] = {portal_x + cosf(a) * r, portal_y + 0.7f + 0.45f * k + 0.2f * sinf(t + k),
                                                    portal_z + sinf(a) * r, 0.14f + 0.03f * (k % 2), 0.12f, 0.15f, a * 1.7f,
                                                    0.36f, 0.34f, 0.33f};
                    memcpy(instances[n++], stone, sizeof stone);
                }
                triangles += 20 * 12;
                if (rift_puff && dx * dx + dz * dz < 40 * 40) {
                    static const float haze[3] = {0.7f, 0.66f, 0.78f};
                    spawn_puff(portal_x, portal_y + 0.5f + rand01(frames, 7, 3) * 3.0f, portal_z, haze, 2, (uint32_t)frames + 11);
                }
            }
        }
        /* A lit beacon sends a beam of light up: the sanctuary can be seen from afar. */
        for (int k = 0; k < BEACONS; k++) {
            if (!beacon_lit(k)) continue;
            float dx = beacon_x[k] + 0.5f - player.x, dz = beacon_z[k] + 0.5f - player.z;
            if (dx * dx + dz * dz > far * far * 1.5f) continue;
            float beam[INSTANCE_FLOATS] = {beacon_x[k] + 0.5f, beacon_y[k] + 13.0f, beacon_z[k] + 0.5f, 0.14f, 12.0f, 0.14f,
                                           frames * 0.01f, 2.72f, 2.95f, 3.0f};
            memcpy(instances[n++], beam, sizeof beam);
            triangles += 12;
        }
        /* The way back from the other side: a tall white light. */
        if (away) {
            float pulse = 0.12f * sinf(frames * 0.08f);
            float core[INSTANCE_FLOATS] = {exit_x, exit_y + 1.3f, exit_z, 0.45f, 1.3f, 0.45f, frames * 0.01f,
                                           2.85f + pulse, 2.95f + pulse, 3.0f};
            float beam[INSTANCE_FLOATS] = {exit_x, exit_y + 12.0f, exit_z, 0.08f, 10.0f, 0.08f, 0, 2.7f, 2.85f, 3.0f};
            memcpy(instances[n++], core, sizeof core);
            memcpy(instances[n++], beam, sizeof beam);
            triangles += 24;
        }
        /* Notes: a sheet of paper, or a phone whose screen still glows. */
        for (int k = 0; k < NOTES; k++) {
            if (!note_here[k]) continue;
            float dx = note_x[k] - player.x, dz = note_z[k] - player.z;
            if (dx * dx + dz * dz > far * far) continue;
            if (note_kind[k] == NOTE_PAPER) {
                float paper[INSTANCE_FLOATS] = {note_x[k], note_y[k] + 0.012f, note_z[k], 0.2f, 0.012f, 0.27f, note_yaw[k],
                                                0.92f, 0.9f, 0.82f};
                memcpy(instances[n++], paper, sizeof paper);
                triangles += 12;
            } else {
                float body[INSTANCE_FLOATS] = {note_x[k], note_y[k] + 0.02f, note_z[k], 0.07f, 0.02f, 0.13f, note_yaw[k],
                                               0.12f, 0.12f, 0.14f};
                float screen[INSTANCE_FLOATS] = {note_x[k], note_y[k] + 0.042f, note_z[k], 0.055f, 0.003f, 0.11f, note_yaw[k],
                                                 2.45f, 2.7f, 2.95f};
                memcpy(instances[n++], body, sizeof body);
                memcpy(instances[n++], screen, sizeof screen);
                triangles += 24;
            }
        }
        /* Things lying on the ground: sticks, flint, a knife, a saw. */
        for (int i = 0; i < pickup_count && !away; i++) {
            const Pickup* k = &pickups[i];
            float dx = k->x - player.x, dz = k->z - player.z;
            if (pickup_taken(k) || dx * dx + dz * dz > 40 * 40) continue;
            float sy = sinf(k->yaw), cy = cosf(k->yaw);
            if (k->item == STICK) {
                float stick[INSTANCE_FLOATS] = {k->x, k->y + 0.035f, k->z, 0.035f, 0.035f, 0.3f, k->yaw, 0.47f, 0.33f, 0.19f};
                memcpy(instances[n++], stick, sizeof stick);
                triangles += 12;
            } else if (k->item == FLINT) {
                float flint[INSTANCE_FLOATS] = {k->x, k->y + 0.05f, k->z, 0.1f, 0.05f, 0.08f, k->yaw, 0.30f, 0.30f, 0.34f};
                memcpy(instances[n++], flint, sizeof flint);
                triangles += 12;
            } else { /* a knife or a saw: a wooden handle and a metal blade */
                int saw = k->item == SAW;
                float handle[INSTANCE_FLOATS] = {k->x - sy * 0.12f, k->y + 0.025f, k->z + cy * 0.12f, saw ? 0.06f : 0.03f, 0.025f,
                                                 0.07f, k->yaw, 0.45f, 0.30f, 0.17f};
                float blade[INSTANCE_FLOATS] = {k->x + sy * (saw ? 0.2f : 0.08f), k->y + 0.012f, k->z - cy * (saw ? 0.2f : 0.08f),
                                                saw ? 0.1f : 0.025f, 0.008f, saw ? 0.22f : 0.12f, k->yaw, 0.82f, 0.84f, 0.88f};
                memcpy(instances[n++], handle, sizeof handle);
                memcpy(instances[n++], blade, sizeof blade);
                triangles += 24;
            }
        }
        int fires_drawn = 0;
        float fire_glow = fminf(fmaxf((0.6f - sk.ambient[1]) / 0.4f, 0), 1); /* 0 by day, 1 at night */
        for (int k = 0; k < visible_count && fires_drawn < MAX_DRAWN_FIRES; k++) {
            const Chunk* ch = &chunks[visible[k] / CHUNKS][visible[k] % CHUNKS];
            for (int f = 0; f < ch->fire_count && fires_drawn < MAX_DRAWN_FIRES; f++) {
                const int16_t* fp = ch->fires[f];
                float dx = fp[0] + 0.5f - player.x, dz = fp[2] + 0.5f - player.z;
                if (dx * dx + dz * dz > 48 * 48) continue;
                n = draw_campfire(instances, n, fp[0], fp[1], fp[2], fire_clock, fire_glow);
                fires_drawn++;
                triangles += CAMPFIRE_BOXES * 12;
            }
        }
        for (int i = 0; i < MAX_PARTICLES; i++) {
            const Particle* pt = &particles[i];
            if (pt->life <= 0) continue;
            float box[INSTANCE_FLOATS] = {pt->x, pt->y, pt->z, pt->size, pt->size, pt->size, 0,
                                          pt->color[0], pt->color[1], pt->color[2]};
            memcpy(instances[n++], box, sizeof box);
            triangles += 12;
        }
        int night_near = 0;
        for (int i = 0; i < MAX_NIGHT; i++) {
            float dx = night[i].x - player.x, dz = night[i].z - player.z;
            night_near += night[i].alive && dx * dx + dz * dz < 24 * 24;
        }
        io->night_near = night_near;
        if (separate_draws && n > 0) {
            p_glUseProgram(box_program);
            p_glUniformMatrix4fv(ub_view_proj, 1, GL_FALSE, vp);
            p_glUniform3f(ub_eye, player.x, player.y + EYE_HEIGHT, player.z);
            p_glUniform3f(ub_fog_color, sky[0], sky[1], sky[2]);
            p_glUniform1f(ub_fog_end, fog_end);
            p_glUniform3f(ub_ambient, sk.ambient[0], sk.ambient[1], sk.ambient[2]);
            p_glBindVertexArray(hud_vao); /* no attributes: corners come from gl_VertexID */
            for (int k = 0; k < n; k++) {
                const float* b = instances[k];
                p_glUniform3f(ub_center, b[0], b[1], b[2]);
                p_glUniform3f(ub_half, b[3], b[4], b[5]);
                p_glUniform1f(ub_yaw, b[6]);
                p_glUniform3f(ub_color, b[7], b[8], b[9]);
                p_glDrawArrays(GL_TRIANGLES, 0, 36);
            }
        } else if (n > 0) {
            p_glUseProgram(inst_program);
            p_glUniformMatrix4fv(ui_view_proj, 1, GL_FALSE, vp);
            p_glUniform3f(ui_eye, player.x, player.y + EYE_HEIGHT, player.z);
            p_glUniform3f(ui_fog_color, sky[0], sky[1], sky[2]);
            p_glUniform1f(ui_fog_end, fog_end);
            p_glUniform3f(ui_ambient, sk.ambient[0], sk.ambient[1], sk.ambient[2]);
            p_glBindVertexArray(inst_vao);
            p_glBindBuffer(GL_ARRAY_BUFFER, inst_vbo);
            /* A fresh buffer each frame, so the GPU never waits for the last one. */
            p_glBufferData(GL_ARRAY_BUFFER, sizeof instances, NULL, GL_STREAM_DRAW);
            p_glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)n * sizeof instances[0], instances);
            p_glDrawArraysInstanced(GL_TRIANGLES, 0, 36, n);
        }
        p_glUseProgram(world_program);

        /* Water last, blended, without writing depth. */
        p_glEnable(GL_BLEND);
        p_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        p_glDepthMask(GL_FALSE);
        p_glDisable(GL_CULL_FACE);
        for (int k = 0; k < visible_count; k++) {
            Chunk* ch = &chunks[visible[k] / CHUNKS][visible[k] % CHUNKS];
            if (!ch->water_quads) continue;
            p_glBindVertexArray(ch->vao);
            p_glUniform3f(u_origin, (float)(visible[k] / CHUNKS * CHUNK), 0, (float)(visible[k] % CHUNKS * CHUNK));
            p_glDrawElements(GL_TRIANGLES, ch->water_quads * 6, GL_UNSIGNED_INT,
                             (void*)((size_t)ch->quads * 6 * sizeof(uint32_t)));
            triangles += ch->water_quads * 2;
        }
        p_glDepthMask(GL_TRUE);
        p_glDisable(GL_DEPTH_TEST);
        p_glDisable(GL_BLEND);
        if (scaled) {
            p_glBindFramebuffer(GL_READ_FRAMEBUFFER, scene_fbo);
            p_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
            p_glBlitFramebuffer(0, 0, sw, sh, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_LINEAR);
            p_glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            p_glViewport(0, 0, w, h);
        }
        p_glEnable(GL_BLEND);
        p_glUseProgram(hud_program);
        p_glBindVertexArray(hud_vao);
        p_glDrawArrays(GL_TRIANGLES, 0, 12);
        p_glDisable(GL_BLEND);

        /* A small picture of the world for the world list: soon after it
         * starts, every 30 seconds and when leaving. */
        if (save_path && (io->quit || frames == 30 || frames % 1800 == 0)) save_thumbnail(save_path, window_target ? 0 : fbo, w, h);

        if (present) {
            if (!window_target) {
                p_glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
                p_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
                p_glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
            }
#ifdef BLOCKS_LIBRARY
            platform->present();
#else
            eglSwapBuffers(display, surface);
#endif
        } else if (header) {
            double read_start = now_ms(); /* reading back waits for the GPU too */
            p_glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
            wait_total += now_ms() - read_start;
            __atomic_store_n(&header[3], (uint32_t)frames + 1, __ATOMIC_RELEASE);
        }
        /* Keep at most one frame queued: wait for the previous one, not this
         * one, so the CPU prepares the next frame while the GPU draws. */
        GLsync fence = p_glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        double wait_start = now_ms();
        if (previous) {
            p_glClientWaitSync(previous, GL_SYNC_FLUSH_COMMANDS_BIT, 100000000ull);
            p_glDeleteSync(previous);
        }
        previous = fence;
        wait_total += now_ms() - wait_start;

        double frame_end = now_ms();
        work_total += frame_end - frame_start;
        window_work += frame_end - frame_start;
        window_wait += wait_total;
        stats_wait += wait_total;
        if (bench && frames >= bench_warmup) {
            if (bench_frames == 0) bench_start = frame_start;
            bench_frames++;
            bench_work += frame_end - frame_start;
            bench_wait += wait_total;
        }
        wait_total = 0;
        window_frames++;
        if (paused) { /* paused frames say nothing about the GPU */
            window_start = frame_end;
            window_work = window_wait = 0;
            window_frames = 0;
        }
        if (dynamic && frame_end - window_start >= 500.0) {
            double work = window_work / window_frames, wait = window_wait / window_frames;
            double fps_now = window_frames * 1000.0 / (frame_end - window_start);
            /* Lower the resolution only when the game misses its frame rate and
             * the time goes into waiting for the GPU (if the CPU is the slow
             * part, fewer pixels would not help). Frames that merely come close
             * to the budget are fine. The first two seconds are skipped: the
             * start is always slow. */
            if (frame_end - run_start < 2000.0) {
                /* warming up */
            } else if (probing) {
                /* We raised the scale to see whether pixels are what limits
                 * the frame rate: keep it if the frame rate held. */
                if (++probe_windows >= 3) {
                    if (fps_now < probe_fps * 0.97) {
                        scale -= 0.1f;
                        cooldown_windows = 20;
                    } else {
                        no_lower_until = frame_end + 30000.0; /* pixels are not the limit */
                    }
                    probing = 0;
                }
            } else if (fps_now < 0.92 * target_fps && wait > 0.25 * work && scale > 0.5f &&
                       frame_end > no_lower_until) {
                /* A single slow half second is a hiccup, not a trend. */
                if (++missed_windows >= 2 || fps_now < 0.75 * target_fps) {
                    scale -= fps_now < 0.75 * target_fps ? 0.1f : 0.05f;
                    if (scale < 0.5f) scale = 0.5f;
                    cooldown_windows = 8; /* wait 4 s before trying higher again */
                    missed_windows = 0;
                }
                calm_windows = 0;
                stuck_windows = 0;
            } else if (fps_now >= 0.97 * target_fps) {
                missed_windows = 0;
                stuck_windows = 0;
                if (cooldown_windows > 0) {
                    cooldown_windows--;
                } else if (++calm_windows >= 4 && scale < 1.0f) {
                    scale += 0.05f;
                    if (scale > 1.0f) scale = 1.0f;
                    calm_windows = 0;
                }
            } else {
                /* Below the target but not lowering (at the floor, cooling down,
                 * or a borderline rate): if this lasts, lower resolution is not
                 * helping, so try higher and see. */
                calm_windows = 0;
                missed_windows = 0;
                if (cooldown_windows > 0) {
                    cooldown_windows--;
                } else if (scale < 1.0f && ++stuck_windows >= 20) {
                    probe_fps = fps_now;
                    scale += 0.1f;
                    if (scale > 1.0f) scale = 1.0f;
                    probing = 1;
                    probe_windows = 0;
                    stuck_windows = 0;
                }
            }
            window_start = frame_end;
            window_work = window_wait = 0;
            window_frames = 0;
        }
        io->scale_percent = (int)((dynamic ? fminf(scale, cap) : cap) * 100.0f + 0.5f);
        io->creatures = creatures_drawn;
        io->clock_minutes = (int)(shown_time() * 1440.0f) % 1440;
        for (int k = 0; k < PALETTE_SIZE; k++) {
            io->all_counts[k] = (int16_t)inventory[palette[k]];
            if (k < 16) io->counts[k] = io->all_counts[k];
        }
        io->day = (int)day_count + 1;
        frames++;
        stats_frames++;
        io->frames = frames;
        io->triangles = triangles;
        io->chunks = visible_count;
        if (frames == 1) {
            io->ready = 1;
            printf("blocks: first frame shown\n");
            fflush(stdout);
        }
        if (frame_end - stats_start >= 1000.0) {
            io->fps = (float)(stats_frames * 1000.0 / (frame_end - stats_start));
            io->frame_ms = (float)(work_total / stats_frames);
            log_frames += stats_frames;
            log_ms += frame_end - stats_start;
            log_work += work_total;
            log_wait += stats_wait;
            if (io->fps < log_low_fps) log_low_fps = io->fps;
            /* Printed every second at first, then every 10 seconds with the
             * slowest second in between: a long session must not fill the
             * app's limited copy of this output. */
            if (log_ms >= (frame_end - run_start < 30000.0 ? 1000.0 : 10000.0)) {
                /* cpu: the game's own work per frame; wait: for the GPU and server. */
                printf("blocks: %.1f fps (lowest %.1f), %.1f ms/frame (cpu %.1f, wait %.1f), %d triangles, %d chunks, "
                       "scale %d%%, %d creatures\n",
                       log_frames * 1000.0 / log_ms, log_low_fps, log_work / log_frames,
                       (log_work - log_wait) / log_frames, log_wait / log_frames, triangles, visible_count,
                       io->scale_percent, creatures_drawn);
                fflush(stdout);
                log_frames = 0;
                log_ms = log_work = log_wait = 0;
                log_low_fps = 1e9f;
            }
            stats_start = frame_end;
            stats_frames = 0;
            work_total = 0;
            stats_wait = 0;
        }
        /* Save a changed world every 30 seconds. */
        if (save_path && world_changed && frame_end - last_save >= 30000.0) {
            if (save_world(save_path, &player)) {
                world_changed = 0;
                io->saves++;
            }
            last_save = frame_end;
        }
        if (fps_limit > 0) sleep_ms(1000.0 / fps_limit - (now_ms() - frame_start));
    }
    /* Always on exit: the player's place changes even without building. */
    if (save_path && save_world(save_path, &player)) {
        io->saves++;
        printf("blocks: world saved\n");
    }
    if (previous) p_glDeleteSync(previous);
    if (bench && bench_frames > 0) {
        double elapsed = now_ms() - bench_start;
        printf("bench: %d frames in %.0f ms: %.1f fps, %.2f ms/frame, cpu %.2f ms, wait %.2f ms\n", bench_frames,
               elapsed, bench_frames * 1000.0 / elapsed, elapsed / bench_frames, (bench_work - bench_wait) / bench_frames,
               bench_wait / bench_frames);
    }
    stop_audio();
    if (header) __atomic_store_n(&header[4], 0, __ATOMIC_RELEASE);
    __atomic_store_n(&io->finished, 1, __ATOMIC_RELEASE);
    printf("blocks: %d frames, GL error 0x%x\n", frames, p_glGetError());
    printf("blocks: ok\n");
    return 0;
}
