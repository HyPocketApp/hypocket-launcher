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
 * file (HYPOCKET_AUDIO_PATH), which the app plays (audio_output.cpp).
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
 * HYPOCKET_BLOCKS_CREATIVE=1 blocks are unlimited and nothing is collected. */
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/glcorearb.h>
#include <fcntl.h>
#include <pthread.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/* ---- Shared input/statistics file ------------------------------------- */

#define IO_MAGIC 0x49425048u /* "HPBI" */
#define IO_SIZE 256

typedef struct {
    uint32_t magic;
    uint32_t version;
    /* Written by the app. */
    float move_x;      /* strafe, -1 (left) to 1 (right) */
    float move_y;      /* -1 (back) to 1 (forward) */
    float look_yaw;    /* radians, accumulated since start */
    float look_pitch;  /* radians, accumulated since start */
    int32_t jump;      /* 1 while the jump button is held */
    int32_t breaks;    /* incremented on each tap: break the aimed block */
    int32_t places;    /* incremented on each place press */
    int32_t quit;      /* 1: exit */
    float look_rate_yaw;   /* radians per second, from a controller stick */
    float look_rate_pitch; /* radians per second */
    int32_t selected;  /* index into the building palette (palette[] below) */
    int32_t reserved_in[3];
    /* Written by the game. */
    int32_t ready;     /* 1 once the world is built and frames are shown */
    int32_t frames;
    float fps;         /* over the last second */
    float frame_ms;    /* CPU + GPU time of one frame, over the last second */
    int32_t triangles; /* in the last frame */
    int32_t chunks;    /* drawn in the last frame */
    float build_ms;    /* time to generate and mesh the world */
    int32_t width;
    int32_t height;
    int32_t scale_percent; /* dynamic resolution: share of width and height drawn */
    int32_t creatures;     /* drawn in the last frame */
    int32_t saves;         /* times the world was written to its save file */
    int32_t clock_minutes; /* time of day in the game, minutes after midnight */
    int32_t day;           /* days passed in this world, from 1 */
    int32_t finished;      /* 1 once the game has saved and is about to exit normally */
    int32_t reserved_out;
    int16_t counts[16];    /* blocks carried, in palette order (unlimited in creative mode) */
} SharedIo;

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
    X(PFNGLDELETESYNCPROC, glDeleteSync)

#define DECLARE(type, name) static type p_##name;
GL_FUNCTIONS(DECLARE)

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
       BLOCK_TYPES };

/* What the player can build with, in the order the app shows it (must match
 * BlocksPalette.kt). */
static const int palette[] = {PLANKS, STONE_BRICKS, BRICKS, GLASS, ROOF, PLASTER, LAMP,
                              WOOD, STONE, DIRT, SAND, LEAVES};
#define PALETTE_SIZE ((int)(sizeof palette / sizeof palette[0]))

/* Texture layers. */
enum { T_GRASS_TOP, T_GRASS_SIDE, T_DIRT, T_STONE, T_SAND, T_WOOD_SIDE, T_WOOD_TOP, T_LEAVES, T_WATER, T_PLANKS,
       T_STONE_BRICKS, T_BRICKS, T_GLASS, T_ROOF, T_PLASTER, T_LAMP, LAYERS };

static uint8_t world[MAX_W][WY][MAX_W];

static int block_at(int x, int y, int z) {
    if (y < 0) return STONE;
    if (x < 0 || z < 0 || x >= WX || z >= WZ || y >= WY) return AIR;
    return world[x][y][z];
}

/* Blocks that hide the faces behind them. */
static int is_opaque(int b) { return b != AIR && b != WATER && b != LEAVES && b != GLASS; }
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

