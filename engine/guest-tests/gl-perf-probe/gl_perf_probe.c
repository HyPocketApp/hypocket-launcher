/* x86-64 Linux graphics performance test. It draws a block terrain (textured,
 * lit, depth-tested cubes) with a desktop OpenGL core context at several
 * resolutions derived from the phone's screen and several loads, measures
 * the GPU time per frame, and shows the frames through the same shared file
 * as the display probe (see gl-display-probe for the layout; here the file
 * is sized for the largest resolution and the header holds the current one).
 *
 * Usage: gl-perf-probe <screen width> <screen height> [seconds per step]
 * The cubes are drawn naively (every face), so this is a stress test, not a
 * model of how a real game meshes its world. */
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/glcorearb.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define HEADER 64
#define TEX 64

#define GL_FUNCTIONS(X) \
    X(PFNGLGETSTRINGPROC, glGetString) \
    X(PFNGLGETERRORPROC, glGetError) \
    X(PFNGLVIEWPORTPROC, glViewport) \
    X(PFNGLCLEARCOLORPROC, glClearColor) \
    X(PFNGLCLEARPROC, glClear) \
    X(PFNGLENABLEPROC, glEnable) \
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
    X(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays) \
    X(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray) \
    X(PFNGLDRAWARRAYSINSTANCEDPROC, glDrawArraysInstanced) \
    X(PFNGLGENTEXTURESPROC, glGenTextures) \
    X(PFNGLBINDTEXTUREPROC, glBindTexture) \
    X(PFNGLTEXIMAGE2DPROC, glTexImage2D) \
    X(PFNGLTEXPARAMETERIPROC, glTexParameteri) \
    X(PFNGLGENERATEMIPMAPPROC, glGenerateMipmap) \
    X(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers) \
    X(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer) \
    X(PFNGLDELETEFRAMEBUFFERSPROC, glDeleteFramebuffers) \
    X(PFNGLGENRENDERBUFFERSPROC, glGenRenderbuffers) \
    X(PFNGLBINDRENDERBUFFERPROC, glBindRenderbuffer) \
    X(PFNGLDELETERENDERBUFFERSPROC, glDeleteRenderbuffers) \
    X(PFNGLRENDERBUFFERSTORAGEPROC, glRenderbufferStorage) \
    X(PFNGLFRAMEBUFFERRENDERBUFFERPROC, glFramebufferRenderbuffer) \
    X(PFNGLCHECKFRAMEBUFFERSTATUSPROC, glCheckFramebufferStatus)

#define DECLARE(type, name) static type p_##name;
GL_FUNCTIONS(DECLARE)

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

static GLuint compile(GLenum type, const char* source) {
    GLuint shader = p_glCreateShader(type);
    p_glShaderSource(shader, 1, &source, NULL);
    p_glCompileShader(shader);
    GLint ok = 0;
    p_glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        p_glGetShaderInfoLog(shader, sizeof log, NULL, log);
        printf("gl-perf-probe: shader error: %s\n", log);
    }
    return shader;
}

/* Cube corners and faces come from gl_VertexID, cube positions from
 * gl_InstanceID: a size x size grid of columns, `layers` cubes deep, with a
 * rolling height. */
static const char* vertex_shader =
    "#version 330 core\n"
    "uniform float time;\n"
    "uniform int size;\n"
    "uniform int layers;\n"
    "uniform vec2 aspect;\n"
    "out vec2 uv;\n"
    "out float light;\n"
    "out float shade;\n"
    "const vec3 normals[6] = vec3[](vec3(1,0,0), vec3(-1,0,0), vec3(0,1,0), vec3(0,-1,0), vec3(0,0,1), vec3(0,0,-1));\n"
    "const vec2 quad[6] = vec2[](vec2(0,0), vec2(1,0), vec2(1,1), vec2(0,0), vec2(1,1), vec2(0,1));\n"
    "void main() {\n"
    "    int face = gl_VertexID / 6;\n"
    "    vec2 q = quad[gl_VertexID % 6];\n"
    "    vec3 n = normals[face];\n"
    "    vec3 t = abs(n.y) > 0.5 ? vec3(1,0,0) : vec3(0,1,0);\n"
    "    vec3 b = cross(n, t);\n"
    "    vec3 corner = 0.5 * n + (q.x - 0.5) * t + (q.y - 0.5) * b;\n"
    "    int column = gl_InstanceID / layers;\n"
    "    int layer = gl_InstanceID % layers;\n"
    "    vec2 cell = vec2(column % size, column / size) - float(size) * 0.5;\n"
    "    float height = floor(3.0 * sin(cell.x * 0.15 + time * 0.3) * cos(cell.y * 0.12));\n"
    "    vec3 world = vec3(cell.x, height - float(layer), cell.y) + corner;\n"
    "    float angle = time * 0.1;\n"
    "    mat3 spin = mat3(cos(angle), 0, sin(angle), 0, 1, 0, -sin(angle), 0, cos(angle));\n"
    "    vec3 view = spin * world - vec3(0.0, 6.0, float(size) * 0.6);\n"
    "    float c = cos(0.5), s = -sin(0.5);\n"
    "    view = vec3(view.x, c * view.y + s * view.z, -s * view.y + c * view.z);\n"
    "    float near = 0.5, far = float(size) * 2.0;\n"
    "    gl_Position = vec4(view.x * aspect.x, view.y * aspect.y,\n"
    "                       (-view.z * (far + near) - 2.0 * far * near) / (far - near), -view.z);\n"
    "    uv = q;\n"
    "    light = 0.35 + 0.65 * max(dot(spin * n, normalize(vec3(0.4, 0.8, 0.3))), 0.0);\n"
    "    shade = layer == 0 && face == 2 ? 0.0 : 1.0;\n"
    "}\n";

