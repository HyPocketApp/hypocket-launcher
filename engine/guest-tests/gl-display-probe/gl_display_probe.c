/* x86-64 Linux test program for showing frames on the phone's screen. It
 * draws an animated scene with a desktop OpenGL core context, reads each
 * frame back and writes it into a shared file that the app maps and shows.
 *
 * This read-back is a temporary display path: it works everywhere but costs a
 * copy per frame. The shared file layout (little endian):
 *   0  uint32 magic 'HPFB'
 *   4  uint32 width
 *   8  uint32 height
 *   12 uint32 frame counter, written after the pixels of each frame
 *   16 uint32 1 while running, 0 when finished
 *   64 width * height RGBA pixels, bottom row first (as glReadPixels gives)
 */
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

#define WIDTH 640
#define HEIGHT 360
#define HEADER 64
#define GRID 24

#define GL_FUNCTIONS(X) \
    X(PFNGLGETSTRINGPROC, glGetString) \
    X(PFNGLVIEWPORTPROC, glViewport) \
    X(PFNGLCLEARCOLORPROC, glClearColor) \
    X(PFNGLCLEARPROC, glClear) \
    X(PFNGLREADPIXELSPROC, glReadPixels) \
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
    X(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays) \
    X(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray) \
    X(PFNGLDRAWARRAYSINSTANCEDPROC, glDrawArraysInstanced) \
    X(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers) \
    X(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer) \
    X(PFNGLGENRENDERBUFFERSPROC, glGenRenderbuffers) \
    X(PFNGLBINDRENDERBUFFERPROC, glBindRenderbuffer) \
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
        char log[512];
        p_glGetShaderInfoLog(shader, sizeof log, NULL, log);
        printf("gl-display-probe: shader error: %s\n", log);
    }
    return shader;
}

/* A grid of spinning, colored quads, drawn with one instanced call and no
 * vertex buffers: positions come from gl_VertexID and gl_InstanceID. */
static const char* vertex_shader =
    "#version 330 core\n"
    "uniform float time;\n"
    "out vec3 color;\n"
    "void main() {\n"
    "    const vec2 corners[6] = vec2[](vec2(-1,-1), vec2(1,-1), vec2(1,1), vec2(-1,-1), vec2(1,1), vec2(-1,1));\n"
    "    int grid = " "24" ";\n"
    "    vec2 cell = vec2(gl_InstanceID % grid, gl_InstanceID / grid);\n"
    "    vec2 centre = (cell + 0.5) / float(grid) * 2.0 - 1.0;\n"
    "    float angle = time * 2.0 + (cell.x + cell.y) * 0.3;\n"
    "    mat2 spin = mat2(cos(angle), sin(angle), -sin(angle), cos(angle));\n"
    "    float size = 0.6 / float(grid) * (1.2 + 0.4 * sin(time * 3.0 + cell.x));\n"
    "    vec2 pos = centre + spin * corners[gl_VertexID] * size;\n"
    "    gl_Position = vec4(pos.x * 0.5625, pos.y, 0.0, 1.0);\n"
    "    color = 0.25 + 0.2 * cos(time * 0.5 + cell.xyx * 0.15 + vec3(0, 2, 4));\n"
    "}\n";

static const char* fragment_shader =
    "#version 330 core\n"
    "in vec3 color;\n"
    "out vec4 frag;\n"
    "void main() { frag = vec4(color, 1.0); }\n";