typedef struct {
    GLuint vao, vbo;
    int quads;       /* opaque quads, first in the buffer */
    int water_quads; /* water and glass quads, after the opaque ones */
    int dirty; /* 2: its blocks changed, mesh now; 1: only its lamp light did */
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
                if (world[x][y][z] == LAMP) chunk_lamps[x / CHUNK][z / CHUNK]++;
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
                        if (world[x][y][z] == LAMP) {
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
    for (int pass = 0; pass < 2; pass++)
        for (int x = cx; x < cx + CHUNK; x++)
            for (int y = 0; y < WY; y++)
                for (int z = cz; z < cz + CHUNK; z++) {
                    int b = world[x][y][z];
                    if (b == AIR || is_clear(b) != (pass == 1)) continue;
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
    "    light = abs(uv_layer_ao_light.y - glow_layer) < 0.5 ? vec3(1.2) : max(shade * ambient, lamp);\n"
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
    "        float patch = smoothstep(0.32, 0.22, length(q - vec2(0.25, 0.2))) + smoothstep(0.22, 0.14, length(q + vec2(0.3, 0.1)));\n"
    "        color = vec4(moon_color.rgb * (1.0 - 0.18 * patch), moon_color.a * smoothstep(0.62, 0.56, r));\n"
    "    }\n"
    "}\n";

static GLuint compile(GLenum type, const char* source) {
    GLuint shader = p_glCreateShader(type);
    p_glShaderSource(shader, 1, &source, NULL);
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

static int collides(float x, float y, float z) {
    int x0 = (int)floorf(x - PLAYER_HALF), x1 = (int)floorf(x + PLAYER_HALF);
    int y0 = (int)floorf(y), y1 = (int)floorf(y + PLAYER_HEIGHT);
    int z0 = (int)floorf(z - PLAYER_HALF), z1 = (int)floorf(z + PLAYER_HALF);
    for (int bx = x0; bx <= x1; bx++)
        for (int by = y0; by <= y1; by++)
            for (int bz = z0; bz <= z1; bz++)
                if (is_solid(block_at(bx, by, bz))) return 1;
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
static uint16_t inventory[BLOCK_TYPES]; /* blocks carried, by block id */
static int creative;                    /* unlimited blocks, nothing collected */

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
    if (old == LAMP) chunk_lamps[ci][cj]--;
    if (b == LAMP) chunk_lamps[ci][cj]++;
    mark_dirty(x, z);
    /* Near a lamp, any change can move light up to LIGHT_REACH blocks away. */
    if (old == LAMP || b == LAMP || lamps_near(ci, cj))
        for (int i = (x - LIGHT_REACH) / CHUNK; i <= (x + LIGHT_REACH) / CHUNK; i++)
            for (int j = (z - LIGHT_REACH) / CHUNK; j <= (z + LIGHT_REACH) / CHUNK; j++)
                if (i >= 0 && j >= 0 && i < CHUNKS && j < CHUNKS && !chunks[i][j].dirty) chunks[i][j].dirty = 1;
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

/* ---- Saved worlds ----------------------------------------------------- */

/* File layout (little endian): this header, then the blocks run-length
 * encoded as (count 1-255, block id) byte pairs, x outermost, then y, then z.
 * Must match BlocksWorlds.kt, which reads the header. */
#define SAVE_MAGIC 0x57425048u /* "HPBW" */
#define SAVE_VERSION 3
/* Version 1 had no time of day (those fields were reserved and zero) and
 * starts in the morning; versions 1 and 2 had no inventory and start with an
 * empty one. Version 3 adds the inventory after the block data: one 16-bit
 * count per block id, inventory_size bytes in all (block ids only ever grow,
 * so a longer list comes from a newer game and its extra counts are dropped). */
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
    uint32_t reserved[3];
} SaveHeader;

/* Writes to a temporary file and renames it over the old save, so a save
 * cut short (the app killed mid-write) never destroys the previous one. */
static int save_world(const char* path, const Player* p) {
    size_t cells = (size_t)WX * WY * WZ;
    uint8_t* data = malloc(cells * 2);
    if (!data) return 0;
    size_t size = 0;
    int run_block = -1, run = 0;
    for (int x = 0; x < WX; x++)
        for (int y = 0; y < WY; y++)
            for (int z = 0; z < WZ; z++) {
                int b = world[x][y][z];
                if (b == run_block && run < 255) { run++; continue; }
                if (run) { data[size++] = (uint8_t)run; data[size++] = (uint8_t)run_block; }
                run_block = b;
                run = 1;
            }
    if (run) { data[size++] = (uint8_t)run; data[size++] = (uint8_t)run_block; }
    SaveHeader h = {SAVE_MAGIC, SAVE_VERSION, world_chunks, world_seed, p->x, p->y, p->z, p->yaw, p->pitch,
                    (uint32_t)size, day_time, day_count, (uint32_t)sizeof inventory, {0}};
    char tmp[4096];
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE* f = fopen(tmp, "wb");
    int ok = f && fwrite(&h, sizeof h, 1, f) == 1 && fwrite(data, 1, size, f) == size &&
             fwrite(inventory, sizeof inventory, 1, f) == 1;
    if (f) {
        ok = fflush(f) == 0 && ok;
        ok = fsync(fileno(f)) == 0 && ok;
        ok = fclose(f) == 0 && ok;
    }
    free(data);
    if (ok) ok = rename(tmp, path) == 0;
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
                for (int b = 0; b < BLOCK_TYPES; b++) inventory[b] = carried[b] > MAX_CARRIED ? MAX_CARRIED : carried[b];
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

/* ---- Creatures and particles ----------------------------------------- */

/* Heavy mode fills the world with small wandering creatures. Each is two
 * boxes drawn with their own draw calls and uniforms, like the separate
 * objects of a real game, and each thinks and moves on the CPU. */
typedef struct {
    float x, y, z, vy, yaw, target_yaw, speed, timer, phase;
    int on_ground, color;
} Creature;

#define MAX_CREATURES 300
static Creature creatures[MAX_CREATURES];
static int creature_count;

static const float creature_colors[6][3] = {
    {0.86f, 0.62f, 0.45f}, {0.55f, 0.72f, 0.88f}, {0.92f, 0.85f, 0.55f},
    {0.70f, 0.55f, 0.85f}, {0.60f, 0.82f, 0.60f}, {0.90f, 0.60f, 0.70f},
};

/* Top of the highest solid block at or below height y in column x, z. */
static float ground_below(float x, float y, float z) {
    int bx = (int)floorf(x), bz = (int)floorf(z);
    for (int by = (int)floorf(y + 0.5f); by >= 0; by--)
        if (is_solid(block_at(bx, by, bz))) return by + 1.0f;
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
    }
}

static void update_creatures(float dt, uint32_t tick) {
    for (int i = 0; i < creature_count; i++) {
        Creature* c = &creatures[i];
        c->timer -= dt;
        if (c->timer <= 0) { /* wander somewhere else, or rest */
            float r = rand01(i, (int)tick, 23);
            c->target_yaw = c->yaw + (r - 0.5f) * 4.0f;
            c->speed = r < 0.25f ? 0 : 0.6f + 1.2f * rand01(i, (int)tick, 29);
            c->timer = 1.0f + 3.0f * rand01(i, (int)tick, 31);
        }
        float turn = c->target_yaw - c->yaw;
        c->yaw += turn * (dt * 3 < 1 ? dt * 3 : 1);
        float nx = c->x + sinf(c->yaw) * c->speed * dt, nz = c->z - cosf(c->yaw) * c->speed * dt;
        int feet = (int)floorf(c->y + 0.1f);
        int ahead = block_at((int)floorf(nx), feet, (int)floorf(nz));
        if (nx < 1 || nz < 1 || nx > WX - 1 || nz > WZ - 1 || ahead == WATER ||
            block_at((int)floorf(nx), feet - 1, (int)floorf(nz)) == WATER) {
            c->target_yaw = c->yaw + 3.1416f; /* turn back from edges and water */
            c->timer = 1.5f;
        } else if (is_solid(ahead)) {
            if (c->on_ground && !is_solid(block_at((int)floorf(nx), feet + 1, (int)floorf(nz)))) {
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
}

typedef struct {
    float x, y, z, vx, vy, vz, life;
    float color[3];
} Particle;

#define MAX_PARTICLES 192
static Particle particles[MAX_PARTICLES];

static void block_color(int block, float* out) {
    static const float colors[BLOCK_TYPES][3] = {
        {0, 0, 0}, {0.32f, 0.58f, 0.24f}, {0.52f, 0.37f, 0.24f}, {0.52f, 0.52f, 0.52f}, {0.86f, 0.80f, 0.58f},
        {0.42f, 0.30f, 0.18f}, {0.20f, 0.45f, 0.18f}, {0.16f, 0.32f, 0.68f}, {0.70f, 0.54f, 0.32f},
        {0.55f, 0.55f, 0.55f}, {0.62f, 0.32f, 0.25f}, {0.82f, 0.90f, 0.95f}, {0.64f, 0.29f, 0.19f},
        {0.86f, 0.84f, 0.78f}, {1.00f, 0.84f, 0.50f},
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
        block_color(block, p->color);
        float shade = 0.75f + 0.4f * r3;
        for (int k = 0; k < 3; k++) p->color[k] *= shade;
        made++;
    }
}

static void update_particles(float dt) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle* p = &particles[i];
        if (p->life <= 0) continue;
        p->life -= dt;
        p->vy -= 18 * dt;
        float nx = p->x + p->vx * dt, ny = p->y + p->vy * dt, nz = p->z + p->vz * dt;
        if (is_solid(block_at((int)floorf(nx), (int)floorf(ny), (int)floorf(nz)))) {
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
    "    frag = vec4(mix(color * light * ambient, fog_color, fog), 1.0);\n"
    "}\n";

/* Per instance: center (3 floats), half size (3), yaw (1), colour (3). */
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
    "    frag = vec4(mix(v_color * light * ambient, fog_color, fog), 1.0);\n"
    "}\n";

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

/* Ring of 16-bit stereo samples shared with the app; must match
 * engine/src/main/cpp/audio_output.cpp. */
#define AUDIO_MAGIC 0x41425048u /* "HPBA" */
typedef struct {
    uint32_t magic, version, rate, channels, capacity, reserved;
    uint64_t write_pos, read_pos;
    uint32_t underruns;
    uint32_t finished; /* set by the game when it stops writing */
} AudioRing;

enum { SND_STEP, SND_STEP2, SND_BREAK, SND_PLACE, SND_JUMP, SND_SPLASH, SND_CHIRP, SOUNDS };

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

static struct {
    AudioRing* ring;
    int16_t* samples;
    int rate;
    Sound sounds[SOUNDS];
    Voice voices[VOICES];
    pthread_mutex_t lock;
    pthread_t thread;
    volatile int running;
    float wind_state, wind_gust, wind_phase;
    uint32_t noise;
    double next_chirp;
    uint64_t mixed;
    int peak_voices;
} audio = {.lock = PTHREAD_MUTEX_INITIALIZER, .noise = 22222};

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
        float l = audio.wind_state * (0.12f + 0.25f * gust), r = l * 0.85f;
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
        audio.samples[index * 2] = (int16_t)(l * 32000.0f);
        audio.samples[index * 2 + 1] = (int16_t)(r * 32000.0f);
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

static void start_audio(void) {
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
    audio.rate = (int)ring->rate;
    static const float lengths[SOUNDS] = {0.12f, 0.12f, 0.45f, 0.16f, 0.25f, 0.6f, 0.33f};
    for (int i = 0; i < SOUNDS; i++) audio.sounds[i] = make_sound(lengths[i], i);
    audio.samples = (int16_t*)((char*)map + 64);
    audio.ring = ring;
    audio.running = 1;
    if (pthread_create(&audio.thread, NULL, audio_thread, NULL) != 0) {
        audio.ring = NULL;
        return;
    }
    printf("blocks: audio %d Hz, %u frames of ring\n", audio.rate, ring->capacity);
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
    return p_glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

static SharedIo* open_io(void) {
    static SharedIo local;
    const char* path = getenv("HYPOCKET_INPUT_PATH");
    if (!path) return &local;
    int fd = open(path, O_RDWR);
    if (fd < 0) return &local;
    void* map = mmap(NULL, IO_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (map == MAP_FAILED || ((SharedIo*)map)->magic != IO_MAGIC) return &local;
    return map;
}

int main(int argc, char** argv) {
    _Static_assert(sizeof(SharedIo) <= IO_SIZE, "shared file too small");
    _Static_assert(sizeof(SaveHeader) == 64, "the save header must keep its size");
    _Static_assert(__builtin_offsetof(SharedIo, counts) == 128, "must match BlocksSession.kt");
    int w = argc > 1 ? atoi(argv[1]) : 1200;
    int h = argc > 2 ? atoi(argv[2]) : 540;
    int fps_limit = argc > 3 ? atoi(argv[3]) : 0;
    int max_frames = argc > 4 ? atoi(argv[4]) : 0;
    if (w < 64 || h < 64 || w > 4096 || h > 4096) return 2;
    SharedIo* io = open_io();
    int present = getenv("HYPOCKET_PRESENT") != NULL;
    const char* frame_path = present ? NULL : getenv("HYPOCKET_FRAME_PATH");

    uint32_t* header = NULL;
    unsigned char* pixels = NULL;
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
    int window_target = 0; /* draw into the pbuffer itself, without our own framebuffer */
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
    enum { MAX_INSTANCES = MAX_CREATURES * 2 + MAX_PARTICLES };
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

    /* A new world: spawn in the middle, on top of the ground. */
    if (!loaded) player = (Player){WX / 2 + 0.5f, WY, WZ / 2 + 0.5f, 0, 0, -0.25f, 0};
    for (int r = 0; r < WX / 2 && !loaded; r++) { /* nearest grass, spiralling out */
        int found = 0;
        for (int dx = -r; dx <= r && !found; dx++)
            for (int dz = -r; dz <= r && !found; dz++) {
                int x = WX / 2 + dx, z = WZ / 2 + dz, y = WY - 1;
                while (y > 0 && world[x][y][z] == AIR) y--;
                if (world[x][y][z] == GRASS && world[x][y + 1][z] == AIR && world[x][y + 2][z] == AIR) {
                    player.x = x + 0.5f;
                    player.z = z + 0.5f;
                    player.y = y + 1.0f;
                    found = 1;
                }
            }
        if (found) break;
    }
    while (!loaded && player.y > 1 && !collides(player.x, player.y - 1, player.z)) player.y -= 1;
    /* A loaded world keeps the direction the player faced (not the pitch,
     * which the app clamps from the default). */
    float base_yaw = io->look_yaw - player.yaw, base_pitch = io->look_pitch;
    int seen_breaks = io->breaks, seen_places = io->places;

    /* Time of day: the save's, or HYPOCKET_BLOCKS_TIME, or the morning. */
    const char* time_env = getenv("HYPOCKET_BLOCKS_TIME");
    if (time_env && (!loaded || max_frames)) day_time = fmodf(fabsf((float)atof(time_env)), 1.0f);
    float day_seconds = 15 * 60;
    if (getenv("HYPOCKET_BLOCKS_DAY_SECONDS") && atof(getenv("HYPOCKET_BLOCKS_DAY_SECONDS")) >= 10)
        day_seconds = (float)atof(getenv("HYPOCKET_BLOCKS_DAY_SECONDS"));
    const float far = heavy ? 128.0f : 72.0f;
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
    const double target_fps = fps_limit > 0 ? fps_limit : 60;
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
    int step_variant = 0, was_in_water = 0;
    double last_save = last;
    if (bench) save_path = NULL; /* the benchmark never touches saves */

    while (!io->quit && (max_frames == 0 || frames < max_frames)) {
        double frame_start = now_ms();
        float dt = (float)((frame_start - last) / 1000.0);
        last = frame_start;
        if (dt > 0.1f) dt = 0.1f;

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
        float len = sqrtf(mx * mx + my * my);
        if (len > 1) { mx /= len; my /= len; }
        float speed = 4.3f;
        float sy = sinf(player.yaw), cy = cosf(player.yaw);
        float vx = (mx * cy + my * sy) * speed, vz = (mx * sy - my * cy) * speed;
        int in_water = block_at((int)floorf(player.x), (int)floorf(player.y + 0.5f), (int)floorf(player.z)) == WATER;
        if ((io->jump || (max_frames && player.on_ground)) && (player.on_ground || in_water)) {
            if (player.on_ground && !in_water) play_sound(SND_JUMP, 0.25f, 0, 1.0f);
            player.vy = in_water ? 3.5f : 8.2f;
            player.on_ground = 0;
        }
        if (in_water && !was_in_water && player.vy < -2) play_sound(SND_SPLASH, 0.6f, 0, 1.0f);
        was_in_water = in_water;
        float fall_speed = -player.vy;
        int was_on_ground = player.on_ground;
        player.vy -= (in_water ? 10.0f : 26.0f) * dt;
        if (player.vy < -40) player.vy = -40;
        if (in_water) { vx *= 0.6f; vz *= 0.6f; if (player.vy < -3) player.vy = -3; }
        move_player(&player, vx * dt, player.vy * dt, vz * dt);
        if (player.y < -10) { player.y = WY; player.vy = 0; }
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
            if (raycast(&player, hit, before)) {
                if (breaks != seen_breaks) {
                    int broken = block_at(hit[0], hit[1], hit[2]), drop = drop_for(broken);
                    if (!creative && drop != AIR && inventory[drop] < MAX_CARRIED) inventory[drop]++;
                    spawn_particles(hit[0], hit[1], hit[2], broken, (uint32_t)frames);
                    play_at(SND_BREAK, &player, hit[0] + 0.5f, hit[1] + 0.5f, hit[2] + 0.5f,
                            0.9f + 0.2f * rand01(hit[0], hit[1], hit[2]));
                    set_block(hit[0], hit[1], hit[2], hit[1] < WATER_LEVEL + 1 && block_at(hit[0], hit[1] + 1, hit[2]) == WATER ? WATER : AIR);
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
            seen_breaks = breaks;
            seen_places = places;
        }
        if (!bench) {
            if (time_lock < 0) day_time += dt / day_seconds;
            if (day_time >= 1.0f) { day_time -= 1.0f; day_count++; }
        }
        const Sky sk = sky_at(shown_time());
        const float* sky = sk.sky;
        update_creatures(dt, (uint32_t)frames);
        update_particles(dt);
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
        int scaled = dynamic && scale < 0.999f;
        int sw = scaled ? ((int)(w * scale) & ~1) : w, sh = scaled ? ((int)(h * scale) & ~1) : h;
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
        p_glUniform1f(u_fog_end, far);

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
        /* Creatures and particles: one draw call per box, or one for all. */
        int creatures_drawn = 0;
        if (separate_draws) {
            p_glUseProgram(box_program);
            p_glUniformMatrix4fv(ub_view_proj, 1, GL_FALSE, vp);
            p_glUniform3f(ub_eye, player.x, player.y + EYE_HEIGHT, player.z);
            p_glUniform3f(ub_fog_color, sky[0], sky[1], sky[2]);
            p_glUniform1f(ub_fog_end, far);
            p_glUniform3f(ub_ambient, sk.ambient[0], sk.ambient[1], sk.ambient[2]);
            p_glBindVertexArray(hud_vao); /* no attributes: corners come from gl_VertexID */
            for (int i = 0; i < creature_count; i++) {
                const Creature* c = &creatures[i];
                float dx = c->x - player.x, dz = c->z - player.z, d = sqrtf(dx * dx + dz * dz);
                if (d > far || (d > 4 && dx * fx + dz * fz < -2)) continue;
                const float* col = creature_colors[c->color];
                float bob = c->on_ground ? fabsf(sinf(c->phase)) * 0.06f : 0;
                p_glUniform1f(ub_yaw, c->yaw);
                p_glUniform3f(ub_center, c->x, c->y + 0.38f + bob, c->z);
                p_glUniform3f(ub_half, 0.32f, 0.26f, 0.42f);
                p_glUniform3f(ub_color, col[0], col[1], col[2]);
                p_glDrawArrays(GL_TRIANGLES, 0, 36);
                /* The head, a little in front and above. */
                p_glUniform3f(ub_center, c->x + sinf(c->yaw) * 0.45f, c->y + 0.62f + bob, c->z - cosf(c->yaw) * 0.45f);
                p_glUniform3f(ub_half, 0.2f, 0.2f, 0.2f);
                p_glUniform3f(ub_color, col[0] * 0.8f, col[1] * 0.8f, col[2] * 0.8f);
                p_glDrawArrays(GL_TRIANGLES, 0, 36);
                creatures_drawn++;
                triangles += 24;
            }
            p_glUniform1f(ub_yaw, 0);
            p_glUniform3f(ub_half, 0.06f, 0.06f, 0.06f);
            for (int i = 0; i < MAX_PARTICLES; i++) {
                const Particle* pt = &particles[i];
                if (pt->life <= 0) continue;
                p_glUniform3f(ub_center, pt->x, pt->y, pt->z);
                p_glUniform3f(ub_color, pt->color[0], pt->color[1], pt->color[2]);
                p_glDrawArrays(GL_TRIANGLES, 0, 36);
                triangles += 12;
            }
        } else {
            int n = 0;
            for (int i = 0; i < creature_count; i++) {
                const Creature* c = &creatures[i];
                float dx = c->x - player.x, dz = c->z - player.z, d = sqrtf(dx * dx + dz * dz);
                if (d > far || (d > 4 && dx * fx + dz * fz < -2)) continue;
                const float* col = creature_colors[c->color];
                float bob = c->on_ground ? fabsf(sinf(c->phase)) * 0.06f : 0;
                float body[INSTANCE_FLOATS] = {c->x, c->y + 0.38f + bob, c->z, 0.32f, 0.26f, 0.42f, c->yaw,
                                               col[0], col[1], col[2]};
                float head[INSTANCE_FLOATS] = {c->x + sinf(c->yaw) * 0.45f, c->y + 0.62f + bob,
                                               c->z - cosf(c->yaw) * 0.45f, 0.2f, 0.2f, 0.2f, c->yaw,
                                               col[0] * 0.8f, col[1] * 0.8f, col[2] * 0.8f};
                memcpy(instances[n++], body, sizeof body);
                memcpy(instances[n++], head, sizeof head);
                creatures_drawn++;
                triangles += 24;
            }
            for (int i = 0; i < MAX_PARTICLES; i++) {
                const Particle* pt = &particles[i];
                if (pt->life <= 0) continue;
                float box[INSTANCE_FLOATS] = {pt->x, pt->y, pt->z, 0.06f, 0.06f, 0.06f, 0,
                                              pt->color[0], pt->color[1], pt->color[2]};
                memcpy(instances[n++], box, sizeof box);
                triangles += 12;
            }
            if (n > 0) {
                p_glUseProgram(inst_program);
                p_glUniformMatrix4fv(ui_view_proj, 1, GL_FALSE, vp);
                p_glUniform3f(ui_eye, player.x, player.y + EYE_HEIGHT, player.z);
                p_glUniform3f(ui_fog_color, sky[0], sky[1], sky[2]);
                p_glUniform1f(ui_fog_end, far);
                p_glUniform3f(ui_ambient, sk.ambient[0], sk.ambient[1], sk.ambient[2]);
                p_glBindVertexArray(inst_vao);
                p_glBindBuffer(GL_ARRAY_BUFFER, inst_vbo);
                /* A fresh buffer each frame, so the GPU never waits for the last one. */
                p_glBufferData(GL_ARRAY_BUFFER, sizeof instances, NULL, GL_STREAM_DRAW);
                p_glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)n * sizeof instances[0], instances);
                p_glDrawArraysInstanced(GL_TRIANGLES, 0, 36, n);
            }
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

        if (present) {
            if (!window_target) {
                p_glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
                p_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
                p_glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
            }
            eglSwapBuffers(display, surface);
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
        io->scale_percent = (int)(scale * 100.0f + 0.5f);
        io->creatures = creatures_drawn;
        io->clock_minutes = (int)(shown_time() * 1440.0f) % 1440;
        for (int k = 0; k < PALETTE_SIZE && k < 16; k++) io->counts[k] = (int16_t)inventory[palette[k]];
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