static const char* fragment_shader =
    "#version 330 core\n"
    "uniform sampler2D tex;\n"
    "in vec2 uv;\n"
    "in float light;\n"
    "in float shade;\n"
    "out vec4 color;\n"
    "void main() {\n"
    "    vec3 grass = vec3(0.20, 0.36, 0.16);\n"
    "    vec3 dirt = vec3(0.33, 0.24, 0.16);\n"
    "    vec3 base = mix(grass, dirt, shade) * texture(tex, uv).r;\n"
    "    color = vec4(base * light, 1.0);\n"
    "}\n";

static int create_target(int width, int height, GLuint* fbo, GLuint* buffers) {
    p_glGenFramebuffers(1, fbo);
    p_glBindFramebuffer(GL_FRAMEBUFFER, *fbo);
    p_glGenRenderbuffers(2, buffers);
    p_glBindRenderbuffer(GL_RENDERBUFFER, buffers[0]);
    p_glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);
    p_glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, buffers[0]);
    p_glBindRenderbuffer(GL_RENDERBUFFER, buffers[1]);
    p_glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    p_glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, buffers[1]);
    return p_glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

int main(int argc, char** argv) {
    int screen_w = argc > 1 ? atoi(argv[1]) : 1600;
    int screen_h = argc > 2 ? atoi(argv[2]) : 720;
    double step_ms = (argc > 3 ? atof(argv[3]) : 3.0) * 1000.0;
    if (screen_w < screen_h) { int t = screen_w; screen_w = screen_h; screen_h = t; }
    const char* path = getenv("HYPOCKET_FRAME_PATH");
    /* HyPocket's direct display: show frames with eglSwapBuffers on a pbuffer
     * of the frame's size instead of reading them back. */
    int present = getenv("HYPOCKET_PRESENT") != NULL;
    if (present) path = NULL;

    uint32_t* header = NULL;
    unsigned char* pixels = NULL;
    if (path) {
        size_t size = HEADER + (size_t)screen_w * screen_h * 4;
        int fd = open(path, O_RDWR | O_CREAT, 0600);
        if (fd < 0 || ftruncate(fd, (off_t)size) != 0) return 1;
        unsigned char* shared = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (shared == MAP_FAILED) return 1;
        header = (uint32_t*)shared;
        pixels = shared + HEADER;
        header[1] = 0;
        header[2] = 0;
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
    const EGLint surface_attribs[] = {EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE};
    EGLSurface surface = eglCreatePbufferSurface(display, config, surface_attribs);
    const EGLint context_attribs[] = {EGL_CONTEXT_MAJOR_VERSION, 4, EGL_CONTEXT_MINOR_VERSION, 1,
                                      EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_NONE};
    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attribs);
    if (count == 0 || context == EGL_NO_CONTEXT || !eglMakeCurrent(display, surface, surface, context)) {
        printf("gl-perf-probe: no OpenGL 4.1 core context (0x%x)\n", eglGetError());
        return 1;
    }
#define LOAD(type, name) \
    p_##name = (type)eglGetProcAddress(#name); \
    if (!p_##name) { printf("gl-perf-probe: missing %s\n", #name); return 1; }
    GL_FUNCTIONS(LOAD)
    printf("gl-perf-probe: GL_RENDERER %s\n", p_glGetString(GL_RENDERER));
    printf("gl-perf-probe: screen %dx%d\n", screen_w, screen_h);

    GLuint program = p_glCreateProgram();
    p_glAttachShader(program, compile(GL_VERTEX_SHADER, vertex_shader));
    p_glAttachShader(program, compile(GL_FRAGMENT_SHADER, fragment_shader));
    p_glLinkProgram(program);
    GLint linked = 0;
    p_glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        printf("gl-perf-probe: link failed\n");
        return 1;
    }
    p_glUseProgram(program);

    /* A small noisy texture, like a block texture, with mipmaps. */
    static unsigned char texels[TEX * TEX * 4];
    uint32_t seed = 12345;
    for (int i = 0; i < TEX * TEX; i++) {
        seed = seed * 1103515245u + 12345u;
        unsigned char v = (unsigned char)(170 + (seed >> 24) % 86);
        int border = (i % TEX) < 2 || (i / TEX) < 2;
        texels[i * 4] = texels[i * 4 + 1] = texels[i * 4 + 2] = border ? 120 : v;
        texels[i * 4 + 3] = 255;
    }
    GLuint texture;
    p_glGenTextures(1, &texture);
    p_glBindTexture(GL_TEXTURE_2D, texture);
    p_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, TEX, TEX, 0, GL_RGBA, GL_UNSIGNED_BYTE, texels);
    p_glGenerateMipmap(GL_TEXTURE_2D);
    p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    p_glUniform1i(p_glGetUniformLocation(program, "tex"), 0);

    GLint u_time = p_glGetUniformLocation(program, "time");
    GLint u_size = p_glGetUniformLocation(program, "size");
    GLint u_layers = p_glGetUniformLocation(program, "layers");
    GLint u_aspect = p_glGetUniformLocation(program, "aspect");
    GLuint vao;
    p_glGenVertexArrays(1, &vao);
    p_glBindVertexArray(vao);
    p_glEnable(GL_DEPTH_TEST);
    p_glEnable(GL_CULL_FACE);

    static const int scales[] = {50, 75, 100};
    static const struct { const char* name; int size; int layers; } loads[] = {
        {"light", 32, 2}, {"medium", 64, 2}, {"heavy", 96, 3},
    };
    double start = now_ms();
    uint32_t frame_counter = 0;
    for (size_t si = 0; si < sizeof scales / sizeof scales[0]; si++) {
        int w = screen_w * scales[si] / 100, h = screen_h * scales[si] / 100;
        GLuint fbo, buffers[2];
        if (!create_target(w, h, &fbo, buffers)) {
            printf("gl-perf-probe: framebuffer %dx%d incomplete\n", w, h);
            return 1;
        }
        EGLSurface frame_surface = EGL_NO_SURFACE;
        if (present) {
            const EGLint attribs[] = {EGL_WIDTH, w, EGL_HEIGHT, h, EGL_NONE};
            frame_surface = eglCreatePbufferSurface(display, config, attribs);
            if (frame_surface == EGL_NO_SURFACE || !eglMakeCurrent(display, frame_surface, frame_surface, context)) {
                printf("gl-perf-probe: no %dx%d pbuffer (0x%x)\n", w, h, eglGetError());
                return 1;
            }
            p_glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        }
        p_glViewport(0, 0, w, h);
        float fy = 1.0f / 0.6f; /* about 62 degrees vertical field of view */
        p_glUniform2f(u_aspect, fy * h / w, fy);
        for (size_t li = 0; li < sizeof loads / sizeof loads[0]; li++) {
            int cubes = loads[li].size * loads[li].size * loads[li].layers;
            p_glUniform1i(u_size, loads[li].size);
            p_glUniform1i(u_layers, loads[li].layers);
            double step_start = now_ms(), gpu_total = 0, read_total = 0;
            int frames = 0;
            while (now_ms() - step_start < step_ms) {
                double t0 = now_ms();
                p_glUniform1f(u_time, (float)((t0 - start) / 1000.0));
                p_glClearColor(0.42f, 0.55f, 0.66f, 1.0f);
                p_glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                p_glDrawArraysInstanced(GL_TRIANGLES, 0, 36, cubes);
                p_glFinish();
                double t1 = now_ms();
                gpu_total += t1 - t0;
                if (present) {
                    p_glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
                    p_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
                    p_glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
                    eglSwapBuffers(display, frame_surface);
                    p_glBindFramebuffer(GL_FRAMEBUFFER, fbo);
                    read_total += now_ms() - t1;
                } else if (header) {
                    p_glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
                    header[1] = (uint32_t)w;
                    header[2] = (uint32_t)h;
                    __atomic_store_n(&header[3], ++frame_counter, __ATOMIC_RELEASE);
                    read_total += now_ms() - t1;
                }
                frames++;
            }
            double ms = gpu_total / frames;
            printf("perf: %d%% %dx%d %s (%d cubes, %d triangles): %.1f ms/frame, %.1f fps, copy %.1f ms\n",
                   scales[si], w, h, loads[li].name, cubes, cubes * 12, ms, 1000.0 / ms, read_total / frames);
            fflush(stdout);
        }
        p_glBindFramebuffer(GL_FRAMEBUFFER, 0);
        p_glDeleteFramebuffers(1, &fbo);
        p_glDeleteRenderbuffers(2, buffers);
        if (frame_surface != EGL_NO_SURFACE) {
            eglMakeCurrent(display, surface, surface, context);
            eglDestroySurface(display, frame_surface);
        }
    }
    if (header) __atomic_store_n(&header[4], 0, __ATOMIC_RELEASE);
    printf("gl-perf-probe: GL error 0x%x\n", p_glGetError());
    printf("gl-perf-probe: ok\n");
    return 0;
}
