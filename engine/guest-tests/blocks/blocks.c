/* HyPocket Blocks: a small, original block world used to test HyPocket's whole
 * path (x86-64 -> Box64 -> desktop OpenGL 4.1 -> VirGL -> phone GPU) with a
 * load shaped like a real game instead of a synthetic benchmark.
 *
 * The world is 8 x 8 chunks of 16 x 48 x 16 blocks. Each chunk is meshed once
 * (and again when a block in it changes) into a vertex buffer that holds only
 * the faces next to air or water, with per-vertex ambient occlusion. Frames
 * are drawn into a framebuffer object and shown with eglSwapBuffers, which
 * HyPocket's Mesa turns into a direct present (HYPOCKET_PRESENT), or read
 * back into a shared file otherwise (HYPOCKET_FRAME_PATH, see
 * gl-display-probe).
 *
 * Input and statistics go through a small shared file (HYPOCKET_INPUT_PATH);
 * the layout is below and must match BlocksSession.kt.
 *
 * Usage: blocks <width> <height> [fps limit] [frames]
 * With [frames] the program exits after that many frames and walks on its
 * own, for testing on a desktop. */
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/glcorearb.h>
#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

/* ---- Shared input/statistics file ------------------------------------- */

#define IO_MAGIC 0x49425048u /* "HPBI" */
#define IO_SIZE 128

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
    int32_t reserved_in[6];
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
    X(PFNGLUNIFORMMATRIX4FVPROC, glUniformMatrix4fv) \
    X(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays) \
    X(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray) \
    X(PFNGLGENBUFFERSPROC, glGenBuffers) \
    X(PFNGLBINDBUFFERPROC, glBindBuffer) \
    X(PFNGLBUFFERDATAPROC, glBufferData) \
    X(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer) \
    X(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray) \
    X(PFNGLDRAWELEMENTSPROC, glDrawElements) \
    X(PFNGLDRAWARRAYSPROC, glDrawArrays) \
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

#define CHUNK 16
#define CHUNKS 8
#define WX (CHUNK * CHUNKS)
#define WZ (CHUNK * CHUNKS)
#define WY 48
#define WATER_LEVEL 15

enum { AIR, GRASS, DIRT, STONE, SAND, WOOD, LEAVES, WATER, PLANKS, BLOCK_TYPES };

/* Texture layers. */
enum { T_GRASS_TOP, T_GRASS_SIDE, T_DIRT, T_STONE, T_SAND, T_WOOD_SIDE, T_WOOD_TOP, T_LEAVES, T_WATER, T_PLANKS, LAYERS };

static uint8_t world[WX][WY][WZ];

static int block_at(int x, int y, int z) {
    if (y < 0) return STONE;
    if (x < 0 || z < 0 || x >= WX || z >= WZ || y >= WY) return AIR;
    return world[x][y][z];
}

/* Blocks that hide the faces behind them. */
static int is_opaque(int b) { return b != AIR && b != WATER && b != LEAVES; }
static int is_solid(int b) { return b != AIR && b != WATER; }

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
    default: return T_PLANKS;
    }
}

static void place_tree(int x, int ground, int z) {
    int height = 4 + (int)(rand01(x, 7, z) * 2);
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
    static int heights[WX][WZ];
    for (int x = 0; x < WX; x++)
        for (int z = 0; z < WZ; z++) {
            float hills = fbm(x / 40.0f, z / 40.0f, 1);
            float detail = fbm(x / 12.0f, z / 12.0f, 9);
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
            if (world[x][heights[x][z]][z] == GRASS && hash3(x, 3, z) % 61 == 0) place_tree(x, heights[x][z], z);
}

/* ---- Meshing ---------------------------------------------------------- */

/* One vertex: position inside the chunk, normal index; texture u, v, layer,
 * ambient occlusion (0-3). 8 bytes. */
typedef struct {
    uint8_t x, y, z, face;
    uint8_t u, v, layer, ao;
} Vertex;

typedef struct {
    GLuint vao, vbo;
    int quads;       /* opaque quads, first in the buffer */
    int water_quads; /* water quads, after the opaque ones */
    int dirty;
} Chunk;

static Chunk chunks[CHUNKS][CHUNKS];

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
        v[i].u = corner_uv[i][0];
        v[i].v = corner_uv[i][1];
        v[i].layer = (uint8_t)layer;
        v[i].ao = (uint8_t)(water ? 3 : corner_ao(x, y, z, face, c));
    }
    scratch_quads++;
    return count + 1;
}