int main(int argc, char** argv) {
    const char* path = getenv("HYPOCKET_FRAME_PATH");
    int seconds = argc > 1 ? atoi(argv[1]) : 15;
    if (!path) {
        printf("gl-display-probe: HYPOCKET_FRAME_PATH is not set\n");
        return 1;
    }
    size_t size = HEADER + (size_t)WIDTH * HEIGHT * 4;
    int fd = open(path, O_RDWR | O_CREAT, 0600);
    if (fd < 0 || ftruncate(fd, (off_t)size) != 0) {
        printf("gl-display-probe: cannot create %s\n", path);
        return 1;
    }
    unsigned char* shared = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shared == MAP_FAILED) {
        printf("gl-display-probe: mmap failed\n");
        return 1;
    }
    uint32_t* header = (uint32_t*)shared;
    header[0] = 0x42465048; /* 'HPFB' */
    header[1] = WIDTH;
    header[2] = HEIGHT;
    header[3] = 0;
    header[4] = 1;

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (!eglInitialize(display, NULL, NULL) || !eglBindAPI(EGL_OPENGL_API)) {
        printf("gl-display-probe: EGL setup failed (0x%x)\n", eglGetError());
        return 1;
    }
    const EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE,
    };
    EGLConfig config;
    EGLint count = 0;
    eglChooseConfig(display, config_attribs, &config, 1, &count);
    const EGLint surface_attribs[] = {EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE};
    EGLSurface surface = eglCreatePbufferSurface(display, config, surface_attribs);
    const EGLint context_attribs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 4, EGL_CONTEXT_MINOR_VERSION, 1,
        EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_NONE,
    };
    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attribs);
    if (count == 0 || context == EGL_NO_CONTEXT || !eglMakeCurrent(display, surface, surface, context)) {
        printf("gl-display-probe: no OpenGL 4.1 core context (0x%x)\n", eglGetError());
        return 1;
    }
#define LOAD(type, name) \
    p_##name = (type)eglGetProcAddress(#name); \
    if (!p_##name) { printf("gl-display-probe: missing %s\n", #name); return 1; }
    GL_FUNCTIONS(LOAD)
    printf("gl-display-probe: GL_RENDERER %s\n", p_glGetString(GL_RENDERER));
    printf("gl-display-probe: GL_VERSION %s\n", p_glGetString(GL_VERSION));

    GLuint fbo, color;
    p_glGenFramebuffers(1, &fbo);
    p_glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    p_glGenRenderbuffers(1, &color);
    p_glBindRenderbuffer(GL_RENDERBUFFER, color);
    p_glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, WIDTH, HEIGHT);
    p_glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color);
    if (p_glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        printf("gl-display-probe: framebuffer incomplete\n");
        return 1;
    }

    GLuint program = p_glCreateProgram();
    p_glAttachShader(program, compile(GL_VERTEX_SHADER, vertex_shader));
    p_glAttachShader(program, compile(GL_FRAGMENT_SHADER, fragment_shader));
    p_glLinkProgram(program);
    GLint linked = 0;
    p_glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        printf("gl-display-probe: link failed\n");
        return 1;
    }
    p_glUseProgram(program);
    GLint time_location = p_glGetUniformLocation(program, "time");
    GLuint vao;
    p_glGenVertexArrays(1, &vao);
    p_glBindVertexArray(vao);
    p_glViewport(0, 0, WIDTH, HEIGHT);

    double start = now_ms(), draw_total = 0, read_total = 0;
    uint32_t frames = 0;
    while (now_ms() - start < seconds * 1000.0) {
        double t0 = now_ms();
        p_glUniform1f(time_location, (float)((t0 - start) / 1000.0));
        p_glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
        p_glClear(GL_COLOR_BUFFER_BIT);
        p_glDrawArraysInstanced(GL_TRIANGLES, 0, 6, GRID * GRID);
        double t1 = now_ms();
        p_glReadPixels(0, 0, WIDTH, HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, shared + HEADER);
        double t2 = now_ms();
        draw_total += t1 - t0;
        read_total += t2 - t1;
        frames++;
        __atomic_store_n(&header[3], frames, __ATOMIC_RELEASE);
    }
    double elapsed = now_ms() - start;
    __atomic_store_n(&header[4], 0, __ATOMIC_RELEASE);

    printf("gl-display-probe: %u frames in %.0f ms, %.1f fps\n", frames, elapsed, frames * 1000.0 / elapsed);
    printf("gl-display-probe: per frame: draw %.1f ms, read back %.1f ms\n",
           draw_total / frames, read_total / frames);
    printf("gl-display-probe: ok\n");
    munmap(shared, size);
    close(fd);
    return 0;
}
