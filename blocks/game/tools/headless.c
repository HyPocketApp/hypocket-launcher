/* HyPocket Blocks: runs the game through the native platform interface
 * (platform.h) with no window, for testing on a Linux machine without a
 * screen: an EGL pbuffer stands in for the window. With HEADLESS_GLES=1 the
 * context is OpenGL ES 3.0, as on the phone; else desktop OpenGL 3.3 core.
 * HEADLESS_SHOT=<file.ppm> keeps the last frame. HEADLESS_SCREENS=1 has the
 * game draw its own screens, as on PC; HEADLESS_KEYS="<frame>:<key>,..."
 * presses BLOCKS_KEY_* numbers on those frames, HEADLESS_POINTER="x,y"
 * places the pointer.
 *
 * Build: see build.sh (headless). Usage: blocks-headless <w> <h> <fps> <frames> */
#include <EGL/egl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../third_party/GL/glcorearb.h"
#include "../platform.h"

static EGLDisplay display;
static EGLSurface surface;
static int width, height;

static PlatformInput input;
static int frame_number;

static void frame(void* io, int* w, int* h) {
    (void)io;
    input.key_count = input.clicks = input.wheel = 0;
    const char* keys = getenv("HEADLESS_KEYS");
    while (keys && *keys) {
        int at = 0, key = 0;
        if (sscanf(keys, "%d:%d", &at, &key) == 2 && at == frame_number) input.keys[input.key_count++] = key;
        keys = strchr(keys, ',');
        if (keys) keys++;
    }
    /* HEADLESS_TOUCH="<frame>:<down|move|up>:<id>:<x>:<y>,...": fingers, in pixels. */
    const char* touches = getenv("HEADLESS_TOUCH");
    int n = 0;
    for (int i = 0; i < input.touch_count; i++)
        if (!input.touches[i].released) { input.touches[n] = input.touches[i]; input.touches[n++].pressed = 0; }
    input.touch_count = n;
    while (touches && *touches) {
        int at, id;
        char kind[8];
        float x, y;
        if (sscanf(touches, "%d:%7[a-z]:%d:%f:%f", &at, kind, &id, &x, &y) == 5 && at == frame_number) {
            int t = -1;
            for (int i = 0; i < input.touch_count; i++) if (input.touches[i].id == id) t = i;
            if (!strcmp(kind, "down") && t < 0 && input.touch_count < 8) {
                t = input.touch_count++;
                input.touches[t].id = id;
                input.touches[t].pressed = 1;
                input.touches[t].released = 0;
            }
            if (t >= 0) {
                input.touches[t].x = x;
                input.touches[t].y = y;
                if (!strcmp(kind, "up")) input.touches[t].released = 1;
            }
        }
        touches = strchr(touches, ',');
        if (touches) touches++;
    }
    input.touch = getenv("HEADLESS_TOUCH") != NULL;
    const char* pointer = getenv("HEADLESS_POINTER");
    input.pointer_x = input.pointer_y = -1;
    if (pointer) sscanf(pointer, "%d,%d", &input.pointer_x, &input.pointer_y);
    frame_number++;
    *w = width;
    *h = height;
}

static void* get_proc(const char* name) { return (void*)eglGetProcAddress(name); }

static void present(void) {
    const char* shot = getenv("HEADLESS_SHOT");
    if (shot) {
        PFNGLREADPIXELSPROC read = (PFNGLREADPIXELSPROC)eglGetProcAddress("glReadPixels");
        unsigned char* px = malloc((size_t)width * height * 4);
        read(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, px);
        FILE* f = fopen(shot, "wb");
        if (f) {
            fprintf(f, "P6\n%d %d\n255\n", width, height);
            for (int y = height - 1; y >= 0; y--)
                for (int x = 0; x < width; x++) fwrite(px + ((size_t)y * width + x) * 4, 1, 3, f);
            fclose(f);
        }
        free(px);
    }
    eglSwapBuffers(display, surface);
}

int main(int argc, char** argv) {
    if (argc < 5) {
        fprintf(stderr, "usage: %s <width> <height> <fps> <frames>\n", argv[0]);
        return 2;
    }
    width = atoi(argv[1]);
    height = atoi(argv[2]);
    int gles = getenv("HEADLESS_GLES") != NULL;
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (!eglInitialize(display, NULL, NULL) || !eglBindAPI(gles ? EGL_OPENGL_ES_API : EGL_OPENGL_API)) return 1;
    const EGLint attribs[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE,
                              gles ? EGL_OPENGL_ES3_BIT : EGL_OPENGL_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8,
                              EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE};
    EGLConfig config;
    EGLint count = 0;
    if (!eglChooseConfig(display, attribs, &config, 1, &count) || !count) return 1;
    const EGLint size[] = {EGL_WIDTH, width, EGL_HEIGHT, height, EGL_NONE};
    surface = eglCreatePbufferSurface(display, config, size);
    const EGLint es_ctx[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 0, EGL_NONE};
    const EGLint gl_ctx[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 3,
                             EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_NONE};
    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, gles ? es_ctx : gl_ctx);
    if (context == EGL_NO_CONTEXT || !eglMakeCurrent(display, surface, surface, context)) {
        fprintf(stderr, "headless: no context (0x%x)\n", eglGetError());
        return 1;
    }
    static const BlocksPlatform platform = {.get_proc = get_proc, .present = present};
    BlocksPlatform* p = malloc(sizeof *p);
    memcpy(p, &platform, sizeof *p);
    p->gles = gles;
    if (getenv("HEADLESS_SCREENS")) {
        p->game_screens = 1;
        p->input = &input;
        p->frame = frame;
    }
    blocks_platform = p;
    return blocks_main(argc, argv);
}