static void mesh_chunk(int ci, int cj) {
    Chunk* ch = &chunks[ci][cj];
    int cx = ci * CHUNK, cz = cj * CHUNK;
    scratch_quads = 0;
    int opaque = 0, water = 0;
    for (int pass = 0; pass < 2; pass++)
        for (int x = cx; x < cx + CHUNK; x++)
            for (int y = 0; y < WY; y++)
                for (int z = cz; z < cz + CHUNK; z++) {
                    int b = world[x][y][z];
                    if (b == AIR || (b == WATER) != (pass == 1)) continue;
                    for (int f = 0; f < 6; f++) {
                        const int* n = face_normal[f];
                        int other = block_at(x + n[0], y + n[1], z + n[2]);
                        int visible = b == WATER ? other == AIR : !is_opaque(other) && other != b;
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
                float r, g, b;
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
                case T_WATER: r = 0.16f; g = 0.32f; b = 0.68f; n = 0.9f + 0.15f * n; break;
                default: /* planks */
                    r = 0.70f; g = 0.54f; b = 0.32f;
                    n = (y % 4 == 3 || (x == ((y / 4) % 2 ? 3 : 11) )) ? 0.65f : 0.9f + 0.12f * n;
                    break;
                }
                px[l][y][x][0] = (uint8_t)fminf(255, r * n * 255);
                px[l][y][x][1] = (uint8_t)fminf(255, g * n * 255);
                px[l][y][x][2] = (uint8_t)fminf(255, b * n * 255);
                px[l][y][x][3] = l == T_WATER ? 200 : 255;
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
    "layout(location = 1) in vec4 uv_layer_ao;\n"
    "uniform mat4 view_proj;\n"
    "uniform vec3 chunk_origin;\n"
    "uniform vec3 eye;\n"
    "out vec3 uvl;\n"
    "out float light;\n"
    "out float dist;\n"
    "const float face_light[6] = float[](0.80, 0.80, 1.00, 0.50, 0.65, 0.65);\n"
    "void main() {\n"
    "    vec3 world = chunk_origin + pos_face.xyz;\n"
    "    gl_Position = view_proj * vec4(world, 1.0);\n"
    "    uvl = uv_layer_ao.xyz;\n"
    "    light = face_light[int(pos_face.w)] * (0.55 + 0.15 * uv_layer_ao.w);\n"
    "    dist = length(world - eye);\n"
    "}\n";

static const char* world_fs =
    "#version 330 core\n"
    "uniform sampler2DArray tex;\n"
    "uniform vec3 fog_color;\n"
    "uniform float fog_end;\n"
    "in vec3 uvl;\n"
    "in float light;\n"
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
    chunks[ci][cj].dirty = 1;
    /* Faces and ambient occlusion of the neighbours can change too. */
    if (x % CHUNK == 0 && ci > 0) chunks[ci - 1][cj].dirty = 1;
    if (x % CHUNK == CHUNK - 1 && ci < CHUNKS - 1) chunks[ci + 1][cj].dirty = 1;
    if (z % CHUNK == 0 && cj > 0) chunks[ci][cj - 1].dirty = 1;
    if (z % CHUNK == CHUNK - 1 && cj < CHUNKS - 1) chunks[ci][cj + 1].dirty = 1;
}

static void set_block(int x, int y, int z, int b) {
    if (x < 0 || z < 0 || x >= WX || z >= WZ || y < 0 || y >= WY) return;
    world[x][y][z] = (uint8_t)b;
    mark_dirty(x, z);
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
    const EGLint config_attribs[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
                                     EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_NONE};
    EGLConfig config;
    EGLint count = 0;
    eglChooseConfig(display, config_attribs, &config, 1, &count);
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

    GLuint fbo;
    if (!create_target(w, h, &fbo)) {
        printf("blocks: framebuffer %dx%d incomplete\n", w, h);
        return 1;
    }

    GLuint world_program = link_program(world_vs, world_fs);
    GLuint hud_program = link_program(hud_vs, hud_fs);
    GLint u_view_proj = p_glGetUniformLocation(world_program, "view_proj");
    GLint u_origin = p_glGetUniformLocation(world_program, "chunk_origin");
    GLint u_eye = p_glGetUniformLocation(world_program, "eye");
    GLint u_fog_color = p_glGetUniformLocation(world_program, "fog_color");
    GLint u_fog_end = p_glGetUniformLocation(world_program, "fog_end");
    p_glUseProgram(world_program);
    p_glUniform1i(p_glGetUniformLocation(world_program, "tex"), 0);
    p_glUseProgram(hud_program);
    p_glUniform2f(p_glGetUniformLocation(hud_program, "pixel"), 2.0f / w * (h / 540.0f), 2.0f / h * (h / 540.0f));
    make_textures();

    double build_start = now_ms();
    generate_world();
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
    printf("blocks: world built in %.0f ms, %d faces\n", build_ms, total_quads);
    fflush(stdout);

    /* Spawn in the middle, on top of the ground. */
    Player player = {WX / 2 + 0.5f, WY, WZ / 2 + 0.5f, 0, 0, -0.25f, 0};
    for (int r = 0; r < WX / 2; r++) { /* nearest grass, spiralling out */
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
    while (player.y > 1 && !collides(player.x, player.y - 1, player.z)) player.y -= 1;
    float base_yaw = io->look_yaw, base_pitch = io->look_pitch;
    int seen_breaks = io->breaks, seen_places = io->places;

    const float sky[3] = {0.55f, 0.73f, 0.92f};
    const float far = 72.0f;
    double last = now_ms(), stats_start = last, work_total = 0;
    int stats_frames = 0, frames = 0;
    GLsync previous = 0;
    io->width = w;
    io->height = h;
    io->build_ms = (float)build_ms;

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
        player.yaw = io->look_yaw - base_yaw;
        player.pitch = io->look_pitch - base_pitch - 0.25f;
        if (player.pitch > 1.5f) player.pitch = 1.5f;
        if (player.pitch < -1.5f) player.pitch = -1.5f;
        float len = sqrtf(mx * mx + my * my);
        if (len > 1) { mx /= len; my /= len; }
        float speed = 4.3f;
        float sy = sinf(player.yaw), cy = cosf(player.yaw);
        float vx = (mx * cy + my * sy) * speed, vz = (mx * sy - my * cy) * speed;
        int in_water = block_at((int)floorf(player.x), (int)floorf(player.y + 0.5f), (int)floorf(player.z)) == WATER;
        if ((io->jump || (max_frames && player.on_ground)) && (player.on_ground || in_water)) {
            player.vy = in_water ? 3.5f : 8.2f;
            player.on_ground = 0;
        }
        player.vy -= (in_water ? 10.0f : 26.0f) * dt;
        if (player.vy < -40) player.vy = -40;
        if (in_water) { vx *= 0.6f; vz *= 0.6f; if (player.vy < -3) player.vy = -3; }
        move_player(&player, vx * dt, player.vy * dt, vz * dt);
        if (player.y < -10) { player.y = WY; player.vy = 0; }

        /* ---- Break and place ---- */
        int breaks = io->breaks, places = io->places;
        if (breaks != seen_breaks || places != seen_places) {
            int hit[3], before[3];
            if (raycast(&player, hit, before)) {
                if (breaks != seen_breaks) {
                    set_block(hit[0], hit[1], hit[2], hit[1] < WATER_LEVEL + 1 && block_at(hit[0], hit[1] + 1, hit[2]) == WATER ? WATER : AIR);
                } else if (block_at(before[0], before[1], before[2]) == AIR || block_at(before[0], before[1], before[2]) == WATER) {
                    set_block(before[0], before[1], before[2], PLANKS);
                    if (collides(player.x, player.y, player.z)) set_block(before[0], before[1], before[2], AIR);
                }
            }
            seen_breaks = breaks;
            seen_places = places;
        }
        for (int i = 0; i < CHUNKS; i++)
            for (int j = 0; j < CHUNKS; j++) {
                if (!chunks[i][j].dirty) continue;
                p_glBindVertexArray(chunks[i][j].vao);
                mesh_chunk(i, j);
            }

        /* ---- Draw ---- */
        p_glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        p_glViewport(0, 0, w, h);
        p_glClearColor(sky[0], sky[1], sky[2], 1.0f);
        p_glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        p_glEnable(GL_DEPTH_TEST);
        p_glEnable(GL_CULL_FACE);
        p_glUseProgram(world_program);
        float vp[16];
        view_projection(vp, &player, (float)w / h, far);
        p_glUniformMatrix4fv(u_view_proj, 1, GL_FALSE, vp);
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
        p_glUseProgram(hud_program);
        p_glBindVertexArray(hud_vao);
        p_glDrawArrays(GL_TRIANGLES, 0, 12);
        p_glDisable(GL_BLEND);

        if (present) {
            p_glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
            p_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            p_glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
            eglSwapBuffers(display, surface);
        } else if (header) {
            p_glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
            __atomic_store_n(&header[3], (uint32_t)frames + 1, __ATOMIC_RELEASE);
        }
        /* Keep at most one frame queued: wait for the previous one, not this
         * one, so the CPU prepares the next frame while the GPU draws. */
        GLsync fence = p_glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        if (previous) {
            p_glClientWaitSync(previous, GL_SYNC_FLUSH_COMMANDS_BIT, 100000000ull);
            p_glDeleteSync(previous);
        }
        previous = fence;

        double frame_end = now_ms();
        work_total += frame_end - frame_start;
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
            printf("blocks: %.1f fps, %.1f ms/frame, %d triangles, %d chunks\n", io->fps, io->frame_ms, triangles,
                   visible_count);
            fflush(stdout);
            stats_start = frame_end;
            stats_frames = 0;
            work_total = 0;
        }
        if (fps_limit > 0) sleep_ms(1000.0 / fps_limit - (now_ms() - frame_start));
    }
    if (previous) p_glDeleteSync(previous);
    if (header) __atomic_store_n(&header[4], 0, __ATOMIC_RELEASE);
    printf("blocks: %d frames, GL error 0x%x\n", frames, p_glGetError());
    printf("blocks: ok\n");
    return 0;
}
